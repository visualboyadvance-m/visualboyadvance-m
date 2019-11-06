#include <stdexcept>

#include "wxvbam.h"
#include "winsparkle-wrapper.h"
#include "wx/msw/private.h"

wxDynamicLibrary                      *winsparkle_dll                  = nullptr;

typedef void (*func_win_sparkle_init)();
func_win_sparkle_init                 winsparkle_init                 = nullptr;

typedef void (*func_win_sparkle_check_update_with_ui)();
func_win_sparkle_check_update_with_ui winsparkle_check_update_with_ui = nullptr;

typedef void (*func_win_sparkle_set_appcast_url)(const char *);
func_win_sparkle_set_appcast_url      winsparkle_set_appcast_url      = nullptr;

typedef void (*func_win_sparkle_set_app_details)(const wchar_t*, const wchar_t*, const wchar_t*);
func_win_sparkle_set_app_details      winsparkle_set_app_details      = nullptr;

typedef void (*func_win_sparkle_cleanup)();
func_win_sparkle_cleanup              winsparkle_cleanup              = nullptr;

wxString *temp_file_name = nullptr;

WinSparkleDllWrapper *instance = nullptr;

WinSparkleDllWrapper *WinSparkleDllWrapper::GetInstance()
{
    if (instance)
	return instance;

    instance = new WinSparkleDllWrapper();

    return instance;
}

WinSparkleDllWrapper::WinSparkleDllWrapper()
{
    wxFile temp_file;
    temp_file_name = new wxString(wxFileName::CreateTempFileName("winsparkle", &temp_file));

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

    winsparkle_dll = new wxDynamicLibrary(*temp_file_name, wxDL_NOW | wxDL_VERBATIM);

    winsparkle_init                 = reinterpret_cast<func_win_sparkle_init>(winsparkle_dll->GetSymbol("win_sparkle_init"));
    winsparkle_check_update_with_ui = reinterpret_cast<func_win_sparkle_check_update_with_ui>(winsparkle_dll->GetSymbol("win_sparkle_check_update_with_ui"));
    winsparkle_set_appcast_url      = reinterpret_cast<func_win_sparkle_set_appcast_url>(winsparkle_dll->GetSymbol("win_sparkle_set_appcast_url"));
    winsparkle_set_app_details      = reinterpret_cast<func_win_sparkle_set_app_details>(winsparkle_dll->GetSymbol("win_sparkle_set_app_details"));
    winsparkle_cleanup              = reinterpret_cast<func_win_sparkle_cleanup>(winsparkle_dll->GetSymbol("win_sparkle_cleanup"));
}

WinSparkleDllWrapper::~WinSparkleDllWrapper()
{
    delete winsparkle_dll;

    wxRemoveFile(*temp_file_name);

    delete temp_file_name;
}

void win_sparkle_init()
{
    winsparkle_init();
}

void win_sparkle_check_update_with_ui()
{
    winsparkle_check_update_with_ui();
}


void win_sparkle_set_appcast_url(const char *url)
{
    winsparkle_set_appcast_url(url);
}


void win_sparkle_set_app_details(const wchar_t *company_name, const wchar_t *app_name, const wchar_t *app_version)
{
    winsparkle_set_app_details(company_name, app_name, app_version);
}


void win_sparkle_cleanup()
{
    winsparkle_cleanup();
}
