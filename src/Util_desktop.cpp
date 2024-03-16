#include "Util.h"

#include <cstdlib>

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
    char *app_data_env = getenv("LOCALAPPDATA");
    if (!app_data_env) app_data_env = getenv("APPDATA");
    std::string app_data(app_data_env);
    path = app_data;
#else // Unix
    char *xdg_var = getenv("XDG_CONFIG_HOME");
    if (!xdg_var || !*xdg_var) {
        std::string xdg_default(getenv("HOME"));
        path = xdg_default + "/.config";
    } else {
        path = xdg_var;
    }
#endif
    return path + FILE_SEP;
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
    char *app_data_env = getenv("LOCALAPPDATA");
    if (!app_data_env) app_data_env = getenv("APPDATA");
    std::string app_data(app_data_env);
    path = app_data;
#else // Unix
    char *xdg_var = getenv("XDG_DATA_HOME");
    if (!xdg_var || !*xdg_var) {
        std::string xdg_default(getenv("HOME"));
        path = xdg_default + "/.local/share";
    } else {
        path = xdg_var;
    }
#endif
    return path + FILE_SEP;
}

