#include "../autoupdater.h"
#include "../../../common/version_cpp.h"
#include "../../strutils.h"
#include "winsparkle-wrapper.h"


void initAutoupdater()
{
    // even if we are a nightly, only check latest stable version
    wxString version = strutils::split(vbam_version, '-')[0];
#ifndef NO_HTTPS
    win_sparkle_set_appcast_url("https://data.vba-m.com/appcast.xml");
#else
    win_sparkle_set_appcast_url("http://data.vba-m.com/appcast.xml");
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
