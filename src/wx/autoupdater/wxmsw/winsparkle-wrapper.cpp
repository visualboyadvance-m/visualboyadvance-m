#include <cstdio>
#include <string>
#include <stdexcept>

#include <wx/utils.h>
#include "wxvbam.h"
#include "winsparkle-wrapper.h"
#include "wx/msw/private.h"


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

    winsparkle_init                 = reinterpret_cast<func_win_sparkle_init>(winsparkle_dll->GetSymbol("win_sparkle_init"));
    winsparkle_check_update_with_ui = reinterpret_cast<func_win_sparkle_check_update_with_ui>(winsparkle_dll->GetSymbol("win_sparkle_check_update_with_ui"));
    winsparkle_set_appcast_url      = reinterpret_cast<func_win_sparkle_set_appcast_url>(winsparkle_dll->GetSymbol("win_sparkle_set_appcast_url"));
    winsparkle_set_app_details      = reinterpret_cast<func_win_sparkle_set_app_details>(winsparkle_dll->GetSymbol("win_sparkle_set_app_details"));
    winsparkle_cleanup              = reinterpret_cast<func_win_sparkle_cleanup>(winsparkle_dll->GetSymbol("win_sparkle_cleanup"));
}


WinSparkleDllWrapper::~WinSparkleDllWrapper()
{
    HMODULE hMod = winsparkle_dll->Detach();
    while(::FreeLibrary(hMod)) {
        wxMilliSleep(50);
    }
    delete winsparkle_dll;
    wxRemoveFile(temp_file_name);
}


void win_sparkle_init()
{
    WinSparkleDllWrapper::GetInstance()->winsparkle_init();
}


void win_sparkle_check_update_with_ui()
{
    WinSparkleDllWrapper::GetInstance()->winsparkle_check_update_with_ui();
}


void win_sparkle_set_appcast_url(const char *url)
{
    WinSparkleDllWrapper::GetInstance()->winsparkle_set_appcast_url(url);
}


void win_sparkle_set_app_details(const wchar_t *company_name, const wchar_t *app_name, const wchar_t *app_version)
{
    WinSparkleDllWrapper::GetInstance()->winsparkle_set_app_details(company_name, app_name, app_version);
}


void win_sparkle_cleanup()
{
    WinSparkleDllWrapper::GetInstance()->winsparkle_cleanup();
}
