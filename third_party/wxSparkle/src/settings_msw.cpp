/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

// Windows implementation of the platform-specific parts of Settings:
// VERSIONINFO and custom resources for metadata, registry for storage.

#ifdef _WIN32

#include "settings.h"

#include "error.h"
#include "utils.h"

#include <windows.h>
#include <cwchar>

namespace winsparkle
{

/*--------------------------------------------------------------------------*
                             resources access
 *--------------------------------------------------------------------------*/

namespace
{

// return language code for the resource to query
std::wstring GetVerInfoLang(void *fi)
{
    struct TranslationInfo
    {
        WORD language;
        WORD codepage;
    } *translations;
    unsigned translationsCnt;

    if ( !VerQueryValueW(fi, L"\\VarFileInfo\\Translation",
                         (LPVOID*)&translations, &translationsCnt) )
        throw Win32Exception("Executable doesn't have required VERSIONINFO\\VarFileInfo resource");

    translationsCnt /= sizeof(struct TranslationInfo);
    if ( translationsCnt == 0 )
        throw std::runtime_error("No translations in VarFileInfo resource?");

    // TODO: be smarter about which language to use:
    //       1. use app main thread's locale
    //       2. failing that, try language-neutral or English
    //       3. use the first one as fallback
    const size_t idx = 0;

    wchar_t lang[9];
    if ( swprintf(lang, 9, L"%04x%04x",
                  translations[idx].language,
                  translations[idx].codepage) < 0 )
        throw std::runtime_error("Failed to format VERSIONINFO language");

    return lang;
}

} // anonymous namespace


std::wstring Settings::DoGetVerInfoField(const wchar_t *field, bool fatal)
{
    // VERSIONINFO has no separate build number, ProductVersion is used
    if ( wcscmp(field, L"BuildVersion") == 0 )
        return std::wstring();

    wchar_t exeFilename[MAX_PATH + 1];

    if ( !GetModuleFileNameW(NULL, exeFilename, MAX_PATH) )
        throw Win32Exception();

    DWORD unusedHandle;
    DWORD fiSize = GetFileVersionInfoSizeW(exeFilename, &unusedHandle);
    if ( fiSize == 0 )
        throw Win32Exception("Executable doesn't have the required VERSIONINFO resource");

    DataBuffer<unsigned char> fi(fiSize);

    if ( !GetFileVersionInfoW(exeFilename, unusedHandle, fiSize, fi) )
        throw Win32Exception();

    const std::wstring key =
        L"\\StringFileInfo\\" + GetVerInfoLang(fi) + L"\\" + field;

    wchar_t *value;
    UINT len;
    if ( !VerQueryValueW(fi, key.c_str(), (LPVOID*)&value, &len) )
    {
        if ( fatal )
            throw Win32Exception("Executable doesn't have required key in StringFileInfo");
        else
            return std::wstring();
    }

    return value;
}


std::string Settings::GetCustomResource(const char *name, const char *type)
{
    const HINSTANCE module = 0; // main executable
    HRSRC hRes = FindResourceA(module, name, type);
    if ( hRes )
    {
        HGLOBAL hData = LoadResource(module, hRes);
        if ( hData )
        {
            const char *data = (const char*)::LockResource(hData);
            size_t size = ::SizeofResource(module, hRes);

            if ( data && size )
            {
                if ( data[size-1] == '\0' ) // null-terminated string
                    size--;
                return std::string(data, size);
            }
        }
    }

    std::string err;
    err += "Failed to get resource \"";
    err += name;
    err += "\" (type \"";
    err += type;
    err += "\")";
    throw Win32Exception(err.c_str());
}


/*--------------------------------------------------------------------------*
                             runtime config access
 *--------------------------------------------------------------------------*/

void WIN_SPARKLE_CALL Settings::DefaultConfigWrite(const char *name, const wchar_t *value, void *)
{
    const std::wstring subkey = FromUtf8(Settings::GetRegistryPath());

    HKEY key;
    LONG result = RegCreateKeyExW
                  (
                      HKEY_CURRENT_USER,
                      subkey.c_str(),
                      0,
                      NULL,
                      REG_OPTION_NON_VOLATILE,
                      KEY_SET_VALUE,
                      NULL,
                      &key,
                      NULL
                  );
    if (result != ERROR_SUCCESS)
        throw Win32Exception("Cannot write settings to registry");

    result = RegSetValueExW
             (
                 key,
                 AnsiToWide(name).c_str(),
                 0,
                 REG_SZ,
                 (const BYTE*)value,
                 (DWORD)((wcslen(value) + 1) * sizeof(wchar_t))
             );

    RegCloseKey(key);

    if (result != ERROR_SUCCESS)
        throw Win32Exception("Cannot write settings to registry");
}

void WIN_SPARKLE_CALL Settings::DefaultConfigDelete(const char *name, void *)
{
    const std::wstring subkey = FromUtf8(Settings::GetRegistryPath());

    HKEY key;
    LONG result = RegOpenKeyExW
                  (
                      HKEY_CURRENT_USER,
                      subkey.c_str(),
                      0,
                      KEY_SET_VALUE,
                      &key
                  );
    if (result != ERROR_SUCCESS)
        throw Win32Exception("Cannot delete settings from registry");

    result = RegDeleteValueW(key, AnsiToWide(name).c_str());

    RegCloseKey(key);

    if (result != ERROR_SUCCESS)
        throw Win32Exception("Cannot delete settings from registry");
}


static int DoRegistryRead(HKEY root, const char *name, wchar_t *buf, size_t len)
{
    const std::wstring subkey = FromUtf8(Settings::GetRegistryPath());

    HKEY key;
    LONG result = RegOpenKeyExW
                  (
                      root,
                      subkey.c_str(),
                      0,
                      KEY_QUERY_VALUE,
                      &key
                  );
    if ( result != ERROR_SUCCESS )
    {
        if ( result == ERROR_FILE_NOT_FOUND )
            return 0;
        throw Win32Exception("Cannot read settings from registry");
    }

    DWORD buflen = (DWORD)len;
    DWORD type;
    result = RegQueryValueExW
             (
                 key,
                 AnsiToWide(name).c_str(),
                 0,
                 &type,
                 (BYTE*)buf,
                 &buflen
             );

    RegCloseKey(key);

    if ( result != ERROR_SUCCESS )
    {
        if ( result == ERROR_FILE_NOT_FOUND )
            return 0;
        throw Win32Exception("Cannot read settings from registry");
    }

    if ( type != REG_SZ )
    {
        // incorrect type -- pretend that the setting doesn't exist, it will
        // be newly written by WinSparkle anyway
        return 0;
    }

    return 1;
}

int WIN_SPARKLE_CALL Settings::DefaultConfigRead(const char *name, wchar_t *buf, size_t len, void *)
{
    size_t bytes = len * sizeof(wchar_t);
    // Try reading from HKCU first. If that fails, look at HKLM too, in case
    // some settings have globally set values (either by the installer or the
    // administrator).
    if (DoRegistryRead(HKEY_CURRENT_USER, name, buf, bytes))
    {
        return 1;
    }
    else
    {
        return DoRegistryRead(HKEY_LOCAL_MACHINE, name, buf, bytes);
    }
}

} // namespace winsparkle

#endif // _WIN32
