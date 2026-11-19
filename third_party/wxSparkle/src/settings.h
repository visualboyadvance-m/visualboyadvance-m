/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _settings_h_
#define _settings_h_

#include "winsparkle.h"
#include "threads.h"
#include "utils.h"

#include <map>
#include <string>
#include <sstream>


namespace winsparkle
{

/**
    Holds all of WinSparkle configuration.

    It is used both for storing explicitly set configuration values (e.g.
    using win_sparkle_set_appcast_url()) and for retrieving them from default
    locations (e.g. from resources or registry).

    Note that it is only allowed to modify the settings before the first
    call to win_sparkle_init().
 */
class Settings
{
public:
    /**
        Getting app metadata.
     */
    //@{

    /// Get location of the appcast
    static std::string GetAppcastURL()
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_appcastURL.empty() )
            ms_appcastURL = GetCustomResource("FeedURL", "APPCAST");
        return ms_appcastURL;
    }

    /// Return application name
    static std::wstring GetAppName()
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_appName.empty() )
            ms_appName = GetVerInfoField(L"ProductName");
        return ms_appName;
    }

    /// Return (human-readable) application version
    static std::wstring GetAppVersion()
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_appVersion.empty() )
            ms_appVersion = GetVerInfoField(L"ProductVersion");
        return ms_appVersion;
    }

    /// Return (internal) application build version
    static std::wstring GetAppBuildVersion()
    {
        {
            CriticalSectionLocker lock(ms_csVars);
            if ( !ms_appBuildVersion.empty() )
                return ms_appBuildVersion;
            // platform-specific build number, e.g. CFBundleVersion on macOS,
            // but only if the app didn't set its version explicitly:
            if ( !ms_appVersionSetByApp )
            {
                try
                {
                    ms_appBuildVersion = TryGetVerInfoField(L"BuildVersion");
                }
                catch ( ... ) {}
                if ( !ms_appBuildVersion.empty() )
                    return ms_appBuildVersion;
            }
        }
        // fallback if build number wasn't set:
        return GetAppVersion();
    }

    /// Return name of the vendor
    static std::wstring GetCompanyName()
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_companyName.empty() )
            ms_companyName = GetVerInfoField(L"CompanyName");
        return ms_companyName;
    }

    /// Return the registry path to store settings in
    static std::string GetRegistryPath()
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_registryPath.empty() )
            ms_registryPath = GetDefaultRegistryPath();
        return ms_registryPath;
    }

    /// Return DSA public key to verify update file signature
    static const std::string &GetDSAPubKeyPem()
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_DSAPubKey.empty() )
            ms_DSAPubKey = GetCustomResource("DSAPub", "DSAPEM");
        return ms_DSAPubKey;
    }

    /// Return EdDSA public key to verify update file signature
    static const std::string& GetEdDSAPubKey()
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_EdDSAPubKey.empty())
            ms_EdDSAPubKey = GetCustomResource("EdDSAPub", "EDDSA");
        return ms_EdDSAPubKey;
    }

    /// Return true if DSA public key is available
    static bool HasDSAPubKeyPem()
    {
        try
        {
            return !GetDSAPubKeyPem().empty();
        }
        CATCH_ALL_EXCEPTIONS
        return false;
    }

    /// Return true if EdDSA public key is available
    static bool HasEdDSAPubKey()
    {
        try
        {
            return !GetEdDSAPubKey().empty();
        }
        CATCH_ALL_EXCEPTIONS
        return false;
    }

    //@}

    /**
        UI language.
    */
    //@{

    struct Lang
    {
        Lang() : langid(0) {}
        bool IsOk() const { return !lang.empty() || langid != 0; }

        std::string lang;
        unsigned short langid;
    };

    static Lang GetLanguage()
    {
        CriticalSectionLocker lock(ms_csVars);
        return ms_lang;
    }

    static void SetLanguage(const char *lang)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_lang.lang = lang;
    }

    static void SetLanguage(unsigned short langid)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_lang.langid = langid;
    }

    //@}

    /**
        Overwriting app metadata.

        Normally, they would be retrieved from resources, but it's sometimes
        necessary to set them manually.
     */
    //@{

    /// Set appcast location
    static void SetAppcastURL(const char *url)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_appcastURL = url;
    }

    /// Set application name
    static void SetAppName(const wchar_t *name)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_appName = name;
    }

    /// Set application version
    static void SetAppVersion(const wchar_t *version)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_appVersion = version;
        ms_appVersionSetByApp = true;
    }

    /// Add a custom HTT header to requests
    static void SetHttpHeader(const char *name, const char *value)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_httpHeaders[name] = value;
    }

    /// Get a string containing all custom HTTP headers
    static std::string GetHttpHeadersString()
    {
        CriticalSectionLocker lock(ms_csVars);
        std::string out;
        for (auto i = ms_httpHeaders.begin(); i != ms_httpHeaders.end(); ++i)
            out += i->first + ": " + i->second + "\r\n";
        return out;
    }

    /// Clear previously set HTTP headers
    static void ClearHttpHeaders()
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_httpHeaders.clear();
    }

    /// Set application's build version number
    static void SetAppBuildVersion(const wchar_t *version)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_appBuildVersion = version;
    }

    /// Set company name
    static void SetCompanyName(const wchar_t *name)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_companyName = name;
    }

    /// Set Windows registry path to store settings in (relative to HKCU/KHLM).
    /// On other platforms, this is mapped to an INI file path (see winsparkle.h).
    static void SetRegistryPath(const char *path)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_registryPath = path;
    }

    /// Return WinSparkle's default configuration read, write and delete functions
    static win_sparkle_config_methods_t GetDefaultConfigMethods()
    {
        win_sparkle_config_methods_t defaultConfigMethods;
        defaultConfigMethods.config_read = &DefaultConfigRead;
        defaultConfigMethods.config_write = &DefaultConfigWrite;
        defaultConfigMethods.config_delete = &DefaultConfigDelete;
        defaultConfigMethods.user_data = NULL;
        return defaultConfigMethods;
    }

    /// Set custom configuration read, write and delete functions
    static void SetConfigMethods(win_sparkle_config_methods_t *customConfigMethods)
    {
        CriticalSectionLocker lock(ms_csVars);
        ms_configMethods = customConfigMethods ? *customConfigMethods : GetDefaultConfigMethods();
    }

    /// Set PEM data and verify that it contains valid DSA public key
    static void SetDSAPubKeyPem(const std::string &pem);
    //@}

    /// Set base64-encoded data and verify it contains valid EdDSA public key
    static void SetEdDSAPubKey(const std::string& pubkey_base64);
    //@}

    /**
        Access to runtime configuration.

        This is stored in registry, under HKCU\Software\...\...\WinSparkle,
        where the vendor and app names are determined from resources (on
        Windows), or in an INI file in the user's configuration directory
        (elsewhere).
     */
    //@{

    // Writes given value to registry under this name.
    template<typename T>
    static void WriteConfigValue(const char *name, const T& value)
    {
        std::wostringstream s;
        s << value;
        DoWriteConfigValue(name, s.str().c_str());
    }

    static void WriteConfigValue(const char *name, const std::string& value)
    {
        DoWriteConfigValue(name, AnsiToWide(value).c_str());
    }

    static void WriteConfigValue(const char *name, const std::wstring& value)
    {
        DoWriteConfigValue(name, value.c_str());
    }

    // Reads a value from registry. Returns true if it was present,
    // false otherwise.
    template<typename T>
    static bool ReadConfigValue(const char *name, T& value)
    {
        const std::wstring v = DoReadConfigValue(name);
        if ( v.empty() )
            return false;
        std::wistringstream s(v);
        s >> value;
        return !s.fail();
    }

    static bool ReadConfigValue(const char *name, std::string& value)
    {
        const std::wstring v = DoReadConfigValue(name);
        if (v.empty())
            return false;
        value = WideToAnsi(v);
        return true;
    }

    static bool ReadConfigValue(const char *name, std::wstring& value)
    {
        value = DoReadConfigValue(name);
        return !value.empty();
    }

    // Reads a value from registry, substituting default value if not present.
    template<typename T>
    static bool ReadConfigValue(const char *name, T& value, const T& defval)
    {
        bool rv = ReadConfigValue(name, value);
        if ( !rv )
            value = defval;
        return rv;
    }

    // Deletes value from registry.
    static void DeleteConfigValue(const char *name);

    //@}

private:
    Settings(); // cannot be instantiated

    // Get given field from the VERSIONINFO/StringFileInfo resource (Windows),
    // Info.plist (macOS) or the host wxApp (elsewhere); throw on failure.
    // Supported fields: ProductName, ProductVersion, CompanyName, BuildVersion
    static std::wstring GetVerInfoField(const wchar_t *field)
        { return DoGetVerInfoField(field, true); }
    // Same, but don't throw if a field is not set
    static std::wstring TryGetVerInfoField(const wchar_t *field)
        { return DoGetVerInfoField(field, false); }
    static std::wstring DoGetVerInfoField(const wchar_t *field, bool fatal);
    // Gets custom win32 resource data (or its platform equivalent, e.g.
    // Info.plist keys on macOS); throws if not available
    static std::string GetCustomResource(const char *name, const char *type);

    static std::string GetDefaultRegistryPath();

    static void DoWriteConfigValue(const char *name, const wchar_t *value);
    static std::wstring DoReadConfigValue(const char *name);

    // default config storage: registry on Windows, INI file elsewhere
    static int WIN_SPARKLE_CALL DefaultConfigRead(const char *name, wchar_t *buf, size_t len, void *);
    static void WIN_SPARKLE_CALL DefaultConfigWrite(const char *name, const wchar_t *value, void *);
    static void WIN_SPARKLE_CALL DefaultConfigDelete(const char *name, void *);

private:
    // guards the variables below:
    static CriticalSection ms_csVars;

    static Lang         ms_lang;
    static std::string  ms_appcastURL;
    static std::string  ms_registryPath;
    static std::wstring ms_companyName;
    static std::wstring ms_appName;
    static std::wstring ms_appVersion;
    static std::wstring ms_appBuildVersion;
    static bool         ms_appVersionSetByApp;
    static std::string  ms_DSAPubKey;
    static std::string  ms_EdDSAPubKey;
    static std::map<std::string, std::string> ms_httpHeaders;
    static win_sparkle_config_methods_t ms_configMethods;
};

} // namespace winsparkle

#endif // _settings_h_
