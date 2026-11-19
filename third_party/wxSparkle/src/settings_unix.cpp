/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

// Non-Windows implementation of the platform-specific parts of Settings:
//
//  - App metadata come from the main bundle's Info.plist on macOS and from
//    the host wxApp (display name, vendor) as a fallback everywhere.
//  - "Custom resources" (feed URL, public keys) map to Sparkle-compatible
//    Info.plist keys on macOS (SUFeedURL, SUPublicEDKey, SUPublicDSAKey[File]).
//  - Runtime configuration is stored in an INI-style file instead of the
//    registry; see win_sparkle_set_registry_path() in winsparkle.h.
//
//  On classic Mac OS (Retro68), the INI file lives in the Preferences folder
//  and is accessed using wxWidgets, which knows how to handle HFS paths; the
//  POSIX file system API isn't available there.

#ifndef _WIN32

#include "settings.h"

#include "error.h"
#include "platform.h"
#include "utils.h"

#include <wx/app.h>
#if WXSPARKLE_CLASSIC_MAC
    #include <wx/filefn.h>
    #include <wx/filename.h>
    #include <wx/stdpaths.h>
#endif

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#if !WXSPARKLE_CLASSIC_MAC
    #include <pwd.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

#ifdef __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
#endif

namespace fs = std::filesystem;

namespace winsparkle
{

#if WXSPARKLE_CLASSIC_MAC
    typedef wxString ConfigPath; // full HFS path
#else
    typedef fs::path ConfigPath;
#endif

/*--------------------------------------------------------------------------*
                                 helpers
 *--------------------------------------------------------------------------*/

namespace
{

// ASCII case-insensitive comparison (strcasecmp() isn't available everywhere)
bool EqualsNoCase(const std::string& a, const char *b)
{
    const size_t len = strlen(b);
    if ( a.size() != len )
        return false;
    for ( size_t i = 0; i < len; i++ )
    {
        if ( tolower(static_cast<unsigned char>(a[i])) != tolower(static_cast<unsigned char>(b[i])) )
            return false;
    }
    return true;
}

#ifdef __APPLE__

std::string CFStringToUtf8(CFStringRef str)
{
    if ( !str )
        return std::string();

    const CFIndex len = CFStringGetLength(str);
    const CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::vector<char> buf(static_cast<size_t>(maxSize));
    if ( !CFStringGetCString(str, buf.data(), maxSize, kCFStringEncodingUTF8) )
        return std::string();
    return std::string(buf.data());
}

// Returns string value of the given key in main bundle's Info.plist.
std::string GetInfoPlistString(const char *key)
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if ( !bundle )
        return std::string();

    CFStringRef cfkey = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    if ( !cfkey )
        return std::string();

    // note: "Get" function, the returned value is not owned by us
    CFTypeRef value = CFBundleGetValueForInfoDictionaryKey(bundle, cfkey);
    CFRelease(cfkey);

    if ( !value || CFGetTypeID(value) != CFStringGetTypeID() )
        return std::string();

    return CFStringToUtf8(static_cast<CFStringRef>(value));
}

// Reads a file from the main bundle's Resources directory.
std::string ReadBundleResource(const std::string& name)
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if ( !bundle || name.empty() )
        return std::string();

    CFStringRef cfname = CFStringCreateWithCString(kCFAllocatorDefault, name.c_str(), kCFStringEncodingUTF8);
    if ( !cfname )
        return std::string();

    CFURLRef url = CFBundleCopyResourceURL(bundle, cfname, NULL, NULL);
    CFRelease(cfname);
    if ( !url )
        return std::string();

    char path[PATH_MAX];
    const bool ok = CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(path), sizeof(path));
    CFRelease(url);
    if ( !ok )
        return std::string();

    std::ifstream f(path, std::ios::binary);
    if ( !f )
        return std::string();
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

#endif // __APPLE__


#if WXSPARKLE_CLASSIC_MAC

// HFS file and folder names are limited to 31 characters and can't contain
// colons, the path separator.
wxString MakeHFSName(const std::string& utf8)
{
    wxString name = wxString::FromUTF8(utf8);
    name.Replace(":", "-");
    if ( name.length() > 31 )
        name.Truncate(31);
    return name;
}

// Maps the registry-style path to a file in the Preferences folder, e.g.
// "Software\Company\App\WinSparkle" -> "<Preferences>:Company:App:WinSparkle.ini"
ConfigPath GetConfigFile(bool global)
{
    // there's no separate system-wide configuration
    if ( global )
        return ConfigPath();

    const std::string regpath = Settings::GetRegistryPath();

    // a full HFS path ("Volume:Folder:File") is used as-is
    if ( regpath.find(':') != std::string::npos && regpath[0] != ':' )
        return wxString::FromUTF8(regpath);

    std::vector<std::string> components;
    std::string p = regpath;
    for ( auto& c : p )
    {
        if ( c == '\\' || c == '/' )
            c = '\n';
    }
    std::istringstream ss(p);
    std::string component;
    while ( std::getline(ss, component, '\n') )
    {
        if ( component.empty() || component == "." || component == ".." )
            continue;
        if ( components.empty() && EqualsNoCase(component, "Software") )
            continue;
        components.push_back(component);
    }

    if ( components.empty() )
        throw std::runtime_error("Invalid settings path \"" + regpath + "\"");

    const wxString prefs = wxStandardPaths::Get().GetUserConfigDir();
    if ( prefs.empty() )
        throw std::runtime_error("Cannot determine the Preferences folder");

    wxFileName fn(prefs, wxString());
    for ( size_t i = 0; i + 1 < components.size(); i++ )
        fn.AppendDir(MakeHFSName(components[i]));

    // (keep room for the extension)
    wxString name = MakeHFSName(components.back());
    if ( name.length() > 27 )
        name.Truncate(27);
    fn.SetFullName(name + ".ini");

    return fn.GetFullPath();
}

#else // !WXSPARKLE_CLASSIC_MAC

std::string GetHomeDir()
{
    const char *home = getenv("HOME");
    if ( home && *home )
        return home;

    struct passwd *pw = getpwuid(getuid());
    if ( pw && pw->pw_dir )
        return pw->pw_dir;

    throw std::runtime_error("Cannot determine home directory");
}

fs::path GetUserConfigBaseDir()
{
#ifdef __APPLE__
    return fs::path(GetHomeDir()) / "Library" / "Application Support";
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if ( xdg && *xdg == '/' )
        return fs::path(xdg);
    return fs::path(GetHomeDir()) / ".config";
#endif
}

fs::path GetSystemConfigBaseDir()
{
#ifdef __APPLE__
    return fs::path("/Library/Application Support");
#else
    return fs::path("/etc/xdg");
#endif
}

// Maps the registry-style path to a relative file path, e.g.
// "Software\Company\App\WinSparkle" -> "Company/App/WinSparkle.ini"
fs::path RegistryPathToRelativeFile(const std::string& regpath)
{
    std::string p = regpath;
    for ( auto& c : p )
    {
        if ( c == '\\' )
            c = '/';
    }

    fs::path rel;
    std::istringstream ss(p);
    std::string component;
    bool first = true;
    while ( std::getline(ss, component, '/') )
    {
        if ( component.empty() || component == "." || component == ".." )
            continue;
        if ( first )
        {
            first = false;
            if ( EqualsNoCase(component, "Software") )
                continue;
        }
        rel /= component;
    }

    if ( rel.empty() )
        throw std::runtime_error("Invalid settings path \"" + regpath + "\"");

    rel += ".ini";
    return rel;
}

// Returns the per-user (global=false) or system-wide (global=true) config
// file. The latter may be empty if there isn't any.
fs::path GetConfigFile(bool global)
{
    const std::string regpath = Settings::GetRegistryPath();

    // an absolute path is used as-is
    if ( !regpath.empty() && regpath[0] == '/' )
        return global ? fs::path() : fs::path(regpath);

    const fs::path rel = RegistryPathToRelativeFile(regpath);
    return (global ? GetSystemConfigBaseDir() : GetUserConfigBaseDir()) / rel;
}

#endif // WXSPARKLE_CLASSIC_MAC/!WXSPARKLE_CLASSIC_MAC


std::string EscapeValue(const std::string& s)
{
    std::string out;
    for ( char c : s )
    {
        switch ( c )
        {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string UnescapeValue(const std::string& s)
{
    std::string out;
    for ( size_t i = 0; i < s.size(); ++i )
    {
        if ( s[i] == '\\' && i + 1 < s.size() )
        {
            ++i;
            switch ( s[i] )
            {
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                default:   out += s[i]; break;
            }
        }
        else
        {
            out += s[i];
        }
    }
    return out;
}

typedef std::vector<std::pair<std::string, std::string>> IniEntries;

// Loads "key=value" lines; comments ('#', ';') and [sections] are ignored.
bool LoadIniFile(const ConfigPath& file, IniEntries& entries)
{
    entries.clear();

#if WXSPARKLE_CLASSIC_MAC
    // (the C library understands HFS paths in the file system encoding)
    std::ifstream f(static_cast<const char*>(file.fn_str()), std::ios::binary);
#else
    std::ifstream f(file, std::ios::binary);
#endif
    if ( !f )
        return false;

    std::string line;
    while ( std::getline(f, line) )
    {
        if ( !line.empty() && line.back() == '\r' )
            line.pop_back();
        if ( line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[' )
            continue;
        const size_t eq = line.find('=');
        if ( eq == std::string::npos )
            continue;
        entries.emplace_back(line.substr(0, eq), UnescapeValue(line.substr(eq + 1)));
    }
    return true;
}

#if WXSPARKLE_CLASSIC_MAC

void SaveIniFile(const ConfigPath& file, const IniEntries& entries)
{
    const wxFileName fn(file);
    const wxString dir = fn.GetPath();
    if ( !wxFileName::DirExists(dir) &&
         !wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) )
    {
        throw std::runtime_error("Cannot write settings: cannot create folder " + std::string(dir.utf8_str()));
    }

    // write to a temporary file first and replace the old one with it (it
    // must be in the same folder, for the rename to work)
    const wxString tmp = wxFileName(dir, "wxSparkle settings.tmp").GetFullPath();

    {
        std::ofstream f(static_cast<const char*>(tmp.fn_str()), std::ios::binary | std::ios::trunc);
        if ( !f )
            throw std::runtime_error("Cannot write settings to " + std::string(tmp.utf8_str()));

        f << "# WinSparkle (wxSparkle) settings\n";
        for ( const auto& e : entries )
            f << e.first << '=' << EscapeValue(e.second) << '\n';

        f.flush();
        if ( !f )
        {
            f.close();
            wxRemoveFile(tmp);
            throw std::runtime_error("Cannot write settings to " + std::string(tmp.utf8_str()));
        }
    }

    if ( !wxRenameFile(tmp, file, true) )
    {
        wxRemoveFile(tmp);
        throw std::runtime_error("Cannot write settings to " + std::string(file.utf8_str()));
    }
}

#else // !WXSPARKLE_CLASSIC_MAC

void SaveIniFile(const ConfigPath& file, const IniEntries& entries)
{
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if ( ec )
        throw std::runtime_error("Cannot write settings: cannot create directory " + file.parent_path().string());

    // write to a temporary file first and atomically replace the old one
    fs::path tmp = file;
    tmp += ".tmp" + std::to_string(static_cast<long>(getpid()));

    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if ( !f )
            throw std::runtime_error("Cannot write settings to " + tmp.string());

        f << "# WinSparkle (wxSparkle) settings\n";
        for ( const auto& e : entries )
            f << e.first << '=' << EscapeValue(e.second) << '\n';

        f.flush();
        if ( !f )
        {
            f.close();
            fs::remove(tmp, ec);
            throw std::runtime_error("Cannot write settings to " + tmp.string());
        }
    }

    fs::rename(tmp, file, ec);
    if ( ec )
    {
        fs::remove(tmp, ec);
        throw std::runtime_error("Cannot write settings to " + file.string());
    }
}

#endif // WXSPARKLE_CLASSIC_MAC/!WXSPARKLE_CLASSIC_MAC

} // anonymous namespace


/*--------------------------------------------------------------------------*
                             app metadata access
 *--------------------------------------------------------------------------*/

std::wstring Settings::DoGetVerInfoField(const wchar_t *field, bool fatal)
{
    const std::wstring f(field);
    std::string value;

#ifdef __APPLE__
    if ( f == L"ProductName" )
    {
        value = GetInfoPlistString("CFBundleDisplayName");
        if ( value.empty() )
            value = GetInfoPlistString("CFBundleName");
    }
    else if ( f == L"ProductVersion" )
    {
        value = GetInfoPlistString("CFBundleShortVersionString");
        if ( value.empty() )
            value = GetInfoPlistString("CFBundleVersion");
    }
    else if ( f == L"BuildVersion" )
    {
        value = GetInfoPlistString("CFBundleVersion");
    }
#endif

    // fall back to the host wxWidgets application's metadata
    if ( value.empty() && wxTheApp )
    {
        if ( f == L"ProductName" )
        {
            value = wxTheApp->GetAppDisplayName().utf8_string();
            if ( value.empty() )
                value = wxTheApp->GetAppName().utf8_string();
        }
        else if ( f == L"CompanyName" )
        {
            value = wxTheApp->GetVendorDisplayName().utf8_string();
        }
    }

    // vendor name is optional, there's no equivalent of VERSIONINFO's
    // mandatory CompanyName on these platforms
    if ( value.empty() && fatal && f != L"CompanyName" )
    {
        throw std::runtime_error(
            "Application metadata \"" + ToUtf8(f) + "\" is not available; "
            "use win_sparkle_set_app_details() to set it.");
    }

    return FromUtf8(value);
}


std::string Settings::GetCustomResource(const char *name, const char *type)
{
    std::string value;

#ifdef __APPLE__
    if ( strcmp(name, "FeedURL") == 0 )
    {
        value = GetInfoPlistString("SUFeedURL");
    }
    else if ( strcmp(name, "EdDSAPub") == 0 )
    {
        value = GetInfoPlistString("SUPublicEDKey");
    }
    else if ( strcmp(name, "DSAPub") == 0 )
    {
        value = GetInfoPlistString("SUPublicDSAKey");
        if ( value.empty() )
            value = ReadBundleResource(GetInfoPlistString("SUPublicDSAKeyFile"));
    }
#endif

    if ( value.empty() )
    {
        std::string err;
        err += "Failed to get resource \"";
        err += name;
        err += "\" (type \"";
        err += type;
        err += "\"); set it using the API";
        throw std::runtime_error(err);
    }

    return value;
}


/*--------------------------------------------------------------------------*
                             runtime config access
 *--------------------------------------------------------------------------*/

int WIN_SPARKLE_CALL Settings::DefaultConfigRead(const char *name, wchar_t *buf, size_t len, void *)
{
    if ( len == 0 )
        return 0;

    // Try reading from per-user file first. If that fails, look at the
    // system-wide file too (equivalent of HKLM on Windows).
    for ( bool global : { false, true } )
    {
        const ConfigPath file = GetConfigFile(global);
        if ( file.empty() )
            continue;

        IniEntries entries;
        if ( !LoadIniFile(file, entries) )
            continue;

        for ( const auto& e : entries )
        {
            if ( e.first == name )
            {
                const std::wstring v = FromUtf8(e.second);
                const size_t n = std::min(v.size(), len - 1);
                wmemcpy(buf, v.data(), n);
                buf[n] = 0;
                return 1;
            }
        }
    }

    return 0;
}

void WIN_SPARKLE_CALL Settings::DefaultConfigWrite(const char *name, const wchar_t *value, void *)
{
    const ConfigPath file = GetConfigFile(false);

    IniEntries entries;
    LoadIniFile(file, entries);

    bool found = false;
    for ( auto& e : entries )
    {
        if ( e.first == name )
        {
            e.second = ToUtf8(value);
            found = true;
        }
    }
    if ( !found )
        entries.emplace_back(name, ToUtf8(value));

    SaveIniFile(file, entries);
}

void WIN_SPARKLE_CALL Settings::DefaultConfigDelete(const char *name, void *)
{
    const ConfigPath file = GetConfigFile(false);

    IniEntries entries;
    if ( !LoadIniFile(file, entries) )
        return;

    const size_t before = entries.size();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [name](const IniEntries::value_type& e) { return e.first == name; }),
                  entries.end());

    if ( entries.size() != before )
        SaveIniFile(file, entries);
}

} // namespace winsparkle

#endif // !_WIN32
