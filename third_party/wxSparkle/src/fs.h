/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _fs_h_
#define _fs_h_

// winsparkle::fs is std::filesystem, or ghc::filesystem where the C++ library
// has none for the deployment target: Apple's libc++ only provides it from
// macOS 10.15 on.

#ifdef __APPLE__
    #include <Availability.h>
#endif

#if defined(__APPLE__) && defined(__MAC_OS_X_VERSION_MIN_REQUIRED) \
    && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
    // (a ghc-filesystem target puts the ghc directory itself on the path)
    #if __has_include(<ghc/filesystem.hpp>)
        #include <ghc/filesystem.hpp>
    #elif __has_include(<filesystem.hpp>)
        #include <filesystem.hpp>
    #else
        #error "std::filesystem needs macOS 10.15; ghc::filesystem is required for earlier targets"
    #endif
    namespace winsparkle { namespace fs = ghc::filesystem; }
#else
    #include <filesystem>
    namespace winsparkle { namespace fs = std::filesystem; }
#endif

#endif // _fs_h_
