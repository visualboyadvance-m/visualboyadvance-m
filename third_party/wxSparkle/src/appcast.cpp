/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "appcast.h"
#include "download.h"
#include "error.h"

#include "platform.h"

#include <expat.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <wx/utils.h> // wxGetOsVersion()
#endif

namespace winsparkle
{

namespace
{

// OS identification strings, as used in the sparkle:os attribute of the
// <enclosure> element. Both generic ("windows", "macos", "linux") and
// architecture-specific ("windows-x64", "macos-arm64", ...) values are
// understood.

#if defined(_WIN32)
    #define OS_MARKER_GENERIC "windows"
#elif defined(__APPLE__)
    #define OS_MARKER_GENERIC "macos"
#elif WXSPARKLE_CLASSIC_MAC
    // (not "macos", whose binaries can't run on classic Mac OS)
    #define OS_MARKER_GENERIC "macos-classic"
#elif defined(__ANDROID__) || defined(ANDROID)
    #define OS_MARKER_GENERIC "android"
#elif defined(__linux__)
    #define OS_MARKER_GENERIC "linux"
#elif defined(__FreeBSD__)
    #define OS_MARKER_GENERIC "freebsd"
#else
    #define OS_MARKER_GENERIC "unix"
#endif

#if defined(__AARCH64EL__) || defined(__aarch64__) || defined(_M_ARM64)
    #define OS_MARKER_ARCH_SUFFIX "-arm64"
#elif defined(__x86_64__) || defined(_M_X64)
    #define OS_MARKER_ARCH_SUFFIX "-x64"
#elif defined(__i386__) || defined(_M_IX86)
    #define OS_MARKER_ARCH_SUFFIX "-x86"
#elif defined(__arm__) || defined(_M_ARM)
    #define OS_MARKER_ARCH_SUFFIX "-arm"
#elif defined(__riscv) && (__riscv_xlen == 64)
    #define OS_MARKER_ARCH_SUFFIX "-riscv64"
#elif defined(__riscv)
    #define OS_MARKER_ARCH_SUFFIX "-riscv32"
#elif defined(__m68k__)
    #define OS_MARKER_ARCH_SUFFIX "-m68k"
#elif defined(__powerpc64__)
    #define OS_MARKER_ARCH_SUFFIX "-ppc64"
#elif (defined(__powerpc__) || defined(__ppc__)) && !defined(__powerpc64__)
    #define OS_MARKER_ARCH_SUFFIX "-ppc"
#else
    #define OS_MARKER_ARCH_SUFFIX "-unknown"
#endif

#define OS_MARKER_ARCH OS_MARKER_GENERIC OS_MARKER_ARCH_SUFFIX


// Misc helper functions:

#ifdef _WIN32

bool is_compatible_with_os_version(const Appcast& item)
{
    auto& version = item.MinOSVersion;

    if (version.empty())
        return true;

    OSVERSIONINFOEXW osvi {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    DWORD dwTypeMask = VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER;
    DWORDLONG dwlConditionMask = 0;
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER,  VER_GREATER_EQUAL);

    // parse the version number as major[.minor[.build]]:
    int parsed = sscanf(version.c_str(), "%lu.%lu.%lu",
                        &osvi.dwMajorVersion, &osvi.dwMinorVersion, &osvi.dwBuildNumber);
    if (parsed <= 0)
    {
        // failed to parse version number, ignore the value
        return true;
    }

    // allow alternative format major.minor-build for compatibility with
    // WinSparkle < 0.8.2, which only understood major.minor.servicepack. By using '-'
    // for the build component, older versions will only parse the major.minor part,
    // while newer WinSparkle versions will understand the full triplet:
    if (parsed == 2 && version.find('-') != std::string::npos)
    {
        parsed = sscanf(version.c_str(), "%lu.%lu-%lu",
                        &osvi.dwMajorVersion, &osvi.dwMinorVersion, &osvi.dwBuildNumber);
    }

    // backwards compatibility with WinSparkle < 0.8.2 which used major.minor.sp
    // instead of major.minor.build for the version number:
    if (parsed == 3 && osvi.dwBuildNumber < 100)
    {
        osvi.wServicePackMajor = static_cast<WORD>(osvi.dwBuildNumber);
        osvi.dwBuildNumber = 0;
        dwTypeMask |= VER_SERVICEPACKMAJOR;
        VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    }

    return VerifyVersionInfoW(&osvi, dwTypeMask, dwlConditionMask) != FALSE;
}

#elif defined(__APPLE__) || WXSPARKLE_CLASSIC_MAC

// sparkle:minimumSystemVersion is the macOS version, e.g. "10.15" or "13.0.1"
// (or the classic Mac OS one, e.g. "7.6.1" or "9.2")
bool is_compatible_with_os_version(const Appcast& item)
{
    auto& version = item.MinOSVersion;

    if (version.empty())
        return true;

    int req[3] = { 0, 0, 0 };
    const int parsed = sscanf(version.c_str(), "%d.%d.%d", &req[0], &req[1], &req[2]);
    if (parsed <= 0)
        return true; // failed to parse version number, ignore the value

    int cur[3] = { 0, 0, 0 };
    wxGetOsVersion(&cur[0], &cur[1], &cur[2]);

    for (int i = 0; i < 3; i++)
    {
        if (cur[i] != req[i])
            return cur[i] > req[i];
    }
    return true;
}

#else

// There's no meaningful single OS version on Linux/BSD (the kernel version
// isn't one), so minimum OS version requirements are ignored there.
bool is_compatible_with_os_version(const Appcast&)
{
    return true;
}

#endif


// Checks if the item is compatible with the running OS, that is:
// - is not for a different OS
// - is either for current architecture or is architecture-independent
//
// E.g. on Windows returns true if item is
// - "windows-arm64" on 64bit ARM
// - "windows-x64"   on 64bit Intel/AMD
// - "windows-x86"   on 32bit
// - "windows"       on any Windows arch
// - empty string    on any OS
// and similarly "macos", "macos-arm64", "macos-x64" on macOS or "linux",
// "linux-x64", "linux-arm64" etc. on Linux.
//
// PowerPC Macs run 68k code too (emulated), so "macos-classic-m68k" items are
// compatible with them, but native ones are preferred.
#if WXSPARKLE_CLASSIC_MAC && !defined(__m68k__)
    #define OS_MARKER_ARCH_FALLBACK OS_MARKER_GENERIC "-m68k"
#endif

inline bool is_compatible_with_os_arch(const Appcast::Enclosure& enclosure)
{
    return enclosure.OS.empty() || enclosure.OS == OS_MARKER_GENERIC || enclosure.OS == OS_MARKER_ARCH
#ifdef OS_MARKER_ARCH_FALLBACK
           || enclosure.OS == OS_MARKER_ARCH_FALLBACK
#endif
           ;
}


// Finds the best enclosure for the current OS and architecture.
Appcast::Enclosure find_best_enclosure_for_os_arch(const std::vector<Appcast::Enclosure>& enclosures)
{
    // filter out incompatible enclosures:
    std::vector<Appcast::Enclosure> compatible;
    std::copy_if(enclosures.begin(), enclosures.end(), std::back_inserter(compatible), is_compatible_with_os_arch);
    if (compatible.empty())
        return Appcast::Enclosure();

    // is there arch-specific enclosure?
    auto it = std::find_if(compatible.begin(), compatible.end(),
                           [](const Appcast::Enclosure& e) { return e.OS == OS_MARKER_ARCH; });
    if (it != compatible.end())
        return *it;

    // is there an enclosure explicitly marked as for this OS?
    it = std::find_if(compatible.begin(), compatible.end(),
                      [](const Appcast::Enclosure& e) { return e.OS == OS_MARKER_GENERIC; });
    if (it != compatible.end())
        return *it;

#ifdef OS_MARKER_ARCH_FALLBACK
    // is there one for an architecture this one can emulate?
    it = std::find_if(compatible.begin(), compatible.end(),
                      [](const Appcast::Enclosure& e) { return e.OS == OS_MARKER_ARCH_FALLBACK; });
    if (it != compatible.end())
        return *it;
#endif

    // because all enclosures are compatible, any one will do, e.g. the first one:
    return compatible.front();
}


void trim_whitespace(std::string& s)
{
    size_t startpos = s.find_first_not_of(" \t\r\n");
    if (startpos != std::string::npos)
        s = s.substr(startpos);
    size_t endpos = s.find_last_not_of(" \t\r\n");
    if (endpos != std::string::npos)
        s = s.substr(0, endpos + 1);
}


/*--------------------------------------------------------------------------*
                                XML parsing
 *--------------------------------------------------------------------------*/

#define MVAL(x) x
#define CONCAT3(a,b,c) MVAL(a)##MVAL(b)##MVAL(c)

#define NS_SPARKLE      "http://www.andymatuschak.org/xml-namespaces/sparkle"
#define NS_SEP          '#'
#define NS_SPARKLE_NAME(name) NS_SPARKLE "#" name

#define NODE_CHANNEL    "channel"
#define NODE_ITEM       "item"
#define NODE_RELNOTES   NS_SPARKLE_NAME("releaseNotesLink")
#define NODE_TITLE "title"
#define NODE_DESCRIPTION "description"
#define NODE_LINK       "link"
#define NODE_ENCLOSURE  "enclosure"
#define NODE_MIN_OS_VERSION NS_SPARKLE_NAME("minimumSystemVersion")
#define NODE_CRITICAL_UPDATE NS_SPARKLE_NAME("criticalUpdate")
#define ATTR_URL        "url"
#define ATTR_VERSION    NS_SPARKLE_NAME("version")
#define ATTR_SHORTVERSION NS_SPARKLE_NAME("shortVersionString")
#define ATTR_DSASIGNATURE NS_SPARKLE_NAME("dsaSignature")
#define ATTR_EDDSASIGNATURE NS_SPARKLE_NAME("edSignature")
#define ATTR_OS         NS_SPARKLE_NAME("os")
#define ATTR_ARGUMENTS  NS_SPARKLE_NAME("installerArguments")
#define NODE_VERSION      ATTR_VERSION        // These can be nodes or
#define NODE_SHORTVERSION ATTR_SHORTVERSION   // attributes.
#define NODE_DSASIGNATURE ATTR_DSASIGNATURE


// context data for the parser
struct ContextData
{
    ContextData(XML_Parser& p)
        : parser(p),
        in_channel(0), in_item(0), in_relnotes(0), in_title(0), in_description(0), in_link(0),
        in_version(0), in_shortversion(0), in_dsasignature(0), in_min_os_version(0)
    {}

    // call when entering <item> element
    void reset_for_new_item()
    {
        current = Appcast();
        enclosures.clear();
        legacy_dsa_signature.clear();
    }

    // the parser we're using
    XML_Parser& parser;

    // is inside <channel>, <item> or <sparkle:releaseNotesLink>, <title>, <description>, or <link> respectively?
    int in_channel, in_item, in_relnotes, in_title, in_description, in_link;

    // is inside <sparkle:version> or <sparkle:shortVersionString> etc. node?
    int in_version, in_shortversion, in_dsasignature, in_min_os_version;

    // currently parsed item
    Appcast current;

    // enclosures encountered so far
    std::vector<Appcast::Enclosure> enclosures;

    // signature present as <sparkle:dsaSignature>, not enclosure attribute 
    std::string legacy_dsa_signature;
    
    // parsed <item>s
    std::vector<Appcast> all_items;
};


void XMLCALL OnStartElement(void *data, const char *name, const char **attrs)
{
    ContextData& ctxt = *static_cast<ContextData*>(data);

    if ( strcmp(name, NODE_CHANNEL) == 0 )
    {
        ctxt.in_channel++;
    }
    else if ( ctxt.in_channel && strcmp(name, NODE_ITEM) == 0 )
    {
        ctxt.in_item++;
        ctxt.reset_for_new_item();
    }
    else if ( ctxt.in_item )
    {
        if ( strcmp(name, NODE_RELNOTES) == 0 )
        {
            ctxt.in_relnotes++;
        }
        else if ( strcmp(name, NODE_TITLE) == 0 )
        {
            ctxt.in_title++;
        }
        else if ( strcmp(name, NODE_DESCRIPTION) == 0 )
        {
            ctxt.in_description++;
        }
        else if ( strcmp(name, NODE_LINK) == 0 )
        {
            ctxt.in_link++;
        }
        else if ( strcmp(name, NODE_VERSION) == 0 )
        {
            ctxt.in_version++;
        }
        else if ( strcmp(name, NODE_SHORTVERSION) == 0 )
        {
            ctxt.in_shortversion++;
        }
        else if (strcmp(name, NODE_DSASIGNATURE) == 0)
        {
            ctxt.in_dsasignature++;
        }
        else if (strcmp(name, NODE_MIN_OS_VERSION) == 0)
        {
            ctxt.in_min_os_version++;
        }
        else if (strcmp(name, NODE_ENCLOSURE) == 0)
        {
            Appcast& item = ctxt.current;
            Appcast::Enclosure enclosure;

            for (int i = 0; attrs[i]; i += 2)
            {
                const char* name = attrs[i];
                const char* value = attrs[i + 1];

                if (strcmp(name, ATTR_URL) == 0)
                    enclosure.DownloadURL = value;
                else if (strcmp(name, ATTR_EDDSASIGNATURE) == 0)
                    enclosure.EdDsaSignature = value;
                else if (strcmp(name, ATTR_DSASIGNATURE) == 0)
                    enclosure.DsaSignature = value;
                else if (strcmp(name, ATTR_OS) == 0)
                    enclosure.OS = value;
                else if (strcmp(name, ATTR_ARGUMENTS) == 0)
                    enclosure.InstallerArguments = value;

                // legacy syntax where version info was on enclosure, not item:
                else if (strcmp(name, ATTR_VERSION) == 0)
                    item.Version = value;
                else if (strcmp(name, ATTR_SHORTVERSION) == 0)
                    item.ShortVersionString = value;
            }

            // note: we intentionally include incompatible enclosures in the list so that
            // we can check for that case later in OnEndElement() and skip the entire <item>
            if (enclosure.IsValid())
                ctxt.enclosures.push_back(enclosure);
        }
        else if (strcmp(name, NODE_CRITICAL_UPDATE) == 0)
        {
            ctxt.current.CriticalUpdate = true;
        }
    }
}


void XMLCALL OnEndElement(void *data, const char *name)
{
    ContextData& ctxt = *static_cast<ContextData*>(data);

    if (ctxt.in_item)
    {
        if (strcmp(name, NODE_RELNOTES) == 0)
        {
            ctxt.in_relnotes--;
        }
        else if (strcmp(name, NODE_TITLE) == 0)
        {
            ctxt.in_title--;
        }
        else if (strcmp(name, NODE_DESCRIPTION) == 0)
        {
            ctxt.in_description--;
        }
        else if (strcmp(name, NODE_MIN_OS_VERSION) == 0)
        {
            ctxt.in_min_os_version--;
        }
        else if (strcmp(name, NODE_LINK) == 0)
        {
            ctxt.in_link--;
        }
        else if (strcmp(name, NODE_VERSION) == 0)
        {
            ctxt.in_version--;
        }
        else if (strcmp(name, NODE_SHORTVERSION) == 0)
        {
            ctxt.in_shortversion--;
        }
        else if (strcmp(name, NODE_DSASIGNATURE) == 0)
        {
            ctxt.in_dsasignature--;
        }
        else if (strcmp(name, NODE_ITEM) == 0)
        {
            ctxt.in_item--;

            Appcast& item = ctxt.current;

            if (!ctxt.legacy_dsa_signature.empty() && item.enclosure.DsaSignature.empty())
                item.enclosure.DsaSignature = ctxt.legacy_dsa_signature;

            if (!ctxt.enclosures.empty())
            {
                item.enclosure = find_best_enclosure_for_os_arch(ctxt.enclosures);
                if (!item.enclosure.IsValid())
                {
                    // There are enclosures (e.g. weblink is not used), but all enclosures are
                    // incompatible. This means the <item> is not meant for this OS and should be
                    // skipped (as Sparkle does; there may be another <item> for us).
                    return;
                }
            }

            if (item.IsValid() && is_compatible_with_os_version(item))
            {
                ctxt.all_items.push_back(item);
            }
        }
    }
    else if (strcmp(name, NODE_CHANNEL) == 0 )
    {
        ctxt.in_channel--;
        // we've reached the end of <channel> element,
        // so we stop parsing
        XML_StopParser(ctxt.parser, XML_TRUE);
    }
}


void XMLCALL OnText(void *data, const char *s, int len)
{
    ContextData& ctxt = *static_cast<ContextData*>(data);
    Appcast& item = ctxt.current;

    if (ctxt.in_relnotes)
    {
        item.ReleaseNotesURL.append(s, len);
        trim_whitespace(item.ReleaseNotesURL);
    }
    else if (ctxt.in_title)
    {
        item.Title.append(s, len);
    }
    else if (ctxt.in_description)
    {
        item.Description.append(s, len);
    }
    else if (ctxt.in_link)
    {
        item.WebBrowserURL.append(s, len);
        trim_whitespace(item.WebBrowserURL);
    }
    else if (ctxt.in_version)
    {
        item.Version.append(s, len);
    }
    else if (ctxt.in_shortversion)
    {
        item.ShortVersionString.append(s, len);
    }
    else if (ctxt.in_dsasignature)
    {
        ctxt.legacy_dsa_signature.assign(s, len);
    }
    else if (ctxt.in_min_os_version)
    {
        item.MinOSVersion.append(s, len);
    }
}

} // anonymous namespace


/*--------------------------------------------------------------------------*
                               Appcast class
 *--------------------------------------------------------------------------*/

std::vector<Appcast> Appcast::Load(const std::string& xml)
{
    XML_Parser p = XML_ParserCreateNS(NULL, NS_SEP);
    if ( !p )
        throw std::runtime_error("Failed to create XML parser.");

    ContextData ctxt(p);

    XML_SetUserData(p, &ctxt);
    XML_SetElementHandler(p, OnStartElement, OnEndElement);
    XML_SetCharacterDataHandler(p, OnText);

    XML_Status st = XML_Parse(p, xml.c_str(), (int)xml.size(), XML_TRUE);

    if ( st == XML_STATUS_ERROR )
    {
        std::string msg("XML parser error: ");
        msg.append(XML_ErrorString(XML_GetErrorCode(p)));
        XML_ParserFree(p);
        throw std::runtime_error(msg);
    }

    XML_ParserFree(p);

    if (ctxt.all_items.empty())
        return {}; // invalid

    // the items were already filtered to only include those compatible with the current OS + arch
    // and meeting minimum OS version requirements, so we can just return the first one
    return std::move(ctxt.all_items);
}


/*--------------------------------------------------------------------------*
                             directory listings
 *--------------------------------------------------------------------------*/

namespace
{

bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

char LowerAscii(char c)
{
    return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
}

bool StartsWithNoCaseAt(const std::string& s, size_t pos, const char *prefix)
{
    for ( ; *prefix; ++prefix, ++pos )
    {
        if ( pos >= s.size() || LowerAscii(s[pos]) != LowerAscii(*prefix) )
            return false;
    }
    return true;
}

size_t FindNoCase(const std::string& s, const char *what, size_t from)
{
    for ( size_t i = from; i < s.size(); ++i )
    {
        if ( StartsWithNoCaseAt(s, i, what) )
            return i;
    }
    return std::string::npos;
}

bool ReadDigits(const std::string& s, size_t& i, int count, int& out)
{
    if ( i + count > s.size() )
        return false;
    int v = 0;
    for ( int k = 0; k < count; ++k )
    {
        const char c = s[i + k];
        if ( !IsDigit(c) )
            return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    i += count;
    return true;
}

bool ReadChar(const std::string& s, size_t& i, char c)
{
    if ( i >= s.size() || s[i] != c )
        return false;
    ++i;
    return true;
}

bool ReadMonthName(const std::string& s, size_t& i, int& month)
{
    static const char *const names[] =
    {
        "jan", "feb", "mar", "apr", "may", "jun",
        "jul", "aug", "sep", "oct", "nov", "dec"
    };
    for ( int m = 0; m < 12; ++m )
    {
        if ( StartsWithNoCaseAt(s, i, names[m]) )
        {
            month = m + 1;
            i += 3;
            return true;
        }
    }
    return false;
}

// Reads a listing's date at s[pos]: "2026-Nov-21 03:33" (fancyindex),
// "2026-11-21 03:33" (Apache) or "21-Nov-2026 03:33" (nginx), each optionally
// with ":SS".
bool ParseListingTimeAt(const std::string& s, size_t pos, time_t& out)
{
    if ( pos > 0 && IsDigit(s[pos - 1]) )
        return false;

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

    size_t i = pos;
    if ( ReadDigits(s, i, 4, year) && ReadChar(s, i, '-') )
    {
        if ( !ReadMonthName(s, i, month) && !ReadDigits(s, i, 2, month) )
            return false;
        if ( !ReadChar(s, i, '-') || !ReadDigits(s, i, 2, day) )
            return false;
    }
    else
    {
        i = pos;
        if ( !ReadDigits(s, i, 2, day) || !ReadChar(s, i, '-') ||
             !ReadMonthName(s, i, month) || !ReadChar(s, i, '-') ||
             !ReadDigits(s, i, 4, year) )
            return false;
    }

    if ( !ReadChar(s, i, ' ') )
        return false;
    while ( i < s.size() && s[i] == ' ' )
        ++i;

    if ( !ReadDigits(s, i, 2, hour) || !ReadChar(s, i, ':') || !ReadDigits(s, i, 2, minute) )
        return false;
    {
        size_t j = i;
        if ( ReadChar(s, j, ':') && ReadDigits(s, j, 2, second) )
            i = j;
    }
    if ( i < s.size() && IsDigit(s[i]) )
        return false;

    if ( year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
         hour > 23 || minute > 59 || second > 60 )
        return false;

    out = detail::MakeUTCTime(year, month, day, hour, minute, second);
    return true;
}

// The value of the href attribute starting at s[pos] (just past "href="),
// with &amp; decoded; npos in @a end if it is malformed.
std::string ReadAttributeValue(const std::string& s, size_t pos, size_t& end)
{
    end = std::string::npos;
    if ( pos >= s.size() )
        return std::string();

    std::string value;
    const char quote = s[pos];
    if ( quote == '"' || quote == '\'' )
    {
        const size_t close = s.find(quote, pos + 1);
        if ( close == std::string::npos )
            return std::string();
        value = s.substr(pos + 1, close - pos - 1);
        end = close + 1;
    }
    else
    {
        size_t close = pos;
        while ( close < s.size() && s[close] != ' ' && s[close] != '>' &&
                s[close] != '\t' && s[close] != '\r' && s[close] != '\n' )
            ++close;
        value = s.substr(pos, close - pos);
        end = close;
    }

    for ( size_t amp = value.find("&amp;"); amp != std::string::npos; amp = value.find("&amp;", amp + 1) )
        value.erase(amp + 1, 4);

    return value;
}

std::string MakeVersion(time_t t)
{
    // "YYYY-MM-DD HH:MM" -> "YYYY.MM.DD.HHMM"
    std::string v = detail::FormatUTCTime(t);
    v[4] = '.';
    v[7] = '.';
    v[10] = '.';
    v.erase(13, 1);
    return v;
}

} // anonymous namespace


namespace detail
{

time_t MakeUTCTime(int year, int month, int day, int hour, int minute, int second)
{
    // days from civil, H. Hinnant's algorithm
    const int y = year - (month <= 2 ? 1 : 0);
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = unsigned(y - era * 400);
    const unsigned mp = unsigned(month > 2 ? month - 3 : month + 9);
    const unsigned doy = (153 * mp + 2) / 5 + unsigned(day) - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = era * 146097 + (long long)doe - 719468;

    return time_t(days * 86400 + hour * 3600 + minute * 60 + second);
}

std::string FormatUTCTime(time_t t)
{
    long long secs = (long long)t;
    long long days = secs / 86400;
    long long rem = secs % 86400;
    if ( rem < 0 )
    {
        rem += 86400;
        --days;
    }

    // civil from days, H. Hinnant's algorithm
    days += 719468;
    const long long era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = unsigned(days - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned day = doy - (153 * mp + 2) / 5 + 1;
    const unsigned month = mp < 10 ? mp + 3 : mp - 9;
    const long long year = (long long)yoe + era * 400 + (month <= 2 ? 1 : 0);

    char buf[32];
    snprintf(buf, sizeof(buf), "%04lld-%02u-%02u %02d:%02d",
             year, month, day, int(rem / 3600), int(rem / 60 % 60));
    return buf;
}

std::string ResolveURL(const std::string& base, const std::string& href)
{
    if ( StartsWithNoCaseAt(href, 0, "http://") || StartsWithNoCaseAt(href, 0, "https://") )
        return href;

    const size_t schemeEnd = base.find("://");
    if ( schemeEnd == std::string::npos )
        return href;

    if ( href.compare(0, 2, "//") == 0 )
        return base.substr(0, schemeEnd + 1) + href;

    const std::string path = base.substr(0, base.find_first_of("?#"));
    const size_t hostEnd = path.find('/', schemeEnd + 3);

    if ( !href.empty() && href[0] == '/' )
        return path.substr(0, hostEnd) + href;

    std::string dir;
    if ( hostEnd == std::string::npos )
        dir = path + "/";
    else
        dir = path.substr(0, path.rfind('/') + 1);

    std::string rel = href;
    while ( rel.compare(0, 2, "./") == 0 )
        rel.erase(0, 2);

    return dir + rel;
}

} // namespace detail


std::vector<Appcast> Appcast::LoadDirectoryListing(const std::string& html,
                                                   const std::string& indexURL,
                                                   const std::string& fileName)
{
    std::vector<Appcast> items;
    if ( fileName.empty() )
        return items;

    size_t pos = FindNoCase(html, "href=", 0);
    while ( pos != std::string::npos )
    {
        size_t valueEnd;
        const std::string href = ReadAttributeValue(html, pos + 5, valueEnd);
        const size_t next = valueEnd == std::string::npos
                            ? std::string::npos
                            : FindNoCase(html, "href=", valueEnd);
        if ( valueEnd == std::string::npos || href.empty() || href[0] == '?' || href[0] == '#' )
        {
            pos = next;
            continue;
        }

        const std::string url = detail::ResolveURL(indexURL, href);
        if ( detail::GetURLFileName(url) == fileName )
        {
            // The date is in the same row: after the link, before the next one.
            const size_t rowEnd = next == std::string::npos ? html.size() : next;
            for ( size_t i = valueEnd; i < rowEnd; ++i )
            {
                time_t t;
                if ( !IsDigit(html[i]) || !ParseListingTimeAt(html, i, t) )
                    continue;

                Appcast item;
                item.ListingTime = t;
                item.Version = MakeVersion(t);
                item.ShortVersionString = detail::FormatUTCTime(t) + " UTC";
                item.Title = item.ShortVersionString;
                item.enclosure.DownloadURL = url;
                items.push_back(item);
                break;
            }
        }

        pos = next;
    }

    return items;
}

} // namespace winsparkle
