/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "winsparkle.h"

#include "appcontroller.h"
#include "settings.h"
#include "error.h"
#include "ui.h"
#include "updatechecker.h"
#include "updatedownloader.h"

#include "utils.h"

#include <ctime>

using namespace winsparkle;

extern "C"
{

/*--------------------------------------------------------------------------*
                       Initialization and shutdown
 *--------------------------------------------------------------------------*/

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_init()
{
    try
    {
        // finish initialization
#ifdef _WIN32
        if (!Settings::GetLanguage().IsOk())
        {
            LANGID lang = 0;
            if (IsWindowsVistaOrGreater())
            {
                auto f_GetThreadUILanguage = LOAD_DYNAMIC_FUNC(GetThreadUILanguage, kernel32);
                if (f_GetThreadUILanguage)
                    lang = f_GetThreadUILanguage();
            }
            if (PRIMARYLANGID(lang) == 0)
            {
                lang = LANGIDFROMLCID(GetThreadLocale());
            }
            if (PRIMARYLANGID(lang) != 0)
                Settings::SetLanguage(lang);
        }
#endif // _WIN32
        // (elsewhere, the UI language is determined when the UI is first shown)

        // first things first
        UpdateDownloader::CleanLeftovers();

        // check for updates
        bool checkUpdates;
        if ( Settings::ReadConfigValue("CheckForUpdates", checkUpdates) )
        {
            if ( checkUpdates )
            {
                PeriodicUpdateChecker::StartGlobal();
            }
        }
        else // not yet configured
        {
            bool didRunOnce;
            Settings::ReadConfigValue("DidRunOnce", didRunOnce, false);
            if ( !didRunOnce )
            {
                // Do nothing on the first execution of the app, for better
                // first-time impression.
                Settings::WriteConfigValue("DidRunOnce", true);
            }
            else
            {
                // Only when the app is launched for the second time, ask the
                // user for their permission to check for updates.
                UI::AskForPermission();
            }
        }
    }
    CATCH_ALL_EXCEPTIONS
}


WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_cleanup()
{
    try
    {
        // stop periodic checks first, as they may still use the UI
        PeriodicUpdateChecker::StopGlobal();

        UI::ShutDown();

        // Note: one-shot update checks run in self-destructing threads; they
        // don't touch the UI after this point.
    }
    CATCH_ALL_EXCEPTIONS
}


/*--------------------------------------------------------------------------*
                              Language Settings
*--------------------------------------------------------------------------*/

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_lang(const char *lang)
{
    try
    {
        Settings::SetLanguage(lang);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_langid(unsigned short lang)
{
    try
    {
        Settings::SetLanguage(lang);
    }
    CATCH_ALL_EXCEPTIONS
}


/*--------------------------------------------------------------------------*
                               Configuration
 *--------------------------------------------------------------------------*/

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_appcast_url(const char *url)
{
    try
    {
        CheckForInsecureURL(url, "appcast feed");
        Settings::SetAppcastURL(url);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API int WIN_SPARKLE_CALL win_sparkle_set_dsa_pub_pem(const char *dsa_pub_pem)
{
    try
    {
        Settings::SetDSAPubKeyPem(dsa_pub_pem);
        return 1;
    }
    CATCH_ALL_EXCEPTIONS
    return 0;
}

WIN_SPARKLE_API int WIN_SPARKLE_CALL win_sparkle_set_eddsa_public_key(const char *pubkey)
{
    try
    {
        Settings::SetEdDSAPubKey(pubkey);
        return 1;
    }
    CATCH_ALL_EXCEPTIONS
    return 0;
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_app_details(const wchar_t *company_name,
                                                         const wchar_t *app_name,
                                                         const wchar_t *app_version)
{
    try
    {
        if ( company_name )
            Settings::SetCompanyName(company_name);
        if ( app_name )
            Settings::SetAppName(app_name);
        if ( app_version )
            Settings::SetAppVersion(app_version);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_http_header(const char *name, const char *value)
{
    try
    {
        Settings::SetHttpHeader(name, value);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_clear_http_headers()
{
    try
    {
        Settings::ClearHttpHeaders();
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_app_build_version(const wchar_t *build)
{
    try
    {
        Settings::SetAppBuildVersion(build);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_registry_path(const char *path)
{
    try
    {
        Settings::SetRegistryPath(path);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_config_methods(win_sparkle_config_methods_t *config_methods)
{
    try
    {
        Settings::SetConfigMethods(config_methods);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_automatic_check_for_updates(int state)
{
    try
    {
        Settings::WriteConfigValue("CheckForUpdates", state != 0);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API int WIN_SPARKLE_CALL win_sparkle_get_automatic_check_for_updates()
{
    try
    {
        bool checkUpdates;
        if ( Settings::ReadConfigValue("CheckForUpdates", checkUpdates, false) )
        return checkUpdates ? 1 : 0;
    }
    CATCH_ALL_EXCEPTIONS

    return 0;
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_check_interval(int interval)
{
    static const int MIN_CHECK_INTERVAL = 3600; // one hour

    try
    {
        if ( interval < MIN_CHECK_INTERVAL )
        {
            winsparkle::LogError("Invalid update interval (min: 3600 seconds)");
            interval = MIN_CHECK_INTERVAL;
        }

        Settings::WriteConfigValue("UpdateInterval", interval);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API int WIN_SPARKLE_CALL win_sparkle_get_update_check_interval()
{
    static const int DEFAULT_CHECK_INTERVAL = 60*60*24; // one day

    try
    {
        int interval;
        Settings::ReadConfigValue("UpdateInterval", interval, DEFAULT_CHECK_INTERVAL);
        return interval;
    }
    CATCH_ALL_EXCEPTIONS

    return DEFAULT_CHECK_INTERVAL;
}

WIN_SPARKLE_API time_t WIN_SPARKLE_CALL win_sparkle_get_last_check_time()
{
    static const time_t DEFAULT_LAST_CHECK_TIME = -1;

    try
    {
        time_t last_check;
        Settings::ReadConfigValue("LastCheckTime", last_check, DEFAULT_LAST_CHECK_TIME);
        return last_check;
    }
    CATCH_ALL_EXCEPTIONS

    return DEFAULT_LAST_CHECK_TIME;
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_error_callback(win_sparkle_error_callback_t callback)
{
    try
    {
        ApplicationController::SetErrorCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_can_shutdown_callback(win_sparkle_can_shutdown_callback_t callback)
{
    try
    {
        ApplicationController::SetCanShutdownCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_screen_area_callback(win_sparkle_screen_area_callback_t callback)
{
    try
    {
        UI::SetScreenAreaCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_shutdown_request_callback(win_sparkle_shutdown_request_callback_t callback)
{
    try
    {
        ApplicationController::SetShutdownRequestCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_did_find_update_callback(win_sparkle_did_find_update_callback_t callback)
{
    try
    {
        ApplicationController::SetDidFindUpdateCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_did_not_find_update_callback(win_sparkle_did_not_find_update_callback_t callback)
{
    try
    {
        ApplicationController::SetDidNotFindUpdateCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_cancelled_callback(win_sparkle_update_cancelled_callback_t callback)
{
    try
    {
        ApplicationController::SetUpdateCancelledCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_skipped_callback(win_sparkle_update_skipped_callback_t callback)
{
    try
    {
        ApplicationController::SetUpdateSkippedCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_postponed_callback(win_sparkle_update_postponed_callback_t callback)
{
    try
    {
        ApplicationController::SetUpdatePostponedCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_dismissed_callback(win_sparkle_update_dismissed_callback_t callback)
{
    try
    {
        ApplicationController::SetUpdateDismissedCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}

/*--------------------------------------------------------------------------*
                              Manual usage
 *--------------------------------------------------------------------------*/

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_check_update_with_ui()
{
    try
    {
        // Initialize UI thread and show progress indicator.
        UI::ShowCheckingUpdates();

        // Then run the actual check in the background.
        UpdateChecker *check = new ManualUpdateChecker();
        check->Start();
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_check_update_with_ui_and_install()
{
    try
    {
        // Initialize UI thread and show progress indicator.
        UI::ShowCheckingUpdates();

        // Then run the actual check in the background.
        UpdateChecker *check = new ManualAutoInstallUpdateChecker();
        check->Start();
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_check_update_without_ui()
{
    try
    {
        // Run the check in background. Only show UI if updates
        // are available.
        UpdateChecker *check = new OneShotUpdateChecker();
        check->Start();
    }
    CATCH_ALL_EXCEPTIONS
}

WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_user_run_installer_callback(win_sparkle_user_run_installer_callback_t callback)
{
    try
    {
        ApplicationController::SetUserRunInstallerCallback(callback);
    }
    CATCH_ALL_EXCEPTIONS
}


} // extern "C"
