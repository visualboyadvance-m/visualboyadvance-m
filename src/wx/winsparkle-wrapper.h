#ifndef WINSPARKLE_WRAPPER_H
#define WINSPARKLE_WRAPPER_H

#include <wx/string.h>
#include <wx/dynlib.h>

#include "winsparkle-rc.h"

class WinSparkleDllWrapper {
public:
    static WinSparkleDllWrapper *GetInstance();
    ~WinSparkleDllWrapper();
private:
    WinSparkleDllWrapper();

    wxDynamicLibrary *winsparkle_dll = nullptr;

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

    wxString temp_file_name;

    // the API needs to access the function pointers
    friend void win_sparkle_init();
    friend void win_sparkle_check_update_with_ui();
    friend void win_sparkle_set_appcast_url(const char *url);
    friend void win_sparkle_set_app_details(const wchar_t *company_name, const wchar_t *app_name, const wchar_t *app_version);
    friend void win_sparkle_cleanup();
};

void win_sparkle_init();
void win_sparkle_check_update_with_ui();
void win_sparkle_set_appcast_url(const char *url);
void win_sparkle_set_app_details(const wchar_t *company_name, const wchar_t *app_name, const wchar_t *app_version);
void win_sparkle_cleanup();

#endif /* WINSPARKLE_WRAPPER_H */
