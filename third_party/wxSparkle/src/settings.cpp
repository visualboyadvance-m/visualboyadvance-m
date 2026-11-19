/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "settings.h"

#include "error.h"
#include "utils.h"
#include "threads.h"
#include "signatureverifier.h"

// Platform-specific parts are in settings_msw.cpp and settings_unix.cpp.

namespace winsparkle
{

CriticalSection Settings::ms_csVars;
Settings::Lang Settings::ms_lang;
std::string  Settings::ms_appcastURL;
std::string  Settings::ms_registryPath;
std::wstring Settings::ms_companyName;
std::wstring Settings::ms_appName;
std::wstring Settings::ms_appVersion;
std::wstring Settings::ms_appBuildVersion;
bool         Settings::ms_appVersionSetByApp = false;
std::string  Settings::ms_DSAPubKey;
std::string  Settings::ms_EdDSAPubKey;
std::map<std::string, std::string> Settings::ms_httpHeaders;

win_sparkle_config_methods_t Settings::ms_configMethods = GetDefaultConfigMethods();


/*--------------------------------------------------------------------------*
                             runtime config access
 *--------------------------------------------------------------------------*/

std::string Settings::GetDefaultRegistryPath()
{
    std::string s("Software\\");
    std::wstring vendor;
    try
    {
        vendor = Settings::GetCompanyName();
    }
    catch ( ... )
    {
        // vendor name is optional on platforms without VERSIONINFO
#ifdef _WIN32
        throw;
#endif
    }
    if ( !vendor.empty() )
        s += ToUtf8(vendor) + "\\";
    s += ToUtf8(Settings::GetAppName());
    s += "\\WinSparkle";

    return s;
}

namespace
{

// Critical section to guard DoWriteConfigValue/DoReadConfigValue.
CriticalSection g_csConfigValues;

} // anonymous namespace


void Settings::DoWriteConfigValue(const char *name, const wchar_t *value)
{
    CriticalSectionLocker lock(g_csConfigValues);

    // NULL methods mean "use the default implementation"
    auto write = ms_configMethods.config_write ? ms_configMethods.config_write : &DefaultConfigWrite;
    write(name, value, ms_configMethods.user_data);
}

std::wstring Settings::DoReadConfigValue(const char *name)
{
    CriticalSectionLocker lock(g_csConfigValues);

    static const int bufferLength = 512;
    wchar_t buf[bufferLength];
    buf[0] = 0;

    auto read = ms_configMethods.config_read ? ms_configMethods.config_read : &DefaultConfigRead;

    if (read(name, buf, bufferLength, ms_configMethods.user_data))
    {
        buf[bufferLength - 1] = 0;
        return buf;
    }
    else
        return std::wstring();
}

void Settings::DeleteConfigValue(const char *name)
{
    CriticalSectionLocker lock(g_csConfigValues);

    auto del = ms_configMethods.config_delete ? ms_configMethods.config_delete : &DefaultConfigDelete;
    del(name, ms_configMethods.user_data);
}

void Settings::SetDSAPubKeyPem(const std::string &pem)
{
    CriticalSectionLocker lock(ms_csVars);
    SignatureVerifier::VerifyDSAPubKeyPem(pem);
    ms_DSAPubKey = pem;
}

void Settings::SetEdDSAPubKey(const std::string& pubkey_base64)
{
    CriticalSectionLocker lock(ms_csVars);
    SignatureVerifier::VerifyEdDSAPubKey(pubkey_base64);
    ms_EdDSAPubKey = pubkey_base64;
}

} // namespace winsparkle
