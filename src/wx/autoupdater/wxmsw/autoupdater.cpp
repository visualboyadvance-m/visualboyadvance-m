#include "wx/autoupdater/autoupdater.h"

#include "core/base/version.h"
#include "wx/autoupdater/wxmsw/winsparkle-wrapper.h"

#if _WIN32_WINNT < 0x0600
#define NO_HTTPS 1
#endif

void initAutoupdater()
{
    // even if we are a nightly, only check latest stable version
    const wxString version(kVbamMainVersion);
#ifndef NO_HTTPS
    win_sparkle_set_appcast_url("https://data.visualboyadvance-m.org/appcast.xml");
#else
    win_sparkle_set_appcast_url("http://data.visualboyadvance-m.org/appcast.xml");
#endif // NO_HTTPS
    win_sparkle_set_app_details(L"visualboyadvance-m", L"VisualBoyAdvance-M", version.wc_str());
    win_sparkle_init();
}


void checkUpdatesUi()
{
    win_sparkle_check_update_with_ui();
}


void shutdownAutoupdater()
{
    win_sparkle_cleanup();
}
