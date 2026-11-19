/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "error.h"

#include "platform.h"

#include <string>
#include <cstdio>
#if !WXSPARKLE_CLASSIC_MAC
    #include <mutex>
#endif

#ifdef _WIN32
    #include <windows.h>
#endif

namespace winsparkle
{

#ifdef _WIN32

/*--------------------------------------------------------------------------*
                          Win32Exception class
 *--------------------------------------------------------------------------*/

namespace
{

std::string GetWin32ErrorMessage(const char *extraMsg, DWORD err)
{
    std::string msg;

    if ( extraMsg )
    {
        msg = extraMsg;
        msg += ": ";
    }

    LPSTR buf;
    bool ok = FormatMessageA
              (
                  FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  0, // source - not set
                  err,
                  0, // language - use best
                  (LPSTR)&buf,
                  0,
                  NULL
              ) != 0;

    if ( ok )
    {
        msg += buf;
        LocalFree(buf);
    }

    return msg;
}

} // anonymous namespace

Win32Exception::Win32Exception(const char *extraMsg)
    : std::runtime_error(GetWin32ErrorMessage(extraMsg, GetLastError()))
{
}

#endif // _WIN32


/*--------------------------------------------------------------------------*
                                 Logging
 *--------------------------------------------------------------------------*/

void LogError(const char *msg)
{
    std::string err("WinSparkle: ");
    err.append(msg ? msg : "(null)");
    err.append("\n");

#ifdef _WIN32
    OutputDebugStringA(err.c_str());
#elif WXSPARKLE_CLASSIC_MAC
    // There's no std::mutex, but no lock is needed either: threads are
    // cooperative and stdio calls don't yield to other threads.
    fputs(err.c_str(), stderr);
    fflush(stderr);
#else
    static std::mutex s_logMutex;
    std::lock_guard<std::mutex> lock(s_logMutex);
    fputs(err.c_str(), stderr);
    fflush(stderr);
#endif
}

void LogWarning(const char *msg)
{
    LogError(msg);
}

} // namespace winsparkle
