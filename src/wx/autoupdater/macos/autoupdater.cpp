#include "../autoupdater.h"
#include "../../../common/version_cpp.h"
#include "../../strutils.h"
#include "sparkle-wrapper.h"

SparkleWrapper autoupdater;

void initAutoupdater()
{
    autoupdater.addAppcastURL("https://data.vba-m.com/appcast.xml");
}


void checkUpdatesUi()
{
    autoupdater.checkForUpdatesUi();
}


void shutdownAutoupdater()
{
}
