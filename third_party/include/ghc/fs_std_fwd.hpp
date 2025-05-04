//---------------------------------------------------------------------------------------
//
// ghc::filesystem - A C++17-like filesystem implementation for C++11/C++14
//
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2018, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------
// fs_std_fwd.hpp - The forwarding header for the header/implementation separated usage of
//                  ghc::filesystem that uses std::filesystem if it detects it.
// This file can be include at any place, where fs::filesystem api is needed while
// not bleeding implementation details (e.g. system includes) into the global namespace,
// as long as one cpp includes fs_std_impl.hpp to deliver the matching implementations.
//---------------------------------------------------------------------------------------
#ifndef GHC_FILESYSTEM_STD_FWD_H
#define GHC_FILESYSTEM_STD_FWD_H

#if defined(_MSVC_LANG) && _MSVC_LANG >= 201703L || __cplusplus >= 201703L && defined(__has_include)
    // ^ Supports MSVC prior to 15.7 without setting /Zc:__cplusplus to fix __cplusplus
    // _MSVC_LANG works regardless. But without the switch, the compiler always reported 199711L: https://blogs.msdn.microsoft.com/vcblog/2018/04/09/msvc-now-correctly-reports-__cplusplus/
    #if __has_include(<filesystem>) // Two stage __has_include needed for MSVC 2015 and per https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005finclude.html
        #define GHC_USE_STD_FS

        // Old Apple OSs don't support std::filesystem, though the header is available at compile
        // time. In particular, std::filesystem is unavailable before macOS 10.15, iOS/tvOS 13.0,
        // and watchOS 6.0.
        #ifdef __APPLE__
            #include <Availability.h>
            // Note: This intentionally uses std::filesystem on any new Apple OS, like visionOS
            // released after std::filesystem, where std::filesystem is always available.
            // (All other __<platform>_VERSION_MIN_REQUIREDs will be undefined and thus 0.)
            #if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500 \
             || defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 130000 \
             || defined(__TV_OS_VERSION_MIN_REQUIRED) && __TV_OS_VERSION_MIN_REQUIRED < 130000 \
             || defined(__WATCH_OS_VERSION_MAX_ALLOWED) && __WATCH_OS_VERSION_MAX_ALLOWED < 60000
                #undef GHC_USE_STD_FS
            #endif
        #endif
    #endif
#endif

#ifdef GHC_USE_STD_FS
    #include <filesystem>
    namespace fs {
        using namespace std::filesystem;
        using ifstream = std::ifstream;
        using ofstream = std::ofstream;
        using fstream = std::fstream;
    }
#else
    #include "fs_fwd.hpp"
    namespace fs {
        using namespace ghc::filesystem;
        using ifstream = ghc::filesystem::ifstream;
        using ofstream = ghc::filesystem::ofstream;
        using fstream = ghc::filesystem::fstream;
    }
#endif

#endif // GHC_FILESYSTEM_STD_FWD_H
