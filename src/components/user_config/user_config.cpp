#include "components/user_config/user_config.h"

#include <cstdlib>

#ifdef __DJGPP__
#include <crt0.h>

// DOS has no home directory. DJGPP's own convention is $HOME when set;
// otherwise keep everything next to the program.
static std::string dos_home()
{
    const char* home = getenv("HOME");
    if (home && *home)
        return home;
    std::string exe = __dos_argv0 ? __dos_argv0 : "";
    size_t cut = exe.find_last_of("/\\:");
    if (cut == std::string::npos)
        return ".";
    return exe.substr(0, cut);
}
#endif

// Get user-specific config dir manually.
// apple:   ~/Library/Application Support/
// windows: %APPDATA%/
// unix:    ${XDG_CONFIG_HOME:-~/.config}/
std::string get_xdg_user_config_home()
{
    std::string path;
#ifdef __APPLE__
    std::string home(getenv("HOME"));
    path = home + "/Library/Application Support";
#elif _WIN32
#if __STDC_WANT_SECURE_LIB__
    char *app_data_env = NULL;
    size_t app_data_env_sz = 0;
    _dupenv_s(&app_data_env, &app_data_env_sz, "LOCALAPPDATA");
    if (!app_data_env) _dupenv_s(&app_data_env, &app_data_env_sz, "APPDATA");
#else
    char *app_data_env = getenv("LOCALAPPDATA");
    if (!app_data_env) app_data_env = getenv("APPDATA");
#endif
    std::string app_data(app_data_env);
    path = app_data;
#elif defined(__DJGPP__)
    path = dos_home();
#else // Unix
    char *xdg_var = getenv("XDG_CONFIG_HOME");
    if (!xdg_var || !*xdg_var) {
        std::string xdg_default(getenv("HOME"));
        path = xdg_default + "/.config";
    } else {
        path = xdg_var;
    }
#endif
    return path + kFileSep;
}

// Get user-specific data dir manually.
// apple:   ~/Library/Application Support/
// windows: %APPDATA%/
// unix:    ${XDG_DATA_HOME:-~/.local/share}/
std::string get_xdg_user_data_home()
{
    std::string path;
#ifdef __APPLE__
    std::string home(getenv("HOME"));
    path = home + "/Library/Application Support";
#elif _WIN32
#if __STDC_WANT_SECURE_LIB__
    char *app_data_env = NULL;
    size_t app_data_env_sz = 0;
    _dupenv_s(&app_data_env, &app_data_env_sz, "LOCALAPPDATA");
    if (!app_data_env) _dupenv_s(&app_data_env, &app_data_env_sz, "APPDATA");
#else
    char *app_data_env = getenv("LOCALAPPDATA");
    if (!app_data_env) app_data_env = getenv("APPDATA");
#endif
    std::string app_data(app_data_env);
    path = app_data;
#elif defined(__DJGPP__)
    path = dos_home();
#else // Unix
    char *xdg_var = getenv("XDG_DATA_HOME");
    if (!xdg_var || !*xdg_var) {
        std::string xdg_default(getenv("HOME"));
        path = xdg_default + "/.local/share";
    } else {
        path = xdg_var;
    }
#endif
    return path + kFileSep;
}

