#include "sparkle-wrapper.h"

#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>
#include <Sparkle/Sparkle.h>


class SparkleWrapper::Private
{
  public:
    SUUpdater* updater;
};


void SparkleWrapper::addAppcastURL(const char *appcastUrl)
{
    NSURL* url = [NSURL URLWithString: [NSString stringWithUTF8String: appcastUrl]];
    [d->updater setFeedURL: url];
}


SparkleWrapper::SparkleWrapper()
{
    d = new Private;
    d->updater = [SUUpdater sharedUpdater];
    [d->updater retain];
}


SparkleWrapper::~SparkleWrapper()
{
    [d->updater release];
    delete d;
}


void SparkleWrapper::checkForUpdatesUi()
{
    [d->updater checkForUpdates: NULL];
}
