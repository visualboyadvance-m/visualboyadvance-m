#if defined(_WIN32)

#include "wx/rpi-proxy/RpiProxyClient.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/msw/private.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <Shlwapi.h>

#include "wx/rpi-proxy/rpi-host-rc.h"

namespace rpi_proxy {

// Static counter for generating unique instance IDs
uint32_t RpiProxyClient::s_next_instance_id_ = 0;

// Shared singleton instance
RpiProxyClient* RpiProxyClient::s_shared_instance_ = nullptr;

RpiProxyClient* RpiProxyClient::GetSharedInstance() {
    if (!s_shared_instance_) {
        s_shared_instance_ = new RpiProxyClient();
    }
    return s_shared_instance_;
}

void RpiProxyClient::ReleaseSharedInstance() {
    if (s_shared_instance_) {
        delete s_shared_instance_;
        s_shared_instance_ = nullptr;
    }
}

bool RpiProxyClient::IsActivelyRendering() const {
    // Check if any filter threads are active
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        if (thread_ipc_[i].active) {
            return true;
        }
    }
    return false;
}

uint32_t RpiProxyClient::GetActiveThreadCount() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        if (thread_ipc_[i].active) {
            count++;
        }
    }
    return count;
}

void RpiProxyClient::RestartThreads(uint32_t count) {
    for (uint32_t i = 0; i < count && i < kMaxFilterThreads; i++) {
        if (!thread_ipc_[i].active) {
            StartThread(i);
        }
    }
}

uint32_t RpiProxyClient::SaveThreadActiveStates() const {
    uint32_t states = 0;
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        if (thread_ipc_[i].active) {
            states |= (1u << i);
        }
    }
    return states;
}

void RpiProxyClient::RestoreThreadActiveStates(uint32_t states) {
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        bool should_be_active = (states & (1u << i)) != 0;
        if (should_be_active && !thread_ipc_[i].active) {
            // Thread was active before but is now inactive - restore it
            thread_ipc_[i].active = true;
        }
    }
    MemoryBarrier();  // Ensure changes are visible to other threads
}

RpiProxyClient::RpiProxyClient() {
    memset(&plugin_info_, 0, sizeof(plugin_info_));
    InitializeCriticalSection(&main_channel_cs_);
    main_channel_cs_initialized_ = true;

    // Initialize per-thread critical sections
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        InitializeCriticalSection(&thread_ipc_[i].cs);
        thread_ipc_[i].cs_initialized = true;
    }

    client_pid_ = GetCurrentProcessId();
    instance_id_ = InterlockedIncrement(&s_next_instance_id_);
}

RpiProxyClient::~RpiProxyClient() {
    // Force terminate the host process immediately - no graceful shutdown
    // This avoids timeouts on app exit
    ForceTerminateHost();

    CleanupIPC();

    // Clean up the extracted exe
    if (!host_exe_path_.empty() && wxFileExists(host_exe_path_)) {
        wxRemoveFile(host_exe_path_);
    }

    // Clean up the critical sections
    if (main_channel_cs_initialized_) {
        DeleteCriticalSection(&main_channel_cs_);
        main_channel_cs_initialized_ = false;
    }

    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        if (thread_ipc_[i].cs_initialized) {
            DeleteCriticalSection(&thread_ipc_[i].cs);
            thread_ipc_[i].cs_initialized = false;
        }
    }
}

bool RpiProxyClient::Is32BitDll(const wxString& path) {
    // Open the file and read the PE header to determine architecture
    HANDLE hFile = CreateFileW(path.wc_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Read DOS header
    IMAGE_DOS_HEADER dosHeader;
    DWORD bytesRead;
    if (!ReadFile(hFile, &dosHeader, sizeof(dosHeader), &bytesRead, nullptr) ||
        bytesRead != sizeof(dosHeader) ||
        dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        CloseHandle(hFile);
        return false;
    }

    // Seek to PE header
    if (SetFilePointer(hFile, dosHeader.e_lfanew, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        CloseHandle(hFile);
        return false;
    }

    // Read PE signature and file header
    DWORD peSignature;
    IMAGE_FILE_HEADER fileHeader;

    if (!ReadFile(hFile, &peSignature, sizeof(peSignature), &bytesRead, nullptr) ||
        bytesRead != sizeof(peSignature) ||
        peSignature != IMAGE_NT_SIGNATURE) {
        CloseHandle(hFile);
        return false;
    }

    if (!ReadFile(hFile, &fileHeader, sizeof(fileHeader), &bytesRead, nullptr) ||
        bytesRead != sizeof(fileHeader)) {
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);

    // Check machine type
    return fileHeader.Machine == IMAGE_FILE_MACHINE_I386;
}

bool RpiProxyClient::IsProcess64Bit() {
#if defined(_WIN64)
    return true;
#else
    // Check if we're running under WoW64
    BOOL isWow64 = FALSE;
    typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
    LPFN_ISWOW64PROCESS fnIsWow64Process = reinterpret_cast<LPFN_ISWOW64PROCESS>(
        GetProcAddress(GetModuleHandleW(L"kernel32"), "IsWow64Process"));

    if (fnIsWow64Process) {
        fnIsWow64Process(GetCurrentProcess(), &isWow64);
    }

    // If running under WoW64, we're a 32-bit process on 64-bit Windows
    // If not, we're native 32-bit
    return false;
#endif
}

bool RpiProxyClient::NeedsProxy(const wxString& pluginPath) {
    // Only need proxy if we're 64-bit and the plugin is 32-bit
    return IsProcess64Bit() && Is32BitDll(pluginPath);
}

bool RpiProxyClient::ExtractHostExe() {
    if (!host_exe_path_.empty()) {
        // Already extracted
        return wxFileExists(host_exe_path_);
    }

    // Find the embedded resource
    HINSTANCE hInstance = wxGetInstance();

    HRSRC res = FindResourceW(hInstance,
        MAKEINTRESOURCEW(RPI_HOST_EXE_RC), RT_RCDATA);

    if (!res) {
        DWORD err = GetLastError();
        wxLogError("RpiProxyClient: Could not find embedded host exe resource (error %lu)", err);
        return false;
    }

    HGLOBAL resHandle = LoadResource(hInstance, res);
    if (!resHandle) {
        DWORD err = GetLastError();
        wxLogError("RpiProxyClient: Could not load host exe resource (error %lu)", err);
        return false;
    }

    DWORD resSize = SizeofResource(hInstance, res);
    void* resData = LockResource(resHandle);

    if (!resData || resSize == 0) {
        wxLogError("RpiProxyClient: Could not access host exe resource data");
        return false;
    }

    // Create temp file with a recognizable name
    // Use format: vbam-rpi-host-<pid>-<instance>.exe
    wxString tempDir = wxFileName::GetTempDir();
    wxString exeName = wxString::Format("vbam-rpi-host-%u-%u.exe", client_pid_, instance_id_);
    host_exe_path_ = wxFileName(tempDir, exeName).GetFullPath();

    // Remove any existing file with this name
    if (wxFileExists(host_exe_path_)) {
        wxRemoveFile(host_exe_path_);
    }

    // Write the resource data to the file
    wxFile tempFile;
    if (!tempFile.Create(host_exe_path_, true)) {
        wxLogError("RpiProxyClient: Could not create temp file for host exe: %s", host_exe_path_);
        host_exe_path_.clear();
        return false;
    }

    if (!tempFile.Write(resData, resSize)) {
        wxLogError("RpiProxyClient: Could not write host exe to temp file");
        tempFile.Close();
        wxRemoveFile(host_exe_path_);
        host_exe_path_.clear();
        return false;
    }

    tempFile.Close();
    return true;
}

bool RpiProxyClient::InitializeIPC() {
    // Generate unique IPC names using client PID and instance ID
    wchar_t memName[128], reqName[128], respName[128];
    GetSharedMemoryName(client_pid_, instance_id_, memName, 128);
    GetRequestEventName(client_pid_, instance_id_, reqName, 128);
    GetResponseEventName(client_pid_, instance_id_, respName, 128);

    // Create shared memory
    shared_mem_handle_ = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        kSharedMemorySize,
        memName);

    if (!shared_mem_handle_) {
        wxLogError("RpiProxyClient: Failed to create shared memory: %lu", GetLastError());
        return false;
    }

    shared_mem_ = static_cast<SharedFilterBuffer*>(
        MapViewOfFile(shared_mem_handle_, FILE_MAP_ALL_ACCESS, 0, 0, kSharedMemorySize));

    if (!shared_mem_) {
        wxLogError("RpiProxyClient: Failed to map shared memory: %lu", GetLastError());
        CloseHandle(shared_mem_handle_);
        shared_mem_handle_ = nullptr;
        return false;
    }

    // Initialize the shared memory structure
    shared_mem_->Initialize();

    // Create synchronization events
    request_event_ = CreateEventW(nullptr, FALSE, FALSE, reqName);
    if (!request_event_) {
        wxLogError("RpiProxyClient: Failed to create request event: %lu", GetLastError());
        return false;
    }

    response_event_ = CreateEventW(nullptr, FALSE, FALSE, respName);
    if (!response_event_) {
        wxLogError("RpiProxyClient: Failed to create response event: %lu", GetLastError());
        return false;
    }

    // Create ready event (manual-reset, initially unsignaled)
    // The host signals this after it has initialized, replacing the Sleep() polling loop
    wchar_t readyName[128];
    GetReadyEventName(client_pid_, instance_id_, readyName, 128);
    ready_event_ = CreateEventW(nullptr, TRUE, FALSE, readyName);
    if (!ready_event_) {
        wxLogError("RpiProxyClient: Failed to create ready event: %lu", GetLastError());
        return false;
    }

    return true;
}

void RpiProxyClient::CleanupIPC() {
    // Clean up all thread IPC channels first
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        CleanupThreadIPC(i);
    }

    if (shared_mem_) {
        UnmapViewOfFile(shared_mem_);
        shared_mem_ = nullptr;
    }
    if (shared_mem_handle_) {
        CloseHandle(shared_mem_handle_);
        shared_mem_handle_ = nullptr;
    }
    if (request_event_) {
        CloseHandle(request_event_);
        request_event_ = nullptr;
    }
    if (response_event_) {
        CloseHandle(response_event_);
        response_event_ = nullptr;
    }
    if (ready_event_) {
        CloseHandle(ready_event_);
        ready_event_ = nullptr;
    }
}

bool RpiProxyClient::InitializeThreadIPC(uint32_t thread_id) {
    if (thread_id >= kMaxFilterThreads) {
        return false;
    }

    ThreadIPC& ipc = thread_ipc_[thread_id];

    // Clean up any existing resources first to avoid leaks and stale state
    if (ipc.shared_mem || ipc.shared_mem_handle || ipc.request_event || ipc.response_event) {
        CleanupThreadIPC(thread_id);
    }

    // Reset sequence counter for fresh start
    ipc.seq_counter = 0;

    // Generate unique names for this thread (includes client PID and instance ID for uniqueness)
    wchar_t memName[128], reqName[128], respName[128];
    GetThreadSharedMemoryName(client_pid_, instance_id_, thread_id, memName, 128);
    GetThreadRequestEventName(client_pid_, instance_id_, thread_id, reqName, 128);
    GetThreadResponseEventName(client_pid_, instance_id_, thread_id, respName, 128);

    // Create shared memory for this thread
    ipc.shared_mem_handle = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        kSharedMemorySize,
        memName);

    if (!ipc.shared_mem_handle) {
        return false;
    }

    // Check if we created new memory or opened existing
    bool is_new_memory = (GetLastError() != ERROR_ALREADY_EXISTS);

    ipc.shared_mem = static_cast<SharedFilterBuffer*>(
        MapViewOfFile(ipc.shared_mem_handle, FILE_MAP_ALL_ACCESS, 0, 0, kSharedMemorySize));

    if (!ipc.shared_mem) {
        CloseHandle(ipc.shared_mem_handle);
        ipc.shared_mem_handle = nullptr;
        return false;
    }

    // Only initialize if this is truly new memory.
    // If we're reusing existing memory (e.g., after enumeration), don't reset it
    // as a panel filter thread might be in the middle of reading from it.
    if (is_new_memory) {
        ipc.shared_mem->Initialize();
    }

    // Create synchronization events
    ipc.request_event = CreateEventW(nullptr, FALSE, FALSE, reqName);
    if (!ipc.request_event) {
        UnmapViewOfFile(ipc.shared_mem);
        ipc.shared_mem = nullptr;
        CloseHandle(ipc.shared_mem_handle);
        ipc.shared_mem_handle = nullptr;
        return false;
    }

    ipc.response_event = CreateEventW(nullptr, FALSE, FALSE, respName);
    if (!ipc.response_event) {
        CloseHandle(ipc.request_event);
        ipc.request_event = nullptr;
        UnmapViewOfFile(ipc.shared_mem);
        ipc.shared_mem = nullptr;
        CloseHandle(ipc.shared_mem_handle);
        ipc.shared_mem_handle = nullptr;
        return false;
    }

    // Note: active is NOT set here - it will be set by StartThread after the host confirms
    // it has opened the shared memory. This prevents a race where ApplyFilter sees active=true
    // but the host hasn't opened the memory yet.
    return true;
}

void RpiProxyClient::CleanupThreadIPC(uint32_t thread_id) {
    if (thread_id >= kMaxFilterThreads) {
        return;
    }

    ThreadIPC& ipc = thread_ipc_[thread_id];

    // First set active=false so new ApplyFilter calls will skip
    ipc.active = false;
    MemoryBarrier();

    // Acquire the lock to ensure any in-progress ApplyFilter finishes
    // before we unmap the shared memory
    if (ipc.cs_initialized) {
        EnterCriticalSection(&ipc.cs);
    }

    if (ipc.shared_mem) {
        UnmapViewOfFile(ipc.shared_mem);
        ipc.shared_mem = nullptr;
    }
    if (ipc.shared_mem_handle) {
        CloseHandle(ipc.shared_mem_handle);
        ipc.shared_mem_handle = nullptr;
    }
    if (ipc.request_event) {
        CloseHandle(ipc.request_event);
        ipc.request_event = nullptr;
    }
    if (ipc.response_event) {
        CloseHandle(ipc.response_event);
        ipc.response_event = nullptr;
    }

    if (ipc.cs_initialized) {
        LeaveCriticalSection(&ipc.cs);
    }
}

bool RpiProxyClient::SendThreadCommand(uint32_t thread_id, FilterCommand cmd, uint32_t timeout_ms) {
    if (thread_id >= kMaxFilterThreads) {
        return false;
    }

    ThreadIPC& ipc = thread_ipc_[thread_id];

    if (!ipc.shared_mem || !ipc.active || !host_process_) {
        return false;
    }

    // Check if host is still running
    DWORD exitCode;
    if (!GetExitCodeProcess(host_process_, &exitCode) || exitCode != STILL_ACTIVE) {
        return false;
    }

    // Generate a unique sequence number for this request
    uint32_t seq = ++ipc.seq_counter;
    if (seq == 0) seq = ++ipc.seq_counter;  // Avoid 0 since it's the initial value

    // Set up the command with sequence number
    // Write command FIRST, then seq, so if host sees the new seq it will see the command
    ipc.shared_mem->command = cmd;
    MemoryBarrier();  // Ensure command is written before seq
    ipc.shared_mem->request_seq = seq;
    MemoryBarrier();  // Ensure both are visible before signaling
    SetEvent(ipc.request_event);

    // Wait for response, handling stale signals
    DWORD startTime = GetTickCount();
    DWORD remaining = timeout_ms;

    while (remaining > 0) {
        DWORD waitResult = WaitForSingleObject(ipc.response_event, remaining);

        if (waitResult == WAIT_TIMEOUT || waitResult != WAIT_OBJECT_0) {
            return false;
        }

        // Got a signal - check if it's for our request via sequence number
        MemoryBarrier();
        if (ipc.shared_mem->response_seq == seq) {
            // This is our response
            bool success = (ipc.shared_mem->result == FilterResult::Success);
            return success;
        }

        // Stale response - keep waiting
        DWORD elapsed = GetTickCount() - startTime;
        if (elapsed >= timeout_ms) {
            return false;
        }
        remaining = timeout_ms - elapsed;
    }

    return false;
}

bool RpiProxyClient::LaunchHost() {
    // Check if host is already running and reuse it
    if (host_process_) {
        DWORD exitCode = 0;
        BOOL result = GetExitCodeProcess(host_process_, &exitCode);
        if (result && exitCode == STILL_ACTIVE) {
            return true;  // Reuse existing host
        }
        // Host exited, clean up
        CloseHandle(host_process_);
        host_process_ = nullptr;
        CleanupIPC();
    }

    if (!ExtractHostExe()) {
        return false;
    }

    if (!InitializeIPC()) {
        return false;
    }

    // Launch the host process with client PID as argument
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    // Build command line: "path\to\host.exe <client_pid> <instance_id>"
    wchar_t cmdLine[MAX_PATH + 64];
    swprintf_s(cmdLine, MAX_PATH + 64, L"\"%s\" %u %u", host_exe_path_.wc_str(), client_pid_, instance_id_);

    if (!CreateProcessW(
            nullptr,  // Use command line for exe path
            cmdLine,
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        DWORD err = GetLastError();
        wxLogError("RpiProxyClient: Failed to launch host process: %lu", err);
        return false;
    }

    host_process_ = pi.hProcess;
    CloseHandle(pi.hThread);

    // Wait for the host to signal it's ready (via ready event)
    // Falls back to shared memory version check if event wait fails
    if (ready_event_) {
        DWORD waitResult = WaitForSingleObject(ready_event_, 5000);
        if (waitResult == WAIT_OBJECT_0 &&
            shared_mem_->version == SharedFilterBuffer::kCurrentVersion) {
            return true;
        }
    }

    // Fallback: brief poll in case event was missed but host did initialize
    for (int i = 0; i < 10; i++) {
        if (shared_mem_->version == SharedFilterBuffer::kCurrentVersion) {
            return true;
        }
        Sleep(50);
    }

    wxLogError("RpiProxyClient: Host process failed to initialize");
    TerminateProcess(host_process_, 1);
    CloseHandle(host_process_);
    host_process_ = nullptr;
    return false;
}

void RpiProxyClient::TerminateHost() {
    if (host_process_) {
        // Send shutdown command
        if (shared_mem_ && request_event_) {
            shared_mem_->command = FilterCommand::Shutdown;
            SetEvent(request_event_);
            if (response_event_) {
                WaitForSingleObject(response_event_, 1000);
            }
        }

        // Wait for graceful exit, then force terminate if needed
        if (WaitForSingleObject(host_process_, 2000) == WAIT_TIMEOUT) {
            TerminateProcess(host_process_, 0);
            // Wait for the process to fully terminate after force kill
            WaitForSingleObject(host_process_, 5000);
        }

        CloseHandle(host_process_);
        host_process_ = nullptr;
    }
}

void RpiProxyClient::ForceTerminateHost() {
    if (host_process_) {
        // Immediately terminate without any graceful shutdown
        TerminateProcess(host_process_, 0);

        // Brief wait for process to fully terminate so we can delete the exe
        WaitForSingleObject(host_process_, 1000);

        CloseHandle(host_process_);
        host_process_ = nullptr;
    }
    is_loaded_ = false;
}

bool RpiProxyClient::SendCommand(FilterCommand cmd, uint32_t timeout_ms) {
    if (!shared_mem_ || !host_process_) {
        return false;
    }

    // Check if host is still running
    DWORD exitCode;
    if (!GetExitCodeProcess(host_process_, &exitCode) || exitCode != STILL_ACTIVE) {
        wxLogError("RpiProxyClient: Host process is not running");
        return false;
    }

    // Protect main channel access - only one thread can send a command at a time
    EnterCriticalSection(&main_channel_cs_);

    bool success = false;

    // Retry loop - if a command times out (e.g., due to host not seeing the command),
    // retry once with a new sequence number
    for (int attempt = 0; attempt < 2 && !success; attempt++) {
        // Generate a unique sequence number for this request
        uint32_t seq = ++main_seq_counter_;
        if (seq == 0) seq = ++main_seq_counter_;  // Avoid 0 since it's the initial value

        // Set up the command with sequence number
        // Write command FIRST, then seq, so if host sees the new seq it will see the command
        shared_mem_->command = cmd;
        MemoryBarrier();  // Ensure command is written before seq
        shared_mem_->request_seq = seq;
        MemoryBarrier();  // Ensure both are visible before signaling
        SetEvent(request_event_);

        // Wait for response, handling stale signals from previous timed-out commands
        DWORD startTime = GetTickCount();
        DWORD remaining = timeout_ms;

        while (remaining > 0) {
            DWORD waitResult = WaitForSingleObject(response_event_, remaining);

            if (waitResult == WAIT_TIMEOUT) {
                if (attempt != 0) {
                    wxLogError("RpiProxyClient: Command timed out (waited %ums)", timeout_ms);
                }
                break;
            }

            if (waitResult != WAIT_OBJECT_0) {
                wxLogError("RpiProxyClient: Command wait failed (result=%lu)", waitResult);
                break;
            }

            // Got a signal - check if it's for our request via sequence number
            MemoryBarrier();  // Ensure we read the latest response_seq
            if (shared_mem_->response_seq == seq) {
                // This is our response
                success = (shared_mem_->result == FilterResult::Success);
                break;
            }

            // Stale response from a previous timed-out command - keep waiting
            DWORD elapsed = GetTickCount() - startTime;
            if (elapsed >= timeout_ms) {
                if (attempt != 0) {
                    wxLogError("RpiProxyClient: Command timed out after stale responses (waited %ums)", timeout_ms);
                }
                break;
            }
            remaining = timeout_ms - elapsed;
        }
    }

    LeaveCriticalSection(&main_channel_cs_);
    return success;
}

bool RpiProxyClient::LoadPlugin(const wxString& path, RENDER_PLUGIN_INFO* info) {
    // If already loaded, unload first
    if (is_loaded_) {
        UnloadPlugin();
    }

    if (!LaunchHost()) {
        return false;
    }

    // Lock BEFORE setting pluginPath and keep locked through SendCommand
    // (CRITICAL_SECTION is recursive, so SendCommand's lock is safe)
    // This prevents race conditions when multiple threads enumerate plugins concurrently
    EnterCriticalSection(&main_channel_cs_);

    // Reset result before sending command
    shared_mem_->result = FilterResult::Success;

    // Copy path to shared memory
    wcsncpy(shared_mem_->pluginPath, path.wc_str(), MAX_PATH - 1);
    shared_mem_->pluginPath[MAX_PATH - 1] = L'\0';

    bool success = SendCommand(FilterCommand::LoadPlugin);
    LeaveCriticalSection(&main_channel_cs_);

    if (!success) {
        wxLogError("RpiProxyClient: LoadPlugin failed for: %s", path);
        return false;
    }

    // Copy plugin info from shared memory
    is_loaded_ = true;
    ++load_generation_;
    strncpy(plugin_info_.Name, shared_mem_->pluginInfo.name, sizeof(plugin_info_.Name) - 1);
    plugin_info_.Name[sizeof(plugin_info_.Name) - 1] = '\0';
    plugin_info_.Flags = shared_mem_->pluginInfo.flags;
    plugin_info_.Handle = nullptr;  // Not meaningful in proxy context
    plugin_info_.Output = nullptr;  // Handled by ApplyFilter method

    if (info) {
        *info = plugin_info_;
    }

    return true;
}

bool RpiProxyClient::StartThread(uint32_t thread_id) {
    if (!is_loaded_ || !shared_mem_ || !host_process_) {
        return false;
    }

    if (thread_id >= kMaxFilterThreads) {
        return false;
    }

    // Initialize per-thread IPC channel (creates shared memory and events)
    if (!InitializeThreadIPC(thread_id)) {
        return false;
    }

    // Tell the host to start its thread (uses main channel)
    // Lock BEFORE setting filterParams and keep locked through SendCommand
    // (CRITICAL_SECTION is recursive, so SendCommand's lock is safe)
    EnterCriticalSection(&main_channel_cs_);
    shared_mem_->filterParams.thread_id = thread_id;
    bool success = SendCommand(FilterCommand::StartThread);
    LeaveCriticalSection(&main_channel_cs_);

    if (!success) {
        CleanupThreadIPC(thread_id);
        return false;
    }

    // Only set active=true AFTER the host has confirmed it opened the shared memory.
    // This prevents a race where ApplyFilter sees active=true but the host hasn't
    // opened the memory yet.
    thread_ipc_[thread_id].active = true;
    MemoryBarrier();  // Ensure active is visible to other threads

    return true;
}

void RpiProxyClient::StopThread(uint32_t thread_id) {
    if (!shared_mem_ || !host_process_) {
        return;
    }

    if (thread_id >= kMaxFilterThreads) {
        return;
    }

    // Tell the host to stop its thread (uses main channel)
    // Lock BEFORE setting filterParams and keep locked through SendCommand
    EnterCriticalSection(&main_channel_cs_);
    shared_mem_->filterParams.thread_id = thread_id;
    SendCommand(FilterCommand::StopThread);
    LeaveCriticalSection(&main_channel_cs_);

    // Clean up per-thread IPC channel
    CleanupThreadIPC(thread_id);
}

bool RpiProxyClient::ApplyFilter(const RENDER_PLUGIN_OUTP* params, uint32_t thread_id, uint32_t actualDstPitch) {
    if (!is_loaded_ || !host_process_) {
        return false;
    }

    if (thread_id >= kMaxFilterThreads) {
        return false;
    }

    ThreadIPC& ipc = thread_ipc_[thread_id];

    // Acquire the lock to prevent CleanupThreadIPC from unmapping memory while we use it
    if (!ipc.cs_initialized) {
        return false;
    }
    EnterCriticalSection(&ipc.cs);

    // Check if per-thread IPC channel is available (must check AFTER acquiring lock)
    if (!ipc.active) {
        // Thread channel not active - this happens during enumeration when threads are stopped.
        LeaveCriticalSection(&ipc.cs);
        return false;
    }

    SharedFilterBuffer* mem = ipc.shared_mem;
    if (!mem) {
        LeaveCriticalSection(&ipc.cs);
        return false;
    }

    // Copy parameters to the appropriate shared memory
    mem->filterParams.thread_id = thread_id;
    mem->filterParams.flags = params->Flags;
    mem->filterParams.srcPitch = params->SrcPitch;
    mem->filterParams.srcW = params->SrcW;
    mem->filterParams.srcH = params->SrcH;
    mem->filterParams.dstPitch = params->DstPitch;
    mem->filterParams.dstW = params->DstW;
    mem->filterParams.dstH = params->DstH;
    mem->filterParams.outW = params->OutW;
    mem->filterParams.outH = params->OutH;

    // Copy source pixel data to shared memory — unless the caller already
    // wrote straight into the shared src buffer (zero-copy fast path).
    size_t srcSize = static_cast<size_t>(params->SrcPitch) * params->SrcH;
    if (!params->SrcPtr || srcSize > kMaxPixelBufferSize) {
        LeaveCriticalSection(&ipc.cs);
        return false;
    }
    if (params->SrcPtr != mem->GetSrcBuffer()) {
        memcpy(mem->GetSrcBuffer(), params->SrcPtr, srcSize);
    }

    // Execute filter using thread channel
    bool success = SendThreadCommand(thread_id, FilterCommand::ApplyFilter, 1000);

    if (!success) {
        // Filter failed, destination buffer unchanged
        LeaveCriticalSection(&ipc.cs);
        return false;
    }

    // Memory barrier to ensure we see the host's writes to shared memory
    MemoryBarrier();

    if (!params->DstPtr) {
        LeaveCriticalSection(&ipc.cs);
        return false;
    }

    const uint8_t* srcData = static_cast<const uint8_t*>(mem->GetDstBuffer());
    uint8_t* dstData = static_cast<uint8_t*>(params->DstPtr);

    // Zero-copy fast path: caller is reading straight from the shared dst
    // buffer, so the copy-back would be a no-op or wrong (same pointer).
    if (dstData == srcData) {
        LeaveCriticalSection(&ipc.cs);
        return true;
    }

    // Copy result back from shared memory.
    // If actualDstPitch differs from DstPitch, we need row-by-row copying
    // because the plugin wrote with DstPitch but the destination buffer has actualDstPitch
    const uint32_t pluginPitch = params->DstPitch;
    const uint32_t bufferPitch = (actualDstPitch > 0) ? actualDstPitch : pluginPitch;
    const uint32_t dstHeight = params->DstH;

    if (pluginPitch == bufferPitch) {
        // Simple case: strides match, single memcpy
        size_t dstSize = static_cast<size_t>(pluginPitch) * dstHeight;
        if (dstSize <= kMaxPixelBufferSize) {
            memcpy(dstData, srcData, dstSize);
        }
    } else {
        // Stride conversion: copy row by row
        // Plugin writes rows with pluginPitch bytes between them
        // Destination buffer expects bufferPitch bytes between rows
        const size_t rowBytes = pluginPitch;  // Amount of data per row to copy
        for (uint32_t y = 0; y < dstHeight; y++) {
            memcpy(dstData + y * bufferPitch, srcData + y * pluginPitch, rowBytes);
        }
    }

    LeaveCriticalSection(&ipc.cs);
    return true;
}

void* RpiProxyClient::GetSrcBuffer(uint32_t thread_id) {
    if (thread_id >= kMaxFilterThreads) return nullptr;
    ThreadIPC& ipc = thread_ipc_[thread_id];
    return ipc.shared_mem ? ipc.shared_mem->GetSrcBuffer() : nullptr;
}

void* RpiProxyClient::GetDstBuffer(uint32_t thread_id) {
    if (thread_id >= kMaxFilterThreads) return nullptr;
    ThreadIPC& ipc = thread_ipc_[thread_id];
    return ipc.shared_mem ? ipc.shared_mem->GetDstBuffer() : nullptr;
}

std::vector<RpiProxyClient::PluginEnumResult> RpiProxyClient::EnumeratePlugins(
        const std::vector<wxString>& paths) {
    std::vector<PluginEnumResult> results;

    if (paths.empty()) {
        return results;
    }

    uint32_t count = static_cast<uint32_t>(paths.size());
    if (count > kMaxEnumPlugins) {
        count = kMaxEnumPlugins;
    }

    if (!LaunchHost()) {
        return results;
    }

    // Lock main channel for the entire enumeration
    EnterCriticalSection(&main_channel_cs_);

    // Write plugin paths into the src buffer area
    PluginEnumEntry* entries = shared_mem_->GetEnumEntries();
    for (uint32_t i = 0; i < count; i++) {
        wcsncpy(entries[i].path, paths[i].wc_str(), MAX_PATH - 1);
        entries[i].path[MAX_PATH - 1] = L'\0';
        entries[i].name[0] = '\0';
        entries[i].flags = 0;
        entries[i].valid = 0;
    }
    shared_mem_->numEnumPlugins = count;

    // Use a longer timeout: ~1 second per plugin for DLL loading
    uint32_t timeout = 5000 + count * 1000;
    bool success = SendCommand(FilterCommand::EnumeratePlugins, timeout);
    LeaveCriticalSection(&main_channel_cs_);

    if (!success) {
        return results;
    }

    // Read results back from shared memory
    results.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        PluginEnumResult r;
        r.path = paths[i];
        r.name = wxString(entries[i].name, wxConvUTF8);
        r.flags = entries[i].flags;
        r.valid = (entries[i].valid != 0);
        results.push_back(std::move(r));
    }

    return results;
}

void RpiProxyClient::UnloadPlugin() {
    if (!is_loaded_) {
        return;
    }

    // Stop each active filter thread through StopThread() so any in-flight
    // ApplyFilter() call is properly drained (it blocks on the per-thread
    // critical section until the call finishes) and its IPC channel is torn
    // down cleanly, before the host is told to unload the plugin under it.
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        if (thread_ipc_[i].active) {
            StopThread(i);
        }
    }

    if (shared_mem_ && host_process_) {
        SendCommand(FilterCommand::UnloadPlugin);
    }

    // Covers any slot StopThread() above didn't reach.
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        thread_ipc_[i].active = false;
    }

    is_loaded_ = false;
    memset(&plugin_info_, 0, sizeof(plugin_info_));
}

}  // namespace rpi_proxy

#endif  // _WIN32
