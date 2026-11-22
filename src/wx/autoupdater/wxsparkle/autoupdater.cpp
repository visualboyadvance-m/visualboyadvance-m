// The online updater on wxSparkle (third_party/wxSparkle), the wxWidgets port
// of WinSparkle: the same appcast, settings location and C API on Windows,
// macOS, Linux and classic Mac OS. It is linked statically and its UI runs in
// this application's main thread.
//
// A stable build (VBAM_UPDATER_CHANNEL, see src/wx/CMakeLists.txt) checks
// appcast.xml; a nightly build (VBAM_UPDATER_NIGHTLY) checks for this
// platform's file on the nightly server, whose directory listing has no feed:
// wxSparkle takes the file's date there as its version, verifies it against
// the file's Last-Modified, and compares it with the date of the file this
// build was installed from, or else with the build time
// (VBAM_UPDATER_BUILD_TIME) plus the time the nightly takes to be uploaded
// (VBAM_UPDATER_NIGHTLY_GRACE).

#include "wx/autoupdater/autoupdater.h"

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/string.h>

#include <winsparkle.h>

#include "core/base/version.h"
#include "wx/android-compat.h"

#ifdef VBAM_INPLACE_UPDATE
#include "wx/autoupdater/wxsparkle/inplace-update.h"
#endif

#ifndef VBAM_UPDATER_EDDSA_PUBLIC_KEY
#define VBAM_UPDATER_EDDSA_PUBLIC_KEY ""
#endif

namespace {

#ifndef NO_HTTPS
constexpr char kAppcastUrl[] = "https://data.visualboyadvance-m.org/appcast.xml";
#else
constexpr char kAppcastUrl[] = "http://data.visualboyadvance-m.org/appcast.xml";
#endif

#ifdef VBAM_UPDATER_NIGHTLY
#ifndef NO_HTTPS
constexpr char kNightlyUrl[] = "https://nightly.visualboyadvance-m.org/";
#else
constexpr char kNightlyUrl[] = "http://nightly.visualboyadvance-m.org/";
#endif

// This platform's file on the nightly server; nullptr where there is none,
// which checks appcast.xml instead.
#if defined(__ANDROID__)
#if defined(__aarch64__)
constexpr const char* kNightlyFile = "visualboyadvance-m-ARM64.apk";
#elif defined(__arm__)
constexpr const char* kNightlyFile = "visualboyadvance-m-ARM32.apk";
#elif defined(__x86_64__)
constexpr const char* kNightlyFile = "visualboyadvance-m-x86_64.apk";
#elif defined(__i386__)
constexpr const char* kNightlyFile = "visualboyadvance-m-x86_32.apk";
#elif defined(__riscv) && __riscv_xlen == 64
constexpr const char* kNightlyFile = "visualboyadvance-m-RISCV64.apk";
#else
constexpr const char* kNightlyFile = nullptr;
#endif
#elif defined(_WIN32)
#if defined(_M_ARM64) || defined(__aarch64__)
constexpr const char* kNightlyFile = "visualboyadvance-m-Win-ARM64.zip";
#elif defined(_M_X64) || defined(__x86_64__)
constexpr const char* kNightlyFile = "visualboyadvance-m-Win-x86_64.zip";
#elif defined(_M_IX86) || defined(__i386__)
constexpr const char* kNightlyFile = "visualboyadvance-m-Win-x86_32.zip";
#else
constexpr const char* kNightlyFile = nullptr;
#endif
#elif defined(__APPLE__) && !defined(__WXRETRO68__) && !defined(macintosh)
#if defined(__aarch64__)
constexpr const char* kNightlyFile = "visualboyadvance-m-Mac-ARM64.zip";
#elif defined(__x86_64__)
constexpr const char* kNightlyFile = "visualboyadvance-m-Mac-x86_64.zip";
#else
constexpr const char* kNightlyFile = nullptr;
#endif
#else
constexpr const char* kNightlyFile = nullptr;
#endif
#endif  // VBAM_UPDATER_NIGHTLY

// Called from a wxSparkle worker thread once the installer is running.
int WIN_SPARKLE_CALL CanShutdown() {
    return 1;
}

void WIN_SPARKLE_CALL RequestShutdown() {
    if (!wxTheApp)
        return;

    wxTheApp->CallAfter([] {
        wxWindow* top = wxTheApp->GetTopWindow();
        if (top)
            top->Close();
        else
            wxTheApp->ExitMainLoop();
    });
}

#if defined(__WXQT__) && defined(__ANDROID__)
// The part of the screen the updater's dialogs may fill: from the main window's
// origin, as large as the activity's content view. wxSparkle would otherwise go
// by the screen's client area and the main window, which Qt makes taller than
// what is on screen by the action bar, leaving the dialogs' buttons off the
// bottom of the screen.
int WIN_SPARKLE_CALL ScreenArea(int* x, int* y, int* width, int* height) {
    int w = 0, h = 0;
    if (!VbamAndroidScreenClientSize(&w, &h))
        return 0;

    wxPoint origin;
    if (wxTheApp) {
        if (const wxWindow* top = wxTheApp->GetTopWindow())
            origin = top->GetScreenPosition();
    }

    *x = origin.x;
    *y = origin.y;
    *width = w;
    *height = h;
    return 1;
}
#endif

}  // namespace

void initAutoupdater() {
    bool nightly = false;
#ifdef VBAM_UPDATER_NIGHTLY
    nightly = kNightlyFile != nullptr;
#endif

    // A stable build compares its version with the appcast's; a nightly goes
    // by dates, and shows its full version (with the commit) in the dialog.
    const wxString version(nightly ? kVbamVersion : kVbamMainVersion);

    win_sparkle_set_appcast_url(kAppcastUrl);
#ifdef VBAM_UPDATER_NIGHTLY
    if (nightly) {
        win_sparkle_set_directory_feed(kNightlyUrl, kNightlyFile);
        win_sparkle_set_app_build_time(static_cast<time_t>(VBAM_UPDATER_BUILD_TIME),
                                       VBAM_UPDATER_NIGHTLY_GRACE);
    }
#endif

    // The registry key (Windows) / WinSparkle.ini directory (elsewhere) is
    // <company>\<app>, as with the WinSparkle DLL before, so the user's
    // "check automatically" choice and last-check time carry over.
    win_sparkle_set_app_details(L"visualboyadvance-m", L"VisualBoyAdvance-M",
                                version.wc_str());

    // The nightly server's files are not signed: with a key set, wxSparkle
    // would turn every one of them down.
    if (!nightly && VBAM_UPDATER_EDDSA_PUBLIC_KEY[0] != '\0')
        win_sparkle_set_eddsa_public_key(VBAM_UPDATER_EDDSA_PUBLIC_KEY);

    win_sparkle_set_can_shutdown_callback(CanShutdown);
    win_sparkle_set_shutdown_request_callback(RequestShutdown);
#ifdef VBAM_INPLACE_UPDATE
    // Windows and macOS: replace this installation and restart instead of
    // opening the downloaded zip (inplace-update.h); 0 from it falls back to
    // opening the zip.
    win_sparkle_set_user_run_installer_callback(InPlaceUpdateRunInstaller);
#endif
#if defined(__WXQT__) && defined(__ANDROID__)
    win_sparkle_set_screen_area_callback(ScreenArea);
#endif

    win_sparkle_init();
}

void checkUpdatesUi() {
    win_sparkle_check_update_with_ui();
}

void shutdownAutoupdater() {
    win_sparkle_cleanup();
}
