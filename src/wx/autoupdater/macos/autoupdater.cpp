#include "wx/autoupdater/autoupdater.h"

#include "wx/autoupdater/macos/sparkle-wrapper.h"

SparkleWrapper autoupdater;

void initAutoupdater()
{
    autoupdater.addAppcastURL("https://data.visualboyadvance-m-emu.org/appcast.xml");
}


void checkUpdatesUi()
{
    autoupdater.checkForUpdatesUi();
}


void shutdownAutoupdater()
{
}
