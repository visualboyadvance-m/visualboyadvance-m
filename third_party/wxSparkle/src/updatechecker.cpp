/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "updatechecker.h"
#include "appcast.h"
#include "ui.h"
#include "error.h"
#include "settings.h"
#include "download.h"
#include "utils.h"

#include <ctime>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <winsparkle.h>

using namespace std;

namespace winsparkle
{

/*--------------------------------------------------------------------------*
                              version comparison
 *--------------------------------------------------------------------------*/

// Note: This code is based on Sparkle's SUStandardVersionComparator by
//       Andy Matuschak.

namespace
{

// String characters classification. Valid components of version numbers
// are numbers, period or string fragments ("beta" etc.).
enum CharType
{
    Type_Number,
    Type_Period,
    Type_String
};

CharType ClassifyChar(char c)
{
    if ( c == '.' )
        return Type_Period;
    else if ( c >= '0' && c <= '9' )
        return Type_Number;
    else
        return Type_String;
}

// Split version string into individual components. A component is continuous
// run of characters with the same classification. For example, "1.20rc3" would
// be split into ["1",".","20","rc","3"].
vector<string> SplitVersionString(const string& version)
{
    vector<string> list;

    if ( version.empty() )
        return list; // nothing to do here

    string s;
    const size_t len = version.length();

    s = version[0];
    CharType prevType = ClassifyChar(version[0]);

    for ( size_t i = 1; i < len; i++ )
    {
        const char c = version[i];
        const CharType newType = ClassifyChar(c);

        if ( prevType != newType || prevType == Type_Period )
        {
            // We reached a new segment. Period gets special treatment,
            // because "." always delimiters components in version strings
            // (and so ".." means there's empty component value).
            list.push_back(s);
            s = c;
        }
        else
        {
            // Add character to current segment and continue.
            s += c;
        }

        prevType = newType;
    }

    // Don't forget to add the last part:
    list.push_back(s);

    return list;
}

} // anonymous namespace


int UpdateChecker::CompareVersions(const string& verA, const string& verB)
{
    const vector<string> partsA = SplitVersionString(verA);
    const vector<string> partsB = SplitVersionString(verB);

    // Compare common length of both version strings.
    const size_t n = min(partsA.size(), partsB.size());
    for ( size_t i = 0; i < n; i++ )
    {
        const string& a = partsA[i];
        const string& b = partsB[i];

        const CharType typeA = ClassifyChar(a[0]);
        const CharType typeB = ClassifyChar(b[0]);

        if ( typeA == typeB )
        {
            if ( typeA == Type_String )
            {
                int result = a.compare(b);
                if ( result != 0 )
                    return result;
            }
            else if ( typeA == Type_Number )
            {
                const int intA = atoi(a.c_str());
                const int intB = atoi(b.c_str());
                if ( intA > intB )
                    return 1;
                else if ( intA < intB )
                    return -1;
            }
        }
        else // components of different types
        {
            if ( typeA != Type_String && typeB == Type_String )
            {
                // 1.2.0 > 1.2rc1
                return 1;
            }
            else if ( typeA == Type_String && typeB != Type_String )
            {
                // 1.2rc1 < 1.2.0
                return -1;
            }
            else
            {
                // One is a number and the other is a period. The period
                // is invalid.
                return (typeA == Type_Number) ? 1 : -1;
            }
        }
    }

    // The versions are equal up to the point where they both still have
    // parts. Lets check to see if one is larger than the other.
    if ( partsA.size() == partsB.size() )
        return 0; // the two strings are identical

    // Lets get the next part of the larger version string
    // Note that 'n' already holds the index of the part we want.

    int shorterResult, longerResult;
    CharType missingPartType; // ('missing' as in "missing in shorter version")

    if ( partsA.size() > partsB.size() )
    {
        missingPartType = ClassifyChar(partsA[n][0]);
        shorterResult = -1;
        longerResult = 1;
    }
    else
    {
        missingPartType = ClassifyChar(partsB[n][0]);
        shorterResult = 1;
        longerResult = -1;
    }

    if ( missingPartType == Type_String )
    {
        // 1.5 > 1.5b3
        return shorterResult;
    }
    else
    {
        // 1.5.1 > 1.5
        return longerResult;
    }
}


/*--------------------------------------------------------------------------*
                             UpdateChecker::Run()
 *--------------------------------------------------------------------------*/

UpdateChecker::UpdateChecker(): Thread("WinSparkle updates check")
{
}

namespace
{

// Config values for directory feeds (see win_sparkle_set_directory_feed()):
// the listing time of the file the user was given to install, and the build
// time of the application that gave it; once a build with another build
// time runs, the install happened and these become the listing time of the
// file the running build came from, and that build's time.
const char *const kPendingTime      = "DirectoryFeedPendingTime";
const char *const kPendingFromBuild = "DirectoryFeedPendingFromBuild";
const char *const kInstalledTime    = "DirectoryFeedInstalledTime";
const char *const kInstalledBuild   = "DirectoryFeedInstalledBuild";

// The listing time of the file the running build came from, or the latest
// it can have been put on the server; listed files after it are newer.
time_t GetInstalledListingTime()
{
    const time_t buildTime = Settings::GetAppBuildTime();
    if ( buildTime == time_t(-1) )
        return 0; // unknown: every listed file is newer

    time_t pending = 0, pendingFrom = 0;
    if ( Settings::ReadConfigValue(kPendingTime, pending) &&
         Settings::ReadConfigValue(kPendingFromBuild, pendingFrom) &&
         pendingFrom != buildTime )
    {
        Settings::WriteConfigValue(kInstalledTime, pending);
        Settings::WriteConfigValue(kInstalledBuild, buildTime);
        Settings::DeleteConfigValue(kPendingTime);
        Settings::DeleteConfigValue(kPendingFromBuild);
    }

    time_t installed = 0, installedBuild = 0;
    if ( Settings::ReadConfigValue(kInstalledTime, installed) &&
         Settings::ReadConfigValue(kInstalledBuild, installedBuild) &&
         installedBuild == buildTime )
    {
        return installed;
    }

    return buildTime + Settings::GetAppBuildTimeGrace();
}

// Throws unless the server's Last-Modified of the update file is in the
// minute (or second) its listing shows.
void VerifyListingTime(const Appcast& appcast, Thread *thread)
{
    const std::string& url = appcast.enclosure.DownloadURL;

    StringDownloadSink head;
    DownloadFile(url, &head, thread, Settings::GetHttpHeadersString(),
                 Download_BypassProxies | Download_HeadOnly);

    if ( head.fileTime == time_t(-1) )
    {
        throw std::runtime_error("The server did not give the date of " + url +
                                 ", so the update could not be verified.");
    }

    if ( head.fileTime < appcast.ListingTime || head.fileTime >= appcast.ListingTime + 60 )
    {
        throw std::runtime_error("The date of " + url + " on the server (" +
                                 detail::FormatUTCTime(head.fileTime) +
                                 " UTC) does not match its date in the listing (" +
                                 appcast.ShortVersionString + ").");
    }
}

} // anonymous namespace


void UpdateChecker::RecordDirectoryFeedInstall(const Appcast& appcast)
{
    if ( appcast.ListingTime == 0 )
        return;

    Settings::WriteConfigValue(kPendingTime, appcast.ListingTime);
    Settings::WriteConfigValue(kPendingFromBuild, Settings::GetAppBuildTime());
}


void UpdateChecker::PerformUpdateCheck()
{
    try
    {
        const std::string dirURL = Settings::GetDirectoryFeedURL();
        const std::string url = dirURL.empty() ? Settings::GetAppcastURL() : dirURL;
        if ( url.empty() )
            throw std::runtime_error("Appcast URL not specified.");
        CheckForInsecureURL(url, dirURL.empty() ? "appcast feed" : "directory feed");

        StringDownloadSink feed;
        DownloadFile(url, &feed, this, Settings::GetHttpHeadersString(), Download_BypassProxies);

        std::vector<Appcast> all;
        if ( dirURL.empty() )
        {
            all = Appcast::Load(feed.data);
        }
        else
        {
            const std::string fileName = Settings::GetDirectoryFeedFileName();
            all = Appcast::LoadDirectoryListing(feed.data, dirURL, fileName);
            if ( all.empty() )
                LogError(("Directory feed: " + fileName + " is not in the listing.").c_str());
        }

        if (all.empty())
        {
            // No applicable updates in the feed.
            UI::NotifyNoUpdates(ShouldAutomaticallyInstall());
            return;
        }

        // Sort by version number (listing date) and pick the latest:
		std::stable_sort
        (
            all.begin(), all.end(),
            [](const Appcast& a, const Appcast& b) { return CompareVersions(a.Version, b.Version) > 0; }
        );

        auto appcast = all.front();

        if (!appcast.ReleaseNotesURL.empty())
            CheckForInsecureURL(appcast.ReleaseNotesURL, "release notes");
        if (!appcast.enclosure.DownloadURL.empty())
            CheckForInsecureURL(appcast.enclosure.DownloadURL, "update file");

        Settings::WriteConfigValue("LastCheckTime", time(NULL));

        // Check if our version is out of date.
        bool isNewer;
        if ( appcast.ListingTime != 0 )
        {
            isNewer = appcast.ListingTime > GetInstalledListingTime();
        }
        else
        {
            const std::string currentVersion =
                    ToUtf8(Settings::GetAppBuildVersion());
            isNewer = CompareVersions(currentVersion, appcast.Version) < 0;
        }

        if ( !appcast.IsValid() || !isNewer )
        {
            // The same or newer version is already installed.
            UI::NotifyNoUpdates(ShouldAutomaticallyInstall());
            return;
        }

        // Check if the user opted to ignore this particular version.
        if ( ShouldSkipUpdate(appcast) )
        {
            UI::NotifyNoUpdates(ShouldAutomaticallyInstall());
            return;
        }

        // A listed file is only offered if the server agrees on its date.
        if ( appcast.ListingTime != 0 )
            VerifyListingTime(appcast, this);

        UI::NotifyUpdateAvailable(appcast, ShouldAutomaticallyInstall());
    }
    catch ( TerminateThreadException& )
    {
        // cancelled (e.g. on shutdown), this is not an error
        throw;
    }
    catch ( ... )
    {
        UI::NotifyUpdateError();
        throw;
    }
}

bool UpdateChecker::ShouldSkipUpdate(const Appcast& appcast) const
{
    std::string toSkip;
    if ( Settings::ReadConfigValue("SkipThisVersion", toSkip) )
    {
        return toSkip == appcast.Version;
    }
    else
    {
        return false;
    }
}


void PeriodicUpdateChecker::Run()
{
    // no initialization to do, so signal readiness immediately
    SignalReady();

    while (true)
    {
        // time to wait for next iteration: either a reasonable default or
        // time to next scheduled update check if checks are enabled
        unsigned sleepTimeInSeconds = 60 * 60; // 1 hour

        bool checkUpdates;
        Settings::ReadConfigValue("CheckForUpdates", checkUpdates, false);

        if (checkUpdates)
        {
            const time_t currentTime = time(NULL);
            time_t lastCheck = 0;
            Settings::ReadConfigValue("LastCheckTime", lastCheck);

            // Only check for updates in reasonable intervals:
            const int interval = win_sparkle_get_update_check_interval();
            time_t nextCheck = lastCheck + interval;
            if (currentTime >= nextCheck)
            {
                try
                {
                    PerformUpdateCheck();
                }
                catch ( TerminateThreadException& )
                {
                    throw;
                }
                CATCH_ALL_EXCEPTIONS // keep checking periodically despite errors
                sleepTimeInSeconds = interval;
            }
            else
            {
                sleepTimeInSeconds = unsigned(nextCheck - currentTime);
            }
        }

        if (WaitForTermination(sleepTimeInSeconds))
            return;
    }
}


namespace
{
CriticalSection g_periodicCheckerMutex;
PeriodicUpdateChecker *g_periodicChecker = NULL;
}

/*static*/ void PeriodicUpdateChecker::StartGlobal()
{
    CriticalSectionLocker lock(g_periodicCheckerMutex);
    if (g_periodicChecker)
        return;

    PeriodicUpdateChecker *check = new PeriodicUpdateChecker();
    try
    {
        check->Start();
    }
    catch (...)
    {
        delete check;
        throw;
    }
    g_periodicChecker = check;
}

/*static*/ void PeriodicUpdateChecker::StopGlobal()
{
    PeriodicUpdateChecker *check;
    {
        CriticalSectionLocker lock(g_periodicCheckerMutex);
        check = g_periodicChecker;
        g_periodicChecker = NULL;
    }

    if (check)
    {
        check->TerminateAndJoin();
        delete check;
    }
}


void OneShotUpdateChecker::Run()
{
    // no initialization to do, so signal readiness immediately
    SignalReady();

    PerformUpdateCheck();
}


/*--------------------------------------------------------------------------*
                            ManualUpdateChecker
 *--------------------------------------------------------------------------*/

bool ManualUpdateChecker::ShouldSkipUpdate(const Appcast&) const
{
    // If I chose "Skip version" it should not prompt me for automatic updates,
    // but if I explicitly open the dialog using
    // win_sparkle_check_update_with_ui() it should still show that version.
    // This is the semantics in Sparkle for Mac.
    return false;
}

} // namespace winsparkle
