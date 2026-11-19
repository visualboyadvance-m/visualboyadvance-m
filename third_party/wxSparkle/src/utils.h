/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _utils_h_
#define _utils_h_

#include "error.h"

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <string.h>

#include "platform.h"

#ifdef _WIN32
    #include <windows.h>
#endif
#if WXSPARKLE_CLASSIC_MAC
    #include <wx/string.h>
#endif

namespace winsparkle
{

/// Helper class for RIIA handling of allocated buffers
template<typename T>
struct DataBuffer
{
    DataBuffer(size_t size) : data(size) {}

    operator T*() { return data.data(); }
    operator const T*() const { return data.data(); }

    std::vector<T> data;
};


// Simple conversion between wide and narrow strings (only safe for ASCII!):

template<typename TIn, typename TOut>
inline std::basic_string<TOut> ConvertString(const std::basic_string<TIn>& s)
{
    std::basic_string<TOut> out;
    out.reserve(s.length());

    for ( typename std::basic_string<TIn>::const_iterator i = s.begin(); i != s.end(); ++i )
    {
        out += static_cast<TOut>(*i);
    }

    return out;
}

inline std::string WideToAnsi(const std::wstring& s)
{
    return ConvertString<wchar_t, char>(s);
}

inline std::wstring AnsiToWide(const std::string& s)
{
    return ConvertString<char, wchar_t>(s);
}


// Proper Unicode conversion between UTF-8 and wide strings (UTF-16 on
// Windows, UTF-32 elsewhere). Invalid sequences are replaced with U+FFFD.

inline std::string ToUtf8(const std::wstring& s)
{
    std::string out;
    out.reserve(s.size());
    for ( size_t i = 0; i < s.size(); ++i )
    {
        uint32_t cp = static_cast<uint32_t>(s[i]);
#if WCHAR_MAX <= 0xFFFF
        if ( cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() )
        {
            uint32_t lo = static_cast<uint32_t>(s[i + 1]);
            if ( lo >= 0xDC00 && lo <= 0xDFFF )
            {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
#endif
        if ( (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF )
            cp = 0xFFFD;

        if ( cp < 0x80 )
        {
            out += static_cast<char>(cp);
        }
        else if ( cp < 0x800 )
        {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if ( cp < 0x10000 )
        {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

inline std::wstring FromUtf8(const std::string& s)
{
    std::wstring out;
    out.reserve(s.size());
    size_t i = 0;
    const size_t n = s.size();
    while ( i < n )
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp;
        size_t extra;
        if ( c < 0x80 )                { cp = c;        extra = 0; }
        else if ( (c & 0xE0) == 0xC0 ) { cp = c & 0x1F; extra = 1; }
        else if ( (c & 0xF0) == 0xE0 ) { cp = c & 0x0F; extra = 2; }
        else if ( (c & 0xF8) == 0xF0 ) { cp = c & 0x07; extra = 3; }
        else                           { out += wchar_t(0xFFFD); ++i; continue; }

        if ( extra && i + extra >= n ) // truncated sequence
        {
            out += wchar_t(0xFFFD);
            break;
        }

        bool ok = true;
        for ( size_t k = 1; k <= extra; ++k )
        {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ( (cc & 0xC0) != 0x80 ) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if ( !ok )
        {
            out += wchar_t(0xFFFD);
            ++i;
            continue;
        }
        i += extra + 1;

        if ( cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) )
            cp = 0xFFFD;

#if WCHAR_MAX <= 0xFFFF
        if ( cp >= 0x10000 )
        {
            cp -= 0x10000;
            out += static_cast<wchar_t>(0xD800 + (cp >> 10));
            out += static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
            continue;
        }
#endif
        out += static_cast<wchar_t>(cp);
    }
    return out;
}


// Conversion between wide strings (used by the public API) and native paths.

inline std::filesystem::path PathFromWide(const std::wstring& s)
{
#ifdef _WIN32
    return std::filesystem::path(s);
#elif WXSPARKLE_CLASSIC_MAC
    // HFS path in the file system encoding (e.g. Mac Roman), as understood
    // by the C library
    const wxCharBuffer buf = wxString(s).fn_str();
    if ( !buf.data() )
        throw std::runtime_error("Invalid file name");
    return std::filesystem::path(std::string(buf.data()));
#else
    return std::filesystem::path(ToUtf8(s));
#endif
}

inline std::wstring PathToWide(const std::filesystem::path& p)
{
#ifdef _WIN32
    return p.wstring();
#elif WXSPARKLE_CLASSIC_MAC
    return wxString(p.native().c_str(), *wxConvFileName).ToStdWstring();
#else
    return FromUtf8(p.string());
#endif
}


// Base64 handling (replacement for CryptStringToBinary/CryptBinaryToString)

inline std::string Base64Encode(const void *data, size_t len)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const unsigned char *p = static_cast<const unsigned char*>(data);
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    for ( ; i + 2 < len; i += 3 )
    {
        const uint32_t v = (uint32_t(p[i]) << 16) | (uint32_t(p[i + 1]) << 8) | p[i + 2];
        out += alphabet[(v >> 18) & 0x3F];
        out += alphabet[(v >> 12) & 0x3F];
        out += alphabet[(v >> 6) & 0x3F];
        out += alphabet[v & 0x3F];
    }
    if ( i < len )
    {
        uint32_t v = uint32_t(p[i]) << 16;
        if ( i + 1 < len )
            v |= uint32_t(p[i + 1]) << 8;
        out += alphabet[(v >> 18) & 0x3F];
        out += alphabet[(v >> 12) & 0x3F];
        out += (i + 1 < len) ? alphabet[(v >> 6) & 0x3F] : '=';
        out += '=';
    }
    return out;
}

/// Decodes base64 data, ignoring whitespace. Throws on invalid input.
inline std::string Base64Decode(const std::string& base64)
{
    std::string out;
    out.reserve(base64.size() * 3 / 4);

    uint32_t acc = 0;
    int bits = 0;
    int padding = 0;
    size_t symbols = 0;

    for ( char ch : base64 )
    {
        int v;
        if ( ch >= 'A' && ch <= 'Z' )      v = ch - 'A';
        else if ( ch >= 'a' && ch <= 'z' ) v = ch - 'a' + 26;
        else if ( ch >= '0' && ch <= '9' ) v = ch - '0' + 52;
        else if ( ch == '+' || ch == '-' ) v = 62; // also accept base64url
        else if ( ch == '/' || ch == '_' ) v = 63;
        else if ( ch == '=' )              { padding++; symbols++; continue; }
        else if ( ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ) continue;
        else
            throw std::runtime_error("Failed to decode base64 string");

        if ( padding )
            throw std::runtime_error("Failed to decode base64 string");

        symbols++;
        acc = (acc << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if ( bits >= 8 )
        {
            bits -= 8;
            out += static_cast<char>((acc >> bits) & 0xFF);
        }
    }

    if ( padding > 2 || (padding && symbols % 4 != 0) || symbols % 4 == 1 )
        throw std::runtime_error("Failed to decode base64 string");

    return out;
}


#ifdef _WIN32

// Checking of Windows version

inline bool IsWindowsVistaOrGreater()
{
    OSVERSIONINFOEXW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    DWORDLONG const dwlConditionMask = VerSetConditionMask(
        VerSetConditionMask(
        VerSetConditionMask(
        0, VER_MAJORVERSION, VER_GREATER_EQUAL),
        VER_MINORVERSION, VER_GREATER_EQUAL),
        VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    osvi.dwMajorVersion = HIBYTE(_WIN32_WINNT_VISTA);
    osvi.dwMinorVersion = LOBYTE(_WIN32_WINNT_VISTA);
    osvi.wServicePackMajor = 0;

    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}


// Dynamic loading of symbols that may be unavailable on earlier
// versions of Windows

template<typename T>
inline T* LoadDynamicFunc(const char *func, const char *dll)
{
    HMODULE mod = GetModuleHandleA(dll);
    if (!mod)
        return NULL;
    return reinterpret_cast<T*>(reinterpret_cast<void*>(GetProcAddress(mod, func)));
}

#define LOAD_DYNAMIC_FUNC(func, dll) \
    LoadDynamicFunc<decltype(func)>(#func, #dll)

#endif // _WIN32


// Check for insecure URLs
inline bool CheckForInsecureURL(const std::string& url, const std::string& purpose)
{
    if (url.compare(0, 8, "https://") != 0)
    {
        LogError("----------------------------");
        LogError("*** USING INSECURE URL: " + purpose + " from " + url + " ***");
        LogError("----------------------------");
        return false;
    }
    return true;
}

} // namespace winsparkle

#endif // _utils_h_
