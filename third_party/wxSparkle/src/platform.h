/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _platform_h_
#define _platform_h_

// Classic Mac OS (System 7 to Mac OS 9) built with the Retro68 toolchain, for
// 68k (Retro68) or PowerPC (RetroPPC) Macs.
//
// The platform has no POSIX file system API (paths are HFS paths, handled by
// wxWidgets' wxRetro68 port), its C++ library has no std::thread/std::mutex
// (threads are cooperative Thread Manager threads provided by
// pthreads-retro68) and networking goes through MacTCP (libmactcp).
#if defined(macintosh) && !defined(__APPLE__) && !defined(_WIN32)
    #define WXSPARKLE_CLASSIC_MAC 1
#else
    #define WXSPARKLE_CLASSIC_MAC 0
#endif

#if WXSPARKLE_CLASSIC_MAC

class wxString;

namespace winsparkle
{

/**
    Opens the downloaded update file (e.g. a StuffIt archive or a disk image)
    using the Finder, as if the user double-clicked it.

    Files downloaded over HTTP have no file type and creator, so they are set
    from the file name's extension first, for the Finder to know which
    application (StuffIt Expander, Disk Copy, ...) to open it with.

    @param path  Full HFS path of the file.
 */
bool ClassicMacOpenFile(const wxString& path);

} // namespace winsparkle

#endif // WXSPARKLE_CLASSIC_MAC

#endif // _platform_h_
