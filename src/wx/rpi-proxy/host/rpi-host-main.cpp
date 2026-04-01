// RPI Filter Host - 32-bit process that loads and executes RPI filter plugins
// This is embedded as a resource in the 64-bit VBA-M build and extracted at runtime
// to enable loading 32-bit filter plugins from the 64-bit application.

#if defined(_WIN32)

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "wx/rpi-proxy/SharedFilterBuffer.h"
#include "wx/rpi.h"

using namespace rpi_proxy;

// Debug logging (disabled in release builds)
static FILE* g_logFile = nullptr;

static void LogInit(uint32_t instanceId) {
    char path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, path)) {
        char filename[64];
        sprintf_s(filename, "vbam-rpi-host-%u.log", instanceId);
        strcat_s(path, filename);
        g_logFile = fopen(path, "w");
    }
}

static void LogClose() {
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}

static void Log(const char* fmt, ...) {
    if (!g_logFile) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    fflush(g_logFile);
}

// Global state
static HANDLE g_sharedMemHandle = nullptr;
static SharedFilterBuffer* g_sharedMem = nullptr;
static HANDLE g_requestEvent = nullptr;
static HANDLE g_responseEvent = nullptr;

// Client process ID and instance ID (passed via command line, used for unique IPC names)
static uint32_t g_clientPid = 0;
static uint32_t g_instanceId = 0;

// Plugin state for the main/loading instance
static HMODULE g_pluginHandle = nullptr;
static RENDER_PLUGIN_INFO* g_pluginInfo = nullptr;
static wchar_t g_pluginPath[MAX_PATH] = {0};  // Store path for thread instances

// Per-thread state for multi-threaded filtering
struct FilterThreadState {
    // Plugin instance
    HMODULE pluginHandle = nullptr;
    RENDER_PLUGIN_INFO* pluginInfo = nullptr;

    // Per-thread IPC
    HANDLE sharedMemHandle = nullptr;
    SharedFilterBuffer* sharedMem = nullptr;
    HANDLE requestEvent = nullptr;
    HANDLE responseEvent = nullptr;

    // Thread management
    HANDLE threadHandle = nullptr;
    uint32_t threadId = 0;
    volatile bool running = false;
    volatile bool active = false;
};
static FilterThreadState g_threadStates[kMaxFilterThreads];

// Critical section to serialize plugin Output calls
// Many RPI plugins have global state and are not thread-safe, so we need
// to ensure only one thread calls Output at a time.
static CRITICAL_SECTION g_pluginOutputCs;
static bool g_pluginOutputCsInitialized = false;

// Log count per thread for ApplyFilter debugging (reset when threads restart)
static int g_applyFilterLogCount[kMaxFilterThreads] = {0};

// Forward declarations
static DWORD WINAPI FilterThreadProc(LPVOID param);
static void CleanupThreadIPC(FilterThreadState* state);

static bool InitializeIPC() {
    // Generate IPC names using client PID and instance ID
    wchar_t memName[128], reqName[128], respName[128];
    GetSharedMemoryName(g_clientPid, g_instanceId, memName, 128);
    GetRequestEventName(g_clientPid, g_instanceId, reqName, 128);
    GetResponseEventName(g_clientPid, g_instanceId, respName, 128);

    Log("rpi-host: InitializeIPC with clientPid=%u, instanceId=%u, memName=%ls\n", g_clientPid, g_instanceId, memName);

    // Open the shared memory created by the parent process
    g_sharedMemHandle = OpenFileMappingW(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        memName);

    if (!g_sharedMemHandle) {
        Log("rpi-host: Failed to open shared memory: %lu\n", GetLastError());
        return false;
    }
    Log("rpi-host: Opened shared memory handle: %p\n", (void*)g_sharedMemHandle);

    g_sharedMem = static_cast<SharedFilterBuffer*>(
        MapViewOfFile(g_sharedMemHandle, FILE_MAP_ALL_ACCESS, 0, 0, kSharedMemorySize));

    if (!g_sharedMem) {
        Log("rpi-host: Failed to map shared memory: %lu\n", GetLastError());
        CloseHandle(g_sharedMemHandle);
        g_sharedMemHandle = nullptr;
        return false;
    }
    Log("rpi-host: Mapped shared memory at: %p\n", (void*)g_sharedMem);

    // Open synchronization events
    g_requestEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, reqName);
    if (!g_requestEvent) {
        Log("rpi-host: Failed to open request event: %lu\n", GetLastError());
        return false;
    }
    Log("rpi-host: Opened request event: %p\n", (void*)g_requestEvent);

    g_responseEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, respName);
    if (!g_responseEvent) {
        Log("rpi-host: Failed to open response event: %lu\n", GetLastError());
        return false;
    }
    Log("rpi-host: Opened response event: %p\n", (void*)g_responseEvent);

    return true;
}

static void CleanupIPC() {
    if (g_sharedMem) {
        UnmapViewOfFile(g_sharedMem);
        g_sharedMem = nullptr;
    }
    if (g_sharedMemHandle) {
        CloseHandle(g_sharedMemHandle);
        g_sharedMemHandle = nullptr;
    }
    if (g_requestEvent) {
        CloseHandle(g_requestEvent);
        g_requestEvent = nullptr;
    }
    if (g_responseEvent) {
        CloseHandle(g_responseEvent);
        g_responseEvent = nullptr;
    }
}

static void UnloadCurrentPlugin() {
    if (g_pluginHandle) {
        FreeLibrary(g_pluginHandle);
        g_pluginHandle = nullptr;
        g_pluginInfo = nullptr;
    }
}

// Load a plugin into a specific handle/info pair (for thread instances)
static bool LoadPluginInstance(const wchar_t* path, HMODULE* outHandle, RENDER_PLUGIN_INFO** outInfo) {
    Log("rpi-host: LoadPluginInstance called with path: %ls\n", path);

    *outHandle = LoadLibraryW(path);
    if (!*outHandle) {
        Log("rpi-host: Failed to load plugin '%ls': %lu\n", path, GetLastError());
        return false;
    }
    Log("rpi-host: LoadLibraryW succeeded, handle=%p\n", (void*)*outHandle);

    RENDPLUG_GetInfo getInfo = reinterpret_cast<RENDPLUG_GetInfo>(
        GetProcAddress(*outHandle, "RenderPluginGetInfo"));

    if (!getInfo) {
        Log("rpi-host: Plugin missing RenderPluginGetInfo export\n");
        FreeLibrary(*outHandle);
        *outHandle = nullptr;
        return false;
    }
    Log("rpi-host: Found RenderPluginGetInfo at %p\n", (void*)getInfo);

    *outInfo = getInfo();
    if (!*outInfo) {
        Log("rpi-host: RenderPluginGetInfo returned null\n");
        FreeLibrary(*outHandle);
        *outHandle = nullptr;
        return false;
    }
    Log("rpi-host: Got plugin info: Name='%s', Flags=0x%x\n",
        (*outInfo)->Name, (*outInfo)->Flags);

    // Validate version - accept both version 1 and 2
    unsigned int pluginVersion = (*outInfo)->Flags & 0xff;
    if (pluginVersion != 1 && pluginVersion != 2) {
        Log("rpi-host: Plugin version %u not supported (expected 1 or 2)\n", pluginVersion);
        FreeLibrary(*outHandle);
        *outHandle = nullptr;
        *outInfo = nullptr;
        return false;
    }

    // Validate color format support (accept 555, 565, or 888)
    // Version 1 plugins may not set color format flags - assume 565 support for compatibility
    if (((*outInfo)->Flags & (RPI_555_SUPP | RPI_565_SUPP | RPI_888_SUPP)) == 0) {
        if (pluginVersion == 1) {
            // Version 1 plugins without color flags - assume 565 support (common for GBA)
            Log("rpi-host: Version 1 plugin without color flags, assuming 565 support\n");
            (*outInfo)->Flags |= RPI_565_SUPP;
        } else {
            Log("rpi-host: Plugin doesn't support required color formats (flags=0x%x)\n",
                (*outInfo)->Flags);
            FreeLibrary(*outHandle);
            *outHandle = nullptr;
            *outInfo = nullptr;
            return false;
        }
    }

    // Get Output function if not set
    if (!(*outInfo)->Output) {
        (*outInfo)->Output = reinterpret_cast<RENDPLUG_Output>(
            GetProcAddress(*outHandle, "RenderPluginOutput"));
        Log("rpi-host: Got RenderPluginOutput from GetProcAddress: %p\n",
            (void*)(*outInfo)->Output);
    }

    if (!(*outInfo)->Output) {
        Log("rpi-host: Plugin has no Output function\n");
        FreeLibrary(*outHandle);
        *outHandle = nullptr;
        *outInfo = nullptr;
        return false;
    }

    Log("rpi-host: Plugin instance loaded successfully\n");
    return true;
}

static bool LoadPlugin(const wchar_t* path) {
    Log("rpi-host: LoadPlugin called with path: %ls\n", path);

    UnloadCurrentPlugin();

    // Store the path for later thread instance creation
    wcsncpy_s(g_pluginPath, MAX_PATH, path, _TRUNCATE);

    // Use the common loading function
    if (!LoadPluginInstance(path, &g_pluginHandle, &g_pluginInfo)) {
        g_pluginPath[0] = L'\0';
        return false;
    }

    Log("rpi-host: Plugin loaded successfully\n");
    return true;
}

static void HandleLoadPlugin() {
    Log("rpi-host: HandleLoadPlugin called, path='%ls'\n", g_sharedMem->pluginPath);

    if (LoadPlugin(g_sharedMem->pluginPath)) {
        // Copy plugin info to shared memory
        strncpy_s(g_sharedMem->pluginInfo.name, sizeof(g_sharedMem->pluginInfo.name),
            g_pluginInfo->Name, _TRUNCATE);
        g_sharedMem->pluginInfo.flags = g_pluginInfo->Flags;
        g_sharedMem->pluginInfo.scale = (g_pluginInfo->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
        g_sharedMem->result = FilterResult::Success;
        unsigned int pluginVersion = g_pluginInfo->Flags & 0xff;
        Log("rpi-host: HandleLoadPlugin success, name='%s', flags=0x%lx, version=%u, scale=%u, "
            "555=%d, 565=%d, 888=%d\n", g_pluginInfo->Name, g_pluginInfo->Flags, pluginVersion,
            g_sharedMem->pluginInfo.scale,
            (g_pluginInfo->Flags & RPI_555_SUPP) ? 1 : 0,
            (g_pluginInfo->Flags & RPI_565_SUPP) ? 1 : 0,
            (g_pluginInfo->Flags & RPI_888_SUPP) ? 1 : 0);
    } else {
        g_sharedMem->result = FilterResult::InvalidPlugin;
        Log("rpi-host: HandleLoadPlugin failed, setting result=InvalidPlugin\n");
    }
}

static void CleanupThreadIPC(FilterThreadState* state) {
    if (state->sharedMem) {
        UnmapViewOfFile(state->sharedMem);
        state->sharedMem = nullptr;
    }
    if (state->sharedMemHandle) {
        CloseHandle(state->sharedMemHandle);
        state->sharedMemHandle = nullptr;
    }
    if (state->requestEvent) {
        CloseHandle(state->requestEvent);
        state->requestEvent = nullptr;
    }
    if (state->responseEvent) {
        CloseHandle(state->responseEvent);
        state->responseEvent = nullptr;
    }
}

static bool InitializeThreadIPC(FilterThreadState* state, uint32_t threadId) {
    wchar_t memName[128], reqName[128], respName[128];
    GetThreadSharedMemoryName(g_clientPid, g_instanceId, threadId, memName, 128);
    GetThreadRequestEventName(g_clientPid, g_instanceId, threadId, reqName, 128);
    GetThreadResponseEventName(g_clientPid, g_instanceId, threadId, respName, 128);

    Log("rpi-host: InitializeThreadIPC: thread %u, memName=%ls\n", threadId, memName);

    // Open shared memory created by the client
    state->sharedMemHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, memName);
    if (!state->sharedMemHandle) {
        Log("rpi-host: Failed to open thread %u shared memory: %lu\n", threadId, GetLastError());
        return false;
    }

    state->sharedMem = static_cast<SharedFilterBuffer*>(
        MapViewOfFile(state->sharedMemHandle, FILE_MAP_ALL_ACCESS, 0, 0, kSharedMemorySize));
    if (!state->sharedMem) {
        Log("rpi-host: Failed to map thread %u shared memory: %lu\n", threadId, GetLastError());
        CloseHandle(state->sharedMemHandle);
        state->sharedMemHandle = nullptr;
        return false;
    }

    // Open events created by the client
    state->requestEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, reqName);
    if (!state->requestEvent) {
        Log("rpi-host: Failed to open thread %u request event: %lu\n", threadId, GetLastError());
        CleanupThreadIPC(state);
        return false;
    }

    state->responseEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, respName);
    if (!state->responseEvent) {
        Log("rpi-host: Failed to open thread %u response event: %lu\n", threadId, GetLastError());
        CleanupThreadIPC(state);
        return false;
    }

    Log("rpi-host: Thread %u IPC initialized successfully\n", threadId);
    return true;
}

// Thread procedure for per-thread filter processing
static DWORD WINAPI FilterThreadProc(LPVOID param) {
    FilterThreadState* state = static_cast<FilterThreadState*>(param);
    uint32_t threadId = state->threadId;

    Log("rpi-host: FilterThreadProc started for thread %u\n", threadId);

    while (state->running) {
        // Wait for a command
        DWORD waitResult = WaitForSingleObject(state->requestEvent, 1000);

        if (!state->running) {
            Log("rpi-host: Thread %u stopping (running=false)\n", threadId);
            break;
        }

        if (waitResult == WAIT_TIMEOUT) {
            continue;
        }

        if (waitResult != WAIT_OBJECT_0) {
            Log("rpi-host: Thread %u wait failed: %lu\n", threadId, GetLastError());
            break;
        }

        // Memory barrier to ensure we see the latest writes from the client
        MemoryBarrier();

        // Process the command
        FilterCommand cmd = state->sharedMem->command;
        Log("rpi-host: Thread %u got command %d\n", threadId, (int)cmd);

        if (cmd == FilterCommand::ApplyFilter) {
            if (state->pluginInfo && state->pluginInfo->Output) {
                // Build RENDER_PLUGIN_OUTP from shared memory
                RENDER_PLUGIN_OUTP outp;
                outp.Size = sizeof(outp);
                outp.Flags = state->sharedMem->filterParams.flags;
                outp.SrcPtr = state->sharedMem->GetSrcBuffer();
                outp.SrcPitch = state->sharedMem->filterParams.srcPitch;
                outp.SrcW = state->sharedMem->filterParams.srcW;
                outp.SrcH = state->sharedMem->filterParams.srcH;
                outp.DstPtr = state->sharedMem->GetDstBuffer();
                outp.DstPitch = state->sharedMem->filterParams.dstPitch;
                outp.DstW = state->sharedMem->filterParams.dstW;
                outp.DstH = state->sharedMem->filterParams.dstH;
                outp.OutW = state->sharedMem->filterParams.outW;
                outp.OutH = state->sharedMem->filterParams.outH;

                // Log filter parameters for debugging (first 50 calls per thread after restart)
                if (g_applyFilterLogCount[threadId] < 50) {
                    Log("rpi-host: T%u ApplyFilter: Flags=0x%lx, SrcH=%lu, DstH=%lu, SrcPitch=%lu, DstPitch=%lu\n",
                        threadId, outp.Flags, outp.SrcH, outp.DstH, outp.SrcPitch, outp.DstPitch);
                    Log("rpi-host: T%u Buffer offsets: srcOff=%u, dstOff=%u, sharedMem=%p\n",
                        threadId, state->sharedMem->srcBufferOffset, state->sharedMem->dstBufferOffset, state->sharedMem);
                    // Log source data sample BEFORE calling plugin
                    const uint8_t* src = static_cast<const uint8_t*>(outp.SrcPtr);
                    Log("rpi-host: T%u SrcRow0: %02x%02x %02x%02x %02x%02x %02x%02x\n",
                        threadId, src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7]);
                    const uint8_t* srcRow1 = src + outp.SrcPitch;
                    Log("rpi-host: T%u SrcRow1: %02x%02x %02x%02x %02x%02x %02x%02x\n",
                        threadId, srcRow1[0], srcRow1[1], srcRow1[2], srcRow1[3], srcRow1[4], srcRow1[5], srcRow1[6], srcRow1[7]);
                }

                // Execute the filter with serialization
                // Many RPI plugins have global state and are not thread-safe,
                // so we serialize all Output calls through a critical section.
                EnterCriticalSection(&g_pluginOutputCs);
                state->pluginInfo->Output(&outp);
                LeaveCriticalSection(&g_pluginOutputCs);

                // Log destination data sample after plugin writes (first 3 calls per thread)
                if (g_applyFilterLogCount[threadId] < 3) {
                    const uint8_t* dst = static_cast<const uint8_t*>(outp.DstPtr);
                    // Log first 8 bytes of row 0
                    Log("rpi-host: T%u DstRow0: %02x%02x %02x%02x %02x%02x %02x%02x\n",
                        threadId, dst[0], dst[1], dst[2], dst[3], dst[4], dst[5], dst[6], dst[7]);
                    g_applyFilterLogCount[threadId]++;
                }
                state->sharedMem->result = FilterResult::Success;
            } else {
                state->sharedMem->result = FilterResult::PluginNotLoaded;
            }
        } else {
            state->sharedMem->result = FilterResult::InvalidParameters;
        }

        // Reset command and signal completion
        state->sharedMem->command = FilterCommand::None;
        state->sharedMem->response_seq = state->sharedMem->request_seq;
        MemoryBarrier();
        SetEvent(state->responseEvent);
    }

    Log("rpi-host: FilterThreadProc exiting for thread %u\n", threadId);
    return 0;
}

static void HandleStartThread() {
    uint32_t threadId = g_sharedMem->filterParams.thread_id;
    Log("rpi-host: HandleStartThread called for thread %u\n", threadId);

    if (threadId >= kMaxFilterThreads) {
        Log("rpi-host: Invalid thread id %u\n", threadId);
        g_sharedMem->result = FilterResult::InvalidParameters;
        return;
    }

    if (g_pluginPath[0] == L'\0') {
        Log("rpi-host: No plugin loaded, cannot start thread\n");
        g_sharedMem->result = FilterResult::PluginNotLoaded;
        return;
    }

    FilterThreadState* state = &g_threadStates[threadId];

    // Stop existing thread if running
    if (state->active) {
        Log("rpi-host: Thread %u already active, stopping first\n", threadId);
        state->running = false;
        if (state->threadHandle) {
            TerminateThread(state->threadHandle, 0);
            CloseHandle(state->threadHandle);
            state->threadHandle = nullptr;
        }
        if (state->pluginHandle) {
            FreeLibrary(state->pluginHandle);
        }
        state->pluginHandle = nullptr;
        state->pluginInfo = nullptr;
        CleanupThreadIPC(state);
        state->active = false;
    }

    // Initialize per-thread IPC
    if (!InitializeThreadIPC(state, threadId)) {
        Log("rpi-host: Failed to initialize IPC for thread %u\n", threadId);
        g_sharedMem->result = FilterResult::Failed;
        return;
    }

    // Load a new plugin instance for this thread
    if (!LoadPluginInstance(g_pluginPath, &state->pluginHandle, &state->pluginInfo)) {
        Log("rpi-host: Failed to load plugin instance for thread %u\n", threadId);
        CleanupThreadIPC(state);
        g_sharedMem->result = FilterResult::InvalidPlugin;
        return;
    }

    // Start the thread
    state->threadId = threadId;
    state->running = true;
    state->active = true;
    state->threadHandle = CreateThread(nullptr, 0, FilterThreadProc, state, 0, nullptr);

    if (!state->threadHandle) {
        Log("rpi-host: Failed to create thread %u: %lu\n", threadId, GetLastError());
        FreeLibrary(state->pluginHandle);
        state->pluginHandle = nullptr;
        state->pluginInfo = nullptr;
        CleanupThreadIPC(state);
        state->active = false;
        state->running = false;
        g_sharedMem->result = FilterResult::Failed;
        return;
    }

    // Reset the ApplyFilter log counter so we can see logs after restart
    g_applyFilterLogCount[threadId] = 0;

    Log("rpi-host: Thread %u started successfully\n", threadId);
    g_sharedMem->result = FilterResult::Success;
}

static void HandleStopThread() {
    uint32_t threadId = g_sharedMem->filterParams.thread_id;
    Log("rpi-host: HandleStopThread called for thread %u\n", threadId);

    if (threadId >= kMaxFilterThreads) {
        Log("rpi-host: Invalid thread id %u\n", threadId);
        g_sharedMem->result = FilterResult::InvalidParameters;
        return;
    }

    FilterThreadState* state = &g_threadStates[threadId];

    if (state->active) {
        // Signal thread to stop
        state->running = false;

        // Forcefully terminate thread for instant shutdown
        if (state->threadHandle) {
            TerminateThread(state->threadHandle, 0);
            CloseHandle(state->threadHandle);
            state->threadHandle = nullptr;
        }

        // Cleanup plugin
        if (state->pluginHandle) {
            FreeLibrary(state->pluginHandle);
        }
        state->pluginHandle = nullptr;
        state->pluginInfo = nullptr;

        // Cleanup IPC
        CleanupThreadIPC(state);

        state->active = false;
        Log("rpi-host: Thread %u stopped\n", threadId);
    }

    g_sharedMem->result = FilterResult::Success;
}

static void HandleApplyFilter() {
    uint32_t threadId = g_sharedMem->filterParams.thread_id;
    Log("rpi-host: HandleApplyFilter called for thread %u\n", threadId);

    // Determine which plugin instance to use
    RENDER_PLUGIN_INFO* pluginInfo = nullptr;

    if (threadId < kMaxFilterThreads && g_threadStates[threadId].active) {
        // Use thread-specific instance
        pluginInfo = g_threadStates[threadId].pluginInfo;
        Log("rpi-host: Using thread %u plugin instance\n", threadId);
    } else {
        // Fall back to main instance
        pluginInfo = g_pluginInfo;
        Log("rpi-host: Using main plugin instance\n");
    }

    if (!pluginInfo || !pluginInfo->Output) {
        Log("rpi-host: No plugin available\n");
        g_sharedMem->result = FilterResult::PluginNotLoaded;
        return;
    }

    // Build RENDER_PLUGIN_OUTP from shared memory
    RENDER_PLUGIN_OUTP outp;
    outp.Size = sizeof(outp);
    outp.Flags = g_sharedMem->filterParams.flags;
    outp.SrcPtr = g_sharedMem->GetSrcBuffer();
    outp.SrcPitch = g_sharedMem->filterParams.srcPitch;
    outp.SrcW = g_sharedMem->filterParams.srcW;
    outp.SrcH = g_sharedMem->filterParams.srcH;
    outp.DstPtr = g_sharedMem->GetDstBuffer();
    outp.DstPitch = g_sharedMem->filterParams.dstPitch;
    outp.DstW = g_sharedMem->filterParams.dstW;
    outp.DstH = g_sharedMem->filterParams.dstH;
    outp.OutW = g_sharedMem->filterParams.outW;
    outp.OutH = g_sharedMem->filterParams.outH;

    // Execute the filter
    pluginInfo->Output(&outp);

    g_sharedMem->result = FilterResult::Success;
}

static void UnloadAllThreadInstances() {
    for (uint32_t i = 0; i < kMaxFilterThreads; i++) {
        FilterThreadState* state = &g_threadStates[i];
        if (state->active) {
            // Signal thread to stop
            state->running = false;

            // Forcefully terminate thread for instant shutdown
            if (state->threadHandle) {
                TerminateThread(state->threadHandle, 0);
                CloseHandle(state->threadHandle);
                state->threadHandle = nullptr;
            }

            // Cleanup plugin
            if (state->pluginHandle) {
                FreeLibrary(state->pluginHandle);
            }
            state->pluginHandle = nullptr;
            state->pluginInfo = nullptr;

            // Cleanup IPC
            CleanupThreadIPC(state);

            state->active = false;
        }
    }
}

static void HandleUnloadPlugin() {
    UnloadAllThreadInstances();
    UnloadCurrentPlugin();
    g_pluginPath[0] = L'\0';
    memset(&g_sharedMem->pluginInfo, 0, sizeof(g_sharedMem->pluginInfo));
    g_sharedMem->result = FilterResult::Success;
}

static void HandleGetInfo() {
    if (!g_pluginInfo) {
        g_sharedMem->result = FilterResult::PluginNotLoaded;
        return;
    }

    strncpy_s(g_sharedMem->pluginInfo.name, sizeof(g_sharedMem->pluginInfo.name),
        g_pluginInfo->Name, _TRUNCATE);
    g_sharedMem->pluginInfo.flags = g_pluginInfo->Flags;
    g_sharedMem->pluginInfo.scale = (g_pluginInfo->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
    g_sharedMem->result = FilterResult::Success;
}

static bool ProcessCommand(FilterCommand cmd) {
    Log("rpi-host: ProcessCommand, command=%d\n", (int)cmd);
    switch (cmd) {
        case FilterCommand::LoadPlugin:
            Log("rpi-host: Processing LoadPlugin\n");
            HandleLoadPlugin();
            break;

        case FilterCommand::ApplyFilter:
            Log("rpi-host: Processing ApplyFilter\n");
            HandleApplyFilter();
            break;

        case FilterCommand::UnloadPlugin:
            Log("rpi-host: Processing UnloadPlugin\n");
            HandleUnloadPlugin();
            break;

        case FilterCommand::GetInfo:
            Log("rpi-host: Processing GetInfo\n");
            HandleGetInfo();
            break;

        case FilterCommand::Shutdown:
            Log("rpi-host: Processing Shutdown\n");
            g_sharedMem->result = FilterResult::Success;
            return false;  // Signal to exit

        case FilterCommand::StartThread:
            Log("rpi-host: Processing StartThread\n");
            HandleStartThread();
            break;

        case FilterCommand::StopThread:
            Log("rpi-host: Processing StopThread\n");
            HandleStopThread();
            break;

        case FilterCommand::None:
            // Should never happen since we check before calling, but handle gracefully
            Log("rpi-host: ProcessCommand called with None, ignoring\n");
            break;

        default:
            Log("rpi-host: Unknown command %d\n", (int)cmd);
            g_sharedMem->result = FilterResult::InvalidParameters;
            break;
    }

    return true;  // Continue running
}

int main(int argc, char* argv[]) {
    // Parse client PID and instance ID from command line first
    if (argc < 3) {
        return 1;  // Can't log yet - no instance ID
    }
    g_clientPid = static_cast<uint32_t>(atoi(argv[1]));
    g_instanceId = static_cast<uint32_t>(atoi(argv[2]));

    // Initialize file logging with unique filename per instance
    LogInit(g_instanceId);
    Log("rpi-host: Starting up\n");
    Log("rpi-host: Client PID = %u, Instance ID = %u\n", g_clientPid, g_instanceId);

    // Pre-load msvcrt.dll to help with MinGW-compiled plugins that depend on it
    // This ensures the CRT is initialized before any plugin tries to use it
    HMODULE hMsvcrt = LoadLibraryA("msvcrt.dll");
    if (hMsvcrt) {
        Log("rpi-host: Pre-loaded msvcrt.dll at %p\n", (void*)hMsvcrt);
    } else {
        Log("rpi-host: Warning: Could not pre-load msvcrt.dll: %lu\n", GetLastError());
    }

    if (g_clientPid == 0) {
        Log("rpi-host: ERROR - invalid client PID\n");
        LogClose();
        return 1;
    }

    if (!InitializeIPC()) {
        Log("rpi-host: Failed to initialize IPC\n");
        LogClose();
        return 1;
    }
    Log("rpi-host: IPC initialized\n");

    // Initialize critical section for serializing plugin Output calls
    InitializeCriticalSection(&g_pluginOutputCs);
    g_pluginOutputCsInitialized = true;

    // Signal that we're ready (version check)
    g_sharedMem->version = SharedFilterBuffer::kCurrentVersion;
    g_sharedMem->result = FilterResult::Success;
    Log("rpi-host: Set version to %u, entering main loop\n", SharedFilterBuffer::kCurrentVersion);

    // Main command processing loop
    bool running = true;
    while (running) {
        Log("rpi-host: Waiting for command (timeout 30s)...\n");
        // Wait for a command from the parent process
        DWORD waitResult = WaitForSingleObject(g_requestEvent, 30000);  // 30 second timeout

        if (waitResult == WAIT_TIMEOUT) {
            Log("rpi-host: Wait timed out\n");
            // No signal received, just continue waiting
            continue;
        } else if (waitResult != WAIT_OBJECT_0) {
            Log("rpi-host: Wait failed: %lu\n", GetLastError());
            break;
        }

        // Memory barrier to ensure we see the latest writes from the client
        // This is critical because the client writes to shared memory then signals,
        // but without a barrier we might see stale cached values
        MemoryBarrier();

        // Read the command and sequence number ONCE into local variables to avoid TOCTOU races
        // The client might modify the shared memory between our reads (especially if it times out
        // and starts a new command), so we snapshot these values immediately after waking up
        // Note: Client writes command FIRST, then seq. If we see a new seq, command should be visible.
        uint32_t req_seq = g_sharedMem->request_seq;
        FilterCommand cmd = g_sharedMem->command;
        Log("rpi-host: Got signal, command=%d, seq=%u\n", (int)cmd, req_seq);

        // If command is None but we got a valid seq, there may be a timing issue where
        // the host woke up before the client's writes became fully visible. Wait briefly and retry.
        if (cmd == FilterCommand::None && req_seq != 0) {
            // Small spin-wait to allow client writes to become visible
            for (int retry = 0; retry < 20 && cmd == FilterCommand::None; retry++) {
                Sleep(1);  // 1ms delay
                MemoryBarrier();
                cmd = g_sharedMem->command;
            }
            if (cmd != FilterCommand::None) {
                Log("rpi-host: After retry %dms, command=%d\n", 20, (int)cmd);
            } else {
                Log("rpi-host: After retry, still command=None\n");
            }
        }

        // If command is still None after retries but we have a valid seq,
        // send a negative acknowledgment so the client can retry immediately
        // instead of waiting for the full timeout
        if (cmd == FilterCommand::None) {
            if (req_seq != 0) {
                Log("rpi-host: Command not received for seq=%u, sending NACK\n", req_seq);
                g_sharedMem->result = FilterResult::Failed;
                g_sharedMem->response_seq = req_seq;
                MemoryBarrier();
                SetEvent(g_responseEvent);
            } else {
                Log("rpi-host: Spurious signal with no command and seq=0, ignoring\n");
            }
            continue;
        }

        // Process the command using the snapshotted value
        running = ProcessCommand(cmd);
        Log("rpi-host: ProcessCommand returned %d, result=%d\n", running, (int)g_sharedMem->result);

        // Reset command
        g_sharedMem->command = FilterCommand::None;

        // Copy the SNAPSHOTTED request sequence number to response
        // This is critical: we must use the seq we captured at the start, not the current value
        // in shared memory (which the client may have already updated for the next command)
        g_sharedMem->response_seq = req_seq;
        MemoryBarrier();  // Ensure response_seq is visible before signaling

        // Signal completion
        Log("rpi-host: Signaling response event (seq=%u)\n", req_seq);
        SetEvent(g_responseEvent);
    }

    // Cleanup
    Log("rpi-host: Exiting, cleaning up\n");
    UnloadAllThreadInstances();
    UnloadCurrentPlugin();
    CleanupIPC();
    if (g_pluginOutputCsInitialized) {
        DeleteCriticalSection(&g_pluginOutputCs);
        g_pluginOutputCsInitialized = false;
    }
    LogClose();

    return 0;
}

#else  // !_WIN32

int main() {
    // This executable is Windows-only
    return 1;
}

#endif  // _WIN32
