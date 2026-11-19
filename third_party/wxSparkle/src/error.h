/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _error_h_
#define _error_h_

#include <string>
#include <stdexcept>

namespace winsparkle
{

#ifdef _WIN32
/**
    Exception thrown if a Platform SDK error happens.

    This exception automatically sets the error message to the appropriate
    error message for GetLastError()'s error code.

    Only used by Windows-specific code.
 */
class Win32Exception : public std::runtime_error
{
public:
    /**
        Creates the exception.

        @param extraMsg  Extra message shown in front of the win32 error.
     */
    Win32Exception(const char *extraMsg = NULL);
};
#endif // _WIN32

/**
    Logs error to the debug output (Windows) or standard error (elsewhere).
 */
void LogError(const char *msg);

inline void LogError(const std::string& msg)
{
    LogError(msg.c_str());
}

/// Logs usage warning
void LogWarning(const char *msg);

inline void LogWarning(const std::string& msg)
{
    LogWarning(msg.c_str());
}


/**
    Helper macro for catching exceptions in DLL API interface.

    Usage:
    @code
    try
    {
        ...
    }
    CATCH_ALL_EXCEPTIONS
    @endcode
 */
#define CATCH_ALL_EXCEPTIONS                                        \
    catch (const std::exception& e)                                 \
    {                                                               \
        winsparkle::LogError(e.what());                             \
    }                                                               \
    catch (...)                                                     \
    {                                                               \
        winsparkle::LogError("Unknown error.");                     \
    }


} // namespace winsparkle

#endif // _error_h_
