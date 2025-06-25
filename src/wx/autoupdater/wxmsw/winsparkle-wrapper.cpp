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

extern "C" {
    func_win_sparkle_init ws_init;
    func_win_sparkle_check_update_with_ui ws_cu_wui;
    func_win_sparkle_set_appcast_url ws_saurl;
    func_win_sparkle_set_app_details ws_sad;
    func_win_sparkle_cleanup ws_cu;
    HMODULE winsparkle_dll = nullptr;

    void winsparkle_load_symbols(wxString path)
    {
        winsparkle_dll = LoadLibraryW(path.wc_str());

        if (winsparkle_dll != nullptr) {
            ws_init = reinterpret_cast<func_win_sparkle_init>(GetProcAddress(winsparkle_dll, "win_sparkle_init"));
            ws_cu_wui = reinterpret_cast<func_win_sparkle_check_update_with_ui>(GetProcAddress(winsparkle_dll, "win_sparkle_check_update_with_ui"));
            ws_saurl = reinterpret_cast<func_win_sparkle_set_appcast_url>(GetProcAddress(winsparkle_dll, "win_sparkle_set_appcast_url"));
            ws_sad = reinterpret_cast<func_win_sparkle_set_app_details>(GetProcAddress(winsparkle_dll, "win_sparkle_set_app_details"));
            ws_cu = reinterpret_cast<func_win_sparkle_cleanup>(GetProcAddress(winsparkle_dll, "win_sparkle_cleanup"));
        }
    }
}

WinSparkleDllWrapper::WinSparkleDllWrapper()
{
    wchar_t temp_file_path_w[MAX_PATH + 1];
	GetTempPathW(MAX_PATH, temp_file_path_w);
    wxString temp_file_path = wxString(temp_file_path_w);
    wchar_t temp_file_name_w[MAX_PATH + 1];
	GetLongPathNameW(temp_file_path.wc_str(), temp_file_name_w, MAX_PATH + 1);
	temp_file_name = wxString(temp_file_name_w) + "WinSparkle.dll";
    wxFile temp_file = wxFile(temp_file_name, wxFile::write);
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

    winsparkle_load_symbols(temp_file_name);

    winsparkle_init = ws_init;
    winsparkle_check_update_with_ui = ws_cu_wui;
    winsparkle_set_appcast_url = ws_saurl;
    winsparkle_set_app_details = ws_sad;
    winsparkle_cleanup = ws_cu;
}


WinSparkleDllWrapper::~WinSparkleDllWrapper()
{
    if (winsparkle_dll != nullptr) {
        while(::FreeLibrary(winsparkle_dll)) {
            wxMilliSleep(50);
        }
    }

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
