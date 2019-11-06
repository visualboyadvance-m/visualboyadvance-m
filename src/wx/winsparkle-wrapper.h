#ifndef WINSPARKLE_WRAPPER_H
#define WINSPARKLE_WRAPPER_H

#define WINSPARKLE_DLL_RC 300

struct WinSparkleDllWrapper {
    WinSparkleDllWrapper();
    ~WinSparkleDllWrapper();
};

void win_sparkle_init();
void win_sparkle_check_update_with_ui();
void win_sparkle_set_appcast_url(const char *url);
void win_sparkle_set_app_details(const wchar_t *company_name, const wchar_t *app_name, const wchar_t *app_version);
void win_sparkle_cleanup();

#endif /* WINSPARKLE_WRAPPER_H */
