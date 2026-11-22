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

#ifndef _appcast_h_
#define _appcast_h_

#include <ctime>
#include <string>
#include <vector>


namespace winsparkle
{

/**
    This class contains information from the appcast.
 */
struct Appcast
{
    /// App version fields
    std::string Version;
    std::string ShortVersionString;

    /// URL of the release notes page
    std::string ReleaseNotesURL;

    /// URL to launch in web browser (instead of downloading update ourselves)
    std::string WebBrowserURL;

    /// Title of the update
    std::string Title;

    /// Description of the update
    std::string Description;

    // Minimum OS version required for update
    std::string MinOSVersion;

    // CriticalUpdate?
    bool CriticalUpdate = false;

    /// For an update found in a directory listing (LoadDirectoryListing()):
    /// the file's date in the listing (UTC); 0 for appcast items.
    time_t ListingTime = 0;

    struct Enclosure
    {
        /// URL of the update
        std::string DownloadURL;

        /// Signing signature of the update
        std::string DsaSignature;

        /// Signing EdDSA signature of the update
        std::string EdDsaSignature;

        // Operating system
        std::string OS;

        // Arguments passed on the the updater executable
        std::string InstallerArguments;

        bool IsValid() const { return !DownloadURL.empty(); }
    };

    Enclosure enclosure;


    /**
        Loads all updates from XML appcast feed.

        The list is in the order of the feed, without any additional sorting.

        Entries that are not appliable (e.g. for different OS) are skipped.

        Throws on error.

        Returns emty list if no error ocurred, but there was no usable update
        in the appcast.

        @param xml Appcast feed data.
     */
    static std::vector<Appcast> Load(const std::string& xml);

    /**
        Finds a file in a web server's directory listing (an autoindex page:
        Apache's, nginx's or nginx's fancyindex) and makes an update of it.

        The update is identified by the file's date in the listing, which is
        taken as UTC (what these listings show) and becomes ListingTime;
        Version is the date as "YYYY.MM.DD.HHMM", ShortVersionString as
        "YYYY-MM-DD HH:MM UTC".

        Returns one item for every row naming @a fileName with a date in it,
        in the order of the listing; an empty list if there is none.

        @param html      The listing page.
        @param indexURL  Its URL, which relative links resolve against.
        @param fileName  The file's name (the last component of its link).
     */
    static std::vector<Appcast> LoadDirectoryListing(const std::string& html,
                                                     const std::string& indexURL,
                                                     const std::string& fileName);

    /// Returns true if the struct constains valid data.
    bool IsValid() const { return !Version.empty() && (HasDownload() || !WebBrowserURL.empty()); }

    /// If true, then download and install the update ourselves.
    /// If false, launch a web browser to WebBrowserURL.
    bool HasDownload() const { return enclosure.IsValid(); }
};


/// Helpers exposed for unit testing.
namespace detail
{

/// Seconds since the epoch of the given UTC date and time.
time_t MakeUTCTime(int year, int month, int day, int hour, int minute, int second);

/// Formats a time as "YYYY-MM-DD HH:MM" (UTC).
std::string FormatUTCTime(time_t t);

/// Resolves a link in the page at @a base to an absolute URL.
std::string ResolveURL(const std::string& base, const std::string& href);

} // namespace detail

} // namespace winsparkle

#endif // _appcast_h_
