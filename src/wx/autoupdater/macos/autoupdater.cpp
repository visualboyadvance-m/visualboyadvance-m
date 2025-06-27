#include "wx/autoupdater/autoupdater.h"

#include "wx/autoupdater/macos/sparkle-wrapper.h"

extern bool is_macosx_1013_or_newer();

SparkleWrapper autoupdater;

void initAutoupdater()
{
    if (is_macosx_1013_or_newer())
        autoupdater.addAppcastURL("https://data.visualboyadvance-m.org/appcast.xml");
}


void checkUpdatesUi()
{
    if (is_macosx_1013_or_newer())
        autoupdater.checkForUpdatesUi();
}


void shutdownAutoupdater()
{
}
