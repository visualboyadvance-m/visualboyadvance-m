/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "appcast.h"
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
#elif defined(__m68k__)
    #define OS_MARKER_ARCH_SUFFIX "-m68k"
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

} // namespace winsparkle
