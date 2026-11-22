/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _winsparkle_h_
#define _winsparkle_h_

#include <stddef.h>
#include <time.h>

#include "winsparkle-version.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    wxSparkle: the library may be built either as a shared library (DLL) or
    as a static library. Define WIN_SPARKLE_STATIC when using the static
    library on Windows (the CMake package config does this automatically).
 */
#if defined(WIN_SPARKLE_STATIC)
    #define WIN_SPARKLE_API
#elif defined(_WIN32)
    #ifdef BUILDING_WIN_SPARKLE
        #define WIN_SPARKLE_API __declspec(dllexport)
    #else
        #define WIN_SPARKLE_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define WIN_SPARKLE_API __attribute__((visibility("default")))
#else
    #define WIN_SPARKLE_API
#endif

/// Calling convention used by all public functions and callbacks.
#ifndef WIN_SPARKLE_CALL
    #if defined(_WIN32) && (defined(_MSC_VER) || defined(__i386__) || defined(__x86_64__))
        #define WIN_SPARKLE_CALL __cdecl
    #else
        #define WIN_SPARKLE_CALL
    #endif
#endif

#if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(deprecated)
        #define WIN_SPARKLE_DEPRECATED(msg) [[deprecated(msg)]]
    #endif
#endif
#ifndef WIN_SPARKLE_DEPRECATED
    #if defined(_MSC_VER)
        #define WIN_SPARKLE_DEPRECATED(msg) __declspec(deprecated("deprecated: " msg))
    #elif defined(__GNUC__) || defined(__clang__)
        #define WIN_SPARKLE_DEPRECATED(msg) __attribute__((deprecated(msg)))
    #else
        #define WIN_SPARKLE_DEPRECATED(msg)
    #endif
#endif


/// Return value for boolean functions to indicate unexpected error.
/// Only used by functions or callbacks that are explicitly documented as using it.
#define WINSPARKLE_RETURN_ERROR (-1)


/*--------------------------------------------------------------------------*
                       Initialization and shutdown
 *--------------------------------------------------------------------------*/

/**
    @name Initialization functions
 */
//@{

/**
    Starts WinSparkle.

    If WinSparkle is configured to check for updates on startup, proceeds
    to perform the check. You should only call this function when your app
    is initialized and shows its main window.

    @note This call doesn't block and returns almost immediately. If an
          update is available, the corresponding UI is shown later from a separate
          thread.

    @see win_sparkle_cleanup()
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_init();

/**
    Cleans up after WinSparkle.

    Should be called by the app when it's shutting down. Cancels any
    pending Sparkle operations and shuts down its helper threads.
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_cleanup();

//@}


/*--------------------------------------------------------------------------*
                             Language settings
*--------------------------------------------------------------------------*/

/**
@name Language settings

These functions set the user interface language. They must be called before
win_sparkle_init() to have any effect. If none of them is called, WinSparkle
detects the UI language automatically.
*/
//@{

/**
    Sets UI language from its ISO code.

    This function must be called before win_sparkle_init().

    @param lang ISO 639 language code with an optional ISO 3166 country
                code, e.g. "fr", "pt-PT", "pt-BR" or "pt_BR", as used
                e.g. by ::GetThreadPreferredUILanguages() too.

    @since 0.5

    @see win_sparkle_set_langid()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_lang(const char *lang);

/**
    Sets UI language from its Win32 LANGID code.

    This function must be called before win_sparkle_init().

    @param lang Language code (LANGID) as created by the MAKELANGID macro
                or returned by e.g. ::GetThreadUILanguage()

    @since 0.5

    @see win_sparkle_set_lang()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_langid(unsigned short lang);

//@}

/*--------------------------------------------------------------------------*
                               Configuration
 *--------------------------------------------------------------------------*/

/**
    @name Configuration functions

    Functions for setting up WinSparkle.

    All functions in this category can only be called @em before the first
    call to win_sparkle_init()!

    Typically, the application would configure WinSparkle on startup and then
    call win_sparkle_init(), all from its main thread.
 */
//@{

/**
    Sets URL for the app's appcast.

    Only http and https schemes are supported.

    If this function isn't called by the app, the URL is obtained from
    Windows resource named "FeedURL" of type "APPCAST" (on Windows) or from
    the SUFeedURL key of the main bundle's Info.plist (on macOS).

    @param url  URL of the appcast.

    @note Always use HTTPS feeds, do not use unencrypted HTTP! This is
          necessary to prevent both leaking user information and preventing
          various MITM attacks.

    @note See https://github.com/vslavik/winsparkle/wiki/Appcast-Feeds for
          more information about appcast feeds.
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_appcast_url(const char *url);

/**
    Sets DSA public key.

    Only PEM format is supported.

    Public key will be used to verify DSA signature of the update file.
    PEM data will be set only if it contains a valid DSA public key.

    If this function isn't called by the app, public key is obtained from
    Windows resource named "DSAPub" of type "DSAPEM".

    @deprecated
    DSA signatures are deprecated and will be removed in a future version.
    Migrate over to EdDSA (ed25519), see
    https://github.com/vslavik/winsparkle/wiki/Upgrading-from-DSA-to-EdDSA-signatures

    @param dsa_pub_pem  DSA public key in PEM format.

    @return  1 if a valid DSA public key is provided, 0 otherwise.

    @since 0.6.0

    @see win_sparkle_set_eddsa_public_key()
 */
WIN_SPARKLE_DEPRECATED("use EdDSA signatures and win_sparkle_set_eddsa_public_key() instead")
WIN_SPARKLE_API int WIN_SPARKLE_CALL win_sparkle_set_dsa_pub_pem(const char *dsa_pub_pem);

/**
    Sets EdDSA public key.

    Only base64-encoded format is supported.

    Public key will be used to verify EdDSA signature of the update file.
    It will be set only if it contains a valid EdDSA public key.

    If this function isn't called by the app, public key is obtained from
    Windows resource named "EdDSAPub" of type "EDDSA" (on Windows) or from
    the SUPublicEDKey key of the main bundle's Info.plist (on macOS).

    @note
    If this function is called, DSA public key set with win_sparkle_set_dsa_pub_pem()
    or present in the resources will be ignored; so will DSA signatures in the appcast.

    @param pubkey  EdDSA public key in base64 encoded format.

    @return  1 if a valid EdDSA public key is provided, 0 otherwise.

    @since 0.9.0
 */
WIN_SPARKLE_API int WIN_SPARKLE_CALL win_sparkle_set_eddsa_public_key(const char *pubkey);

/**
    Sets application metadata.

    Normally, these are taken from VERSIONINFO/StringFileInfo resources
    (Windows) or the main bundle's Info.plist (macOS), but if your application
    doesn't use them for some reason, using this function is an alternative.

    On other platforms (e.g. Linux), the application name and vendor default
    to wxApp::GetAppDisplayName() and wxApp::GetVendorName() of the host
    wxWidgets application, but the version must be set with this function.

    @param company_name  Company name of the vendor.
    @param app_name      Application name. This is both shown to the user
                         and used in HTTP User-Agent header.
    @param app_version   Version of the app, as a string (e.g. "1.2" or "1.2rc1").

    @note @a company_name and @a app_name are used to determine the location
          of WinSparkle settings in the registry.
          (HKCU\Software\<company_name>\<app_name>\WinSparkle is used.)

    @since 0.3

    @see win_sparkle_set_app_build_version();
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_app_details(const wchar_t *company_name,
                                                         const wchar_t *app_name,
                                                         const wchar_t *app_version);

/**
    Sets application build version number.

    This is the internal version number that is not normally shown to the user.
    It can be used for finer granularity than official release versions, e.g. for
    interim builds.

    If this function is called, then the provided *build* number is used for comparing
    versions; it is compared to the "version" attribute in the appcast and corresponds
    to macOS Sparkle's CFBundleVersion handling. If used, then the appcast must
    also contain the "shortVersionString" attribute with a human-readable display
    version string. The version passed to win_sparkle_set_app_details()
    corresponds to this and is used for display.

    @since 0.4

    @see win_sparkle_set_app_details()
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_app_build_version(const wchar_t *build);

/**
    Sets a custom HTTP header for appcast checks and update downloads.

    @since 0.7

    @see win_sparkle_clear_http_headers()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_http_header(const char *name, const char *value);

/**
    Clears all custom HTTP headers previously added using
    win_sparkle_set_http_header().

    @since 0.7

    @see win_sparkle_set_http_header()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_clear_http_headers();

/**
    Sets the registry path where settings will be stored.

    Normally, these are stored in
    "HKCU\Software\<company_name>\<app_name>\WinSparkle"
    but if your application needs to store the data elsewhere for
    some reason, using this function is an alternative.

    On platforms without a registry (macOS, Linux), the path is mapped to an
    INI file: the leading "Software\" component is dropped and the rest is
    used as a relative path inside the per-user configuration directory
    (~/Library/Application Support on macOS, $XDG_CONFIG_HOME or ~/.config
    elsewhere), with ".ini" appended. For example the default path maps to
    "~/.config/<company_name>/<app_name>/WinSparkle.ini". A system-wide file
    in /etc/xdg (Linux) or /Library/Application Support (macOS) is consulted
    as a read-only fallback, analogous to HKLM. An absolute path (starting
    with '/') is used as the INI file's path directly.

    Note that @a path is relative to HKCU/HKLM root and the root is not part
    of it. For example:
    @code
    win_sparkle_set_registry_path("Software\\My App\\Updates");
    @endcode

    @param path  Registry path where settings will be stored.

    @since 0.3
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_registry_path(const char *path);

/// Type used to override WinSparkle's configuration read, write and delete functions
typedef struct win_sparkle_config_methods_tag {
    /// Copies the config value named @a name to the buffer pointed to by @a buf; returns TRUE on success, FALSE on failure
    int(WIN_SPARKLE_CALL *config_read)(const char *name, wchar_t *buf, size_t len, void *user_data);
    /// Writes @a value as the new value of the config value named @a name
    void(WIN_SPARKLE_CALL *config_write)(const char *name, const wchar_t *value, void *user_data);
    /// Deletes the config value named @a name
    void(WIN_SPARKLE_CALL *config_delete)(const char *name, void *user_data);
    /// Arbitrary data that will be passed to the above functions; WinSparkle will not read or alter it.
    void *user_data;
} win_sparkle_config_methods_t;


/**
    Override WinSparkle's configuration read, write and delete functions.

    By default, WinSparkle will read, write and delete configuration values by
    interacting directly with the Windows Registry.
    If you want to manage configuration yourself, or if you don't want to let WinSparkle
    write settings directly to the Windows Registry, you can provide your own functions
    to read, write and delete configuration.

    If any configuration action's function pointer (read, write or delete) is NULL,
    WinSparkle will use the default function for that action.

    @param config_methods  Your own configuration read, write and delete functions.
                           Pass NULL to let WinSparkle continue to use its default functions.

    @note There's no guarantee about the thread from which these functions are called.
          Make sure your functions are thread-safe.

    @since 0.7
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_config_methods(win_sparkle_config_methods_t *config_methods);

/**
    Sets whether updates are checked automatically or only through a manual call.

    If disabled, win_sparkle_check_update_with_ui() must be used explicitly.

    @param  state  1 to have updates checked automatically, 0 otherwise

    @since 0.4
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_automatic_check_for_updates(int state);

/**
    Gets the automatic update checking state.

    @return  1 if updates are set to be checked automatically, 0 otherwise

    @note Defaults to 0 when not yet configured (as happens on first start).

    @since 0.4
 */
WIN_SPARKLE_API int WIN_SPARKLE_CALL win_sparkle_get_automatic_check_for_updates();

/**
    Sets the automatic update interval.

    @param  interval The interval in seconds between checks for updates.
                     The minimum update interval is 3600 seconds (1 hour).

    @since 0.4
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_check_interval(int interval);

/**
    Gets the automatic update interval in seconds.

    Default value is one day.

    @since 0.4
 */
WIN_SPARKLE_API int WIN_SPARKLE_CALL win_sparkle_get_update_check_interval();

/**
    Gets the time of the last update check.

    Default value is -1, indicating that the update check has never run.

    @since 0.4
*/
WIN_SPARKLE_API time_t WIN_SPARKLE_CALL win_sparkle_get_last_check_time();

/// Callback type for win_sparkle_error_callback()
typedef void (WIN_SPARKLE_CALL *win_sparkle_error_callback_t)();

/**
    Sets a callback to be called when the updater encounters an error.

    @since 0.5
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_error_callback(win_sparkle_error_callback_t callback);

/// Callback type for win_sparkle_can_shutdown_callback()
typedef int (WIN_SPARKLE_CALL *win_sparkle_can_shutdown_callback_t)();

/**
    Sets a callback for querying the application if it can be closed.

    This callback will be called to ask the host if it's ready to shut down,
    before attempting to launch the installer. The callback returns TRUE if
    the host application can be safely shut down or FALSE if not (e.g. because
    the user has unsaved documents).

    @note There's no guarantee about the thread from which the callback is called,
          except that it certainly *won't* be called from the app's main thread.
          Make sure the callback is thread-safe.

    @since 0.4

    @see win_sparkle_set_shutdown_request_callback()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_can_shutdown_callback(win_sparkle_can_shutdown_callback_t callback);


/// Callback type for win_sparkle_shutdown_request_callback()
typedef void (WIN_SPARKLE_CALL *win_sparkle_shutdown_request_callback_t)();

/**
    Sets a callback for shutting down the application.

    This callback will be called to ask the host to shut down immediately after
    launching the installer. Its implementation should gracefully terminate the
    application.

    It will only be called if the call to the callback set with
    win_sparkle_set_can_shutdown_callback() returns TRUE.

    @note There's no guarantee about the thread from which the callback is called,
          except that it certainly *won't* be called from the app's main thread.
          Make sure the callback is thread-safe.

    @since 0.4

    @see win_sparkle_set_can_shutdown_callback()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_shutdown_request_callback(win_sparkle_shutdown_request_callback_t);

/// Callback type for win_sparkle_did_find_update_callback()
typedef void(WIN_SPARKLE_CALL *win_sparkle_did_find_update_callback_t)();

/**
    Sets a callback to be called when the updater finds an update.

    This is useful in combination with
    win_sparkle_check_update_with_ui_and_install() as it allows you to perform
    some action after WinSparkle checks for updates.

    @since 0.5

    @see win_sparkle_did_not_find_update_callback()
    @see win_sparkle_check_update_with_ui_and_install()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_did_find_update_callback(win_sparkle_did_find_update_callback_t callback);

/// Callback type for win_sparkle_did_not_find_update_callback()
typedef void (WIN_SPARKLE_CALL *win_sparkle_did_not_find_update_callback_t)();

/**
    Sets a callback to be called when the updater does not find an update.

    This is useful in combination with
    win_sparkle_check_update_with_ui_and_install() as it allows you to perform
    some action after WinSparkle checks for updates.

    @since 0.5

    @see win_sparkle_did_find_update_callback()
    @see win_sparkle_check_update_with_ui_and_install()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_did_not_find_update_callback(win_sparkle_did_not_find_update_callback_t callback);

/// Callback type for win_sparkle_update_cancelled_callback()
typedef void (WIN_SPARKLE_CALL *win_sparkle_update_cancelled_callback_t)();

/**
    Sets a callback to be called when the user cancels an update, e.g.
    by closing the window, skipping an update or cancelling a download.
    This callback is not called when there's no update to install or an error occurs.

    This is useful in combination with
    win_sparkle_check_update_with_ui_and_install() as it allows you to perform
    some action when the installation is interrupted.

    @since 0.5

    @see win_sparkle_check_update_with_ui_and_install(), win_sparkle_set_update_dismissed_callback()
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_cancelled_callback(win_sparkle_update_cancelled_callback_t callback);

/// Callback type for win_sparkle_update_skipped_callback()
typedef void(WIN_SPARKLE_CALL* win_sparkle_update_skipped_callback_t)();

/**
    Sets a callback to be called when the user skips an update.

    This is useful in combination with
    win_sparkle_check_update_with_ui() or similar functions, as it
    allows you to perform some action when the update
    is skipped.

    @see win_sparkle_check_update_with_ui_and_install()

    @since 0.8
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_skipped_callback(win_sparkle_update_skipped_callback_t callback);

/// Callback type for win_sparkle_update_postponed_callback()
typedef void(WIN_SPARKLE_CALL* win_sparkle_update_postponed_callback_t)();

/**
    Sets a callback to be called when the user postpones an update
    (by pressing the 'remind me later' button).

    This is useful in combination with
    win_sparkle_check_update_with_ui() or similar functions, as it
    allows you to perform some action when the download
    is postponed.

    @see win_sparkle_check_update_with_ui()

    @since 0.8
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_postponed_callback(win_sparkle_update_postponed_callback_t callback);

/// Callback type for win_sparkle_update_dismissed_callback()
typedef void(WIN_SPARKLE_CALL* win_sparkle_update_dismissed_callback_t)();

/**
    Sets a callback to be called when the user dismisses
    (closes) the update dialog, including when there were no updates or an error occurred.
    See win_sparkle_set_update_cancelled_callback() for a subtly different callback
    that may be more appropriate.

    This is useful in combination with
    win_sparkle_check_update_with_ui() or similar functions, as it
    allows you to perform some action when the update
    dialog is closed.

    @see win_sparkle_check_update_with_ui(), win_sparkle_set_update_cancelled_callback()

    @since 0.8
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_update_dismissed_callback(win_sparkle_update_dismissed_callback_t callback);


/// Callback type for win_sparkle_user_run_installer_callback()
typedef int(WIN_SPARKLE_CALL* win_sparkle_user_run_installer_callback_t)(const wchar_t *);

/**
    Sets a callback to be called when the update payload is
    downloaded and ready to be executed or handled in some
    other manner.

    The callback returns:
    - 1 if the file was handled by the callback
    - 0 if it was not, in which case WinSparkle's default handling will take place
    - WINSPARKLE_RETURN_ERROR in case of an error

    @since 0.8
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_user_run_installer_callback(win_sparkle_user_run_installer_callback_t callback);


/// Callback type for win_sparkle_set_screen_area_callback()
typedef int(WIN_SPARKLE_CALL* win_sparkle_screen_area_callback_t)(int *x, int *y, int *width, int *height);

/**
    Sets a callback that tells WinSparkle the part of the screen its windows
    may take, in the same coordinates as the windows' screen positions.

    It is only used where the dialogs fill the screen (Android), where the
    system's idea of the screen's client area can be larger than what the
    application actually gets: e.g. Qt for Android does not leave out the
    activity's action bar, so a dialog sized to it hangs off the bottom of
    the screen, buttons and all.

    The callback is called on the UI thread and returns:
    - 1 if it filled in the area, which then limits the dialogs' size
    - 0 if it doesn't know it (yet), in which case the default is used

    @since wxSparkle
*/
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_set_screen_area_callback(win_sparkle_screen_area_callback_t callback);

//@}


/*--------------------------------------------------------------------------*
                              Manual usage
 *--------------------------------------------------------------------------*/

/**
    @name Manually using WinSparkle
 */
//@{

/**
    Checks if an update is available, showing a progress UI to the user.

    Normally, WinSparkle checks for updates on startup and only shows its UI
    when it finds an update. If the application disables this behavior, it
    can hook this function to a "Check for updates..." menu item.

    When called, a background thread is started to check for updates. A small
    window is shown to let the user know about the progress. If no update is found,
    the user is told so. If there is an update, the usual "update available"
    window is shown.

    This function returns immediately.

    @note Because this function is intended for manual, user-initiated checks
          for updates, it ignores "Skip this version" even if the user checked
          it previously.

    @see win_sparkle_check_update_without_ui()
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_check_update_with_ui();

/**
    Checks if an update is available, showing a progress UI to the user and
    immediately installing the update if one is available.

    This is useful for the case when users should almost always use the
    newest version of your software. When called, WinSparkle will check for
    updates while showing a progress UI to the user. If an update is found, the update
    prompt will be skipped and the update will be installed immediately.

    If your application expects to do something after checking for updates, you
    may wish to use win_sparkle_set_did_not_find_update_callback() and
    win_sparkle_set_update_cancelled_callback().

    @since 0.5

    @see win_sparkle_set_did_find_update_callback()
    @see win_sparkle_set_update_cancelled_callback()
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_check_update_with_ui_and_install();

/**
    Checks if an update is available.

    No progress UI is shown to the user when checking. If an update is
    available, the usual "update available" window is shown; this function
    is *not* completely UI-less.

    Use with caution, it usually makes more sense to use the automatic update
    checks or a manual check with visible UI.

    This function returns immediately.

    @note This function respects "Skip this version" choice by the user.

    @since 0.4

    @see win_sparkle_check_update_with_ui()
 */
WIN_SPARKLE_API void WIN_SPARKLE_CALL win_sparkle_check_update_without_ui();

//@}

#ifdef __cplusplus
}
#endif

#endif // _winsparkle_h_
