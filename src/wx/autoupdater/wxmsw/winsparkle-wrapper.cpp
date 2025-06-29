#include "wx/autoupdater/wxmsw/winsparkle-wrapper.h"

#include <cstdio>
#include <cstdint>

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/msw/private.h>
#include <wx/utils.h>

#include <Shlwapi.h>

#include "wx/autoupdater/wxmsw/winsparkle-rc.h"

WinSparkleDllWrapper *WinSparkleDllWrapper::GetInstance()
{
    static WinSparkleDllWrapper instance;

    return &instance;
}

WinSparkleDllWrapper::WinSparkleDllWrapper()
{
    wxFile temp_file;
    temp_file_name = wxFileName::CreateTempFileName("winsparkle", &temp_file);
    HRSRC res = FindResource(wxGetInstance(), MAKEINTRESOURCE(WINSPARKLE_DLL_RC), RT_RCDATA);

    if (!res)
	wxLogFatalError("Could not find resource for winsparkle DLL.");

    HGLOBAL res_handle = LoadResource(NULL, res);

    if (!res_handle)
	wxLogFatalError("Could not load resource for winsparkle DLL.");

    uint64_t res_size = SizeofResource(NULL, res);

    char *res_data = (char *)LockResource(res_handle);

    temp_file.Write((void *)res_data, res_size);
    temp_file.Close();

    winsparkle_dll = new wxDynamicLibrary(temp_file_name, wxDL_NOW | wxDL_VERBATIM);

    if (winsparkle_dll != nullptr) {
        winsparkle_init = reinterpret_cast<func_win_sparkle_init>(winsparkle_dll->GetSymbol("win_sparkle_init"));
        winsparkle_check_update_with_ui = reinterpret_cast<func_win_sparkle_check_update_with_ui>(winsparkle_dll->GetSymbol("win_sparkle_check_update_with_ui"));
        winsparkle_set_appcast_url = reinterpret_cast<func_win_sparkle_set_appcast_url>(winsparkle_dll->GetSymbol("win_sparkle_set_appcast_url"));
        winsparkle_set_app_details = reinterpret_cast<func_win_sparkle_set_app_details>(winsparkle_dll->GetSymbol("win_sparkle_set_app_details"));
        winsparkle_cleanup = reinterpret_cast<func_win_sparkle_cleanup>(winsparkle_dll->GetSymbol("win_sparkle_cleanup"));
    }
}


WinSparkleDllWrapper::~WinSparkleDllWrapper()
{
    HMODULE hmod = winsparkle_dll->Detach();
    if (hmod != nullptr) {
        while(::FreeLibrary(hmod)) {
            wxMilliSleep(50);
        }
    }
    delete winsparkle_dll;

    if (!temp_file_name) {
        return; // No need to delete the file if it was never created.
	}

    if (PathFileExistsW(temp_file_name.wc_str())) {
        wxRemoveFile(temp_file_name);
	}
}


void win_sparkle_init()
{
    if (WinSparkleDllWrapper::GetInstance()->winsparkle_init)
        WinSparkleDllWrapper::GetInstance()->winsparkle_init();
}


void win_sparkle_check_update_with_ui()
{
    if (WinSparkleDllWrapper::GetInstance()->winsparkle_check_update_with_ui)
        WinSparkleDllWrapper::GetInstance()->winsparkle_check_update_with_ui();
}


void win_sparkle_set_appcast_url(const char *url)
{
    if (WinSparkleDllWrapper::GetInstance()->winsparkle_set_appcast_url)
        WinSparkleDllWrapper::GetInstance()->winsparkle_set_appcast_url(url);
}


void win_sparkle_set_app_details(const wchar_t *company_name, const wchar_t *app_name, const wchar_t *app_version)
{
    if (WinSparkleDllWrapper::GetInstance()->winsparkle_set_app_details)
        WinSparkleDllWrapper::GetInstance()->winsparkle_set_app_details(company_name, app_name, app_version);
}


void win_sparkle_cleanup()
{
    if (WinSparkleDllWrapper::GetInstance()->winsparkle_cleanup)
        WinSparkleDllWrapper::GetInstance()->winsparkle_cleanup();
}
