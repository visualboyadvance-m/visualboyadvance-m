// The online updater on wxSparkle (third_party/wxSparkle), the wxWidgets port
// of WinSparkle: the same appcast, settings location and C API on Windows,
// macOS, Linux and classic Mac OS. It is linked statically and its UI runs in
// this application's main thread.

#include "wx/autoupdater/autoupdater.h"

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/string.h>

#include <winsparkle.h>

#include "core/base/version.h"
#include "wx/android-compat.h"

#ifndef VBAM_UPDATER_EDDSA_PUBLIC_KEY
#define VBAM_UPDATER_EDDSA_PUBLIC_KEY ""
#endif

namespace {

#ifndef NO_HTTPS
constexpr char kAppcastUrl[] = "https://data.visualboyadvance-m.org/appcast.xml";
#else
constexpr char kAppcastUrl[] = "http://data.visualboyadvance-m.org/appcast.xml";
#endif

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
    // Even if we are a nightly, only check for the latest stable version.
    const wxString version(kVbamMainVersion);

    win_sparkle_set_appcast_url(kAppcastUrl);

    // The registry key (Windows) / WinSparkle.ini directory (elsewhere) is
    // <company>\<app>, as with the WinSparkle DLL before, so the user's
    // "check automatically" choice and last-check time carry over.
    win_sparkle_set_app_details(L"visualboyadvance-m", L"VisualBoyAdvance-M",
                                version.wc_str());

    if (VBAM_UPDATER_EDDSA_PUBLIC_KEY[0] != '\0')
        win_sparkle_set_eddsa_public_key(VBAM_UPDATER_EDDSA_PUBLIC_KEY);

    win_sparkle_set_can_shutdown_callback(CanShutdown);
    win_sparkle_set_shutdown_request_callback(RequestShutdown);
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
