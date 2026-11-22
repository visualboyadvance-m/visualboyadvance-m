// In-place updates on Windows, see inplace-update.h.

#include "wx/autoupdater/wxsparkle/inplace-update.h"

#include <windows.h>

#include <cwchar>
#include <memory>
#include <string>

#include <wx/filename.h>
#include <wx/log.h>
#include <wx/string.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

// Set for the new instance: the process id of the one that installed it.
constexpr wchar_t kWaitPidVar[] = L"VBAM_UPDATER_WAIT_PID";

std::wstring RunningExecutable() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = ::GetModuleFileNameW(nullptr, &path[0], static_cast<DWORD>(path.size()));
        if (n == 0)
            return std::wstring();
        if (n < path.size()) {
            path.resize(n);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

// Writes the zip's entry `name` to `dest`; wxZipInputStream checks the CRC
// at the end of the entry.
bool ExtractEntry(const wxString& zip, const wxString& name, const wxString& dest) {
    wxFFileInputStream in(zip);
    if (!in.IsOk())
        return false;
    wxZipInputStream zin(in);

    std::unique_ptr<wxZipEntry> entry;
    while (entry.reset(zin.GetNextEntry()), entry) {
        if (entry->GetInternalName() != name)
            continue;

        bool ok;
        {
            wxFFileOutputStream out(dest);
            if (!out.IsOk())
                return false;
            zin.Read(out);
            ok = zin.GetLastError() == wxSTREAM_EOF && out.Close();
        }
        if (ok && entry->GetSize() >= 0)
            ok = wxFileName::GetSize(dest) == wxULongLong(entry->GetSize());
        if (!ok)
            ::DeleteFileW(dest.wc_str());
        return ok;
    }
    return false;
}

// The zip's executable for this one: the .exe of the same name, else the only
// .exe in it.
wxString FindExecutable(const wxString& zip, const wxString& exeName) {
    wxFFileInputStream in(zip);
    if (!in.IsOk())
        return wxString();
    wxZipInputStream zin(in);

    wxString only;
    int count = 0;
    std::unique_ptr<wxZipEntry> entry;
    while (entry.reset(zin.GetNextEntry()), entry) {
        if (entry->IsDir())
            continue;
        const wxFileName fn(entry->GetName(wxPATH_WIN), wxPATH_WIN);
        if (fn.GetExt().CmpNoCase("exe") != 0)
            continue;
        if (fn.GetFullName().CmpNoCase(exeName) == 0)
            return entry->GetInternalName();
        only = entry->GetInternalName();
        ++count;
    }
    return count == 1 ? only : wxString();
}

bool Move(const std::wstring& from, const std::wstring& to) {
    return ::MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

bool Start(const std::wstring& exe) {
    const std::wstring pid = std::to_wstring(::GetCurrentProcessId());
    ::SetEnvironmentVariableW(kWaitPidVar, pid.c_str());

    std::wstring cmdline = L"\"" + exe + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const std::wstring dir = wxFileName(exe).GetPath().ToStdWstring();
    const BOOL ok = ::CreateProcessW(exe.c_str(), &cmdline[0], nullptr, nullptr, FALSE, 0,
                                     nullptr, dir.c_str(), &si, &pi);
    ::SetEnvironmentVariableW(kWaitPidVar, nullptr);
    if (!ok)
        return false;
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
}

}  // namespace

int InPlaceUpdateRunInstaller(const wchar_t* file) {
    const wxString zip(file);
    if (wxFileName(zip).GetExt().CmpNoCase("zip") != 0)
        return 0;

    const std::wstring exe = RunningExecutable();
    if (exe.empty())
        return 0;
    const std::wstring newExe = exe + L".new";
    const std::wstring oldExe = exe + L".old";

    wxLogNull noLog;

    const wxString entry = FindExecutable(zip, wxFileName(exe).GetFullName());
    if (entry.empty()) {
        wxLogDebug(wxT("In-place update: no executable in %s"), zip);
        return 0;
    }

    // Unpacked next to the executable, so that the renames stay on one volume
    // (and a directory that cannot be written to -- Program Files without
    // elevation -- shows here, before anything was changed).
    if (!ExtractEntry(zip, entry, newExe)) {
        wxLogDebug(wxT("In-place update: cannot unpack %s"), entry);
        return 0;
    }

    ::DeleteFileW(oldExe.c_str());
    if (!Move(exe, oldExe)) {
        ::DeleteFileW(newExe.c_str());
        return 0;
    }
    if (!Move(newExe, exe)) {
        Move(oldExe, exe);
        ::DeleteFileW(newExe.c_str());
        return 0;
    }
    if (!Start(exe)) {
        Move(exe, newExe);
        Move(oldExe, exe);
        ::DeleteFileW(newExe.c_str());
        return 0;
    }
    return 1;
}

void InPlaceUpdateStartup() {
    wchar_t buf[16];
    const DWORD n = ::GetEnvironmentVariableW(kWaitPidVar, buf, 16);
    bool updated = false;
    if (n > 0 && n < 16) {
        ::SetEnvironmentVariableW(kWaitPidVar, nullptr);
        updated = true;
        const DWORD pid = static_cast<DWORD>(std::wcstoul(buf, nullptr, 10));
        // The old instance writes its configuration as it exits.
        if (HANDLE h = ::OpenProcess(SYNCHRONIZE, FALSE, pid)) {
            ::WaitForSingleObject(h, 60000);
            ::CloseHandle(h);
        }
    }

    const std::wstring exe = RunningExecutable();
    if (exe.empty())
        return;
    const std::wstring oldExe = exe + L".old";
    // The old image may stay mapped for a moment after its process ended.
    for (int i = 0; i < (updated ? 50 : 1); ++i) {
        if (::DeleteFileW(oldExe.c_str()) || ::GetLastError() == ERROR_FILE_NOT_FOUND)
            break;
        ::Sleep(100);
    }
}
