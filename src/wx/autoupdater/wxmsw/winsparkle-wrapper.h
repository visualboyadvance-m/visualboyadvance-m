#ifndef WINSPARKLE_WRAPPER_H
#define WINSPARKLE_WRAPPER_H

#include <wx/string.h>
#include <wx/dynlib.h>

extern "C" {
    typedef void (__cdecl *func_win_sparkle_init)();
    typedef void (__cdecl *func_win_sparkle_check_update_with_ui)();
    typedef void (__cdecl *func_win_sparkle_set_appcast_url)(const char*);
    typedef void (__cdecl *func_win_sparkle_set_app_details)(const wchar_t*, const wchar_t*, const wchar_t*);
    typedef void (__cdecl *func_win_sparkle_cleanup)();

    extern wxDynamicLibrary* winsparkle_dll;
}

class WinSparkleDllWrapper {
public:
    static WinSparkleDllWrapper *GetInstance();
private:
    WinSparkleDllWrapper();
    ~WinSparkleDllWrapper();

    func_win_sparkle_init                 winsparkle_init = nullptr;
    func_win_sparkle_check_update_with_ui winsparkle_check_update_with_ui = nullptr;
    func_win_sparkle_set_appcast_url      winsparkle_set_appcast_url      = nullptr;
    func_win_sparkle_set_app_details      winsparkle_set_app_details      = nullptr;
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
