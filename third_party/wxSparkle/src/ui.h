/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _ui_h_
#define _ui_h_

#include "threads.h"
#include "appcast.h"

namespace winsparkle
{

enum ErrorCode
{
    Err_Generic,
    Err_BadSignature
};

/**
    The user interface (available updates, download progress etc.).

    All functions may be called from any thread. The UI itself always runs
    in the GUI thread:

    - If the host application is a wxWidgets application (wxTheApp exists),
      the UI runs in its main thread and calls are marshalled there using
      wxEvtHandler::CallAfter(). This is the only mode supported on macOS
      and GTK, where GUI must be used from the main thread.

    - On Windows, if the host application doesn't use wxWidgets (e.g. when
      wxSparkle is used as a self-contained DLL, like WinSparkle), a
      dedicated UI thread running its own wxApp is created when needed.
 */
class UI
{
public:
    /**
        Shuts the UI down.

        @note Currently, this may only be called once, from
              win_sparkle_cleanup().
     */
    static void ShutDown();

    /**
        Notifies the UI that no updates were found.

        If the UI is running and WinSparkle window is currently open
        (i.e. the user is manually checking for updates), "no updates found"
        message is shown. Otherwise, does nothing.
     */
    static void NotifyNoUpdates(bool installAutomatically);

    /**
        Notifies the UI that there was an error retrieving updates.

        If the UI is running and WinSparkle window is currently open
        (i.e. the user is manually checking for updates), "update error"
        message is shown. Otherwise, does nothing.
     */
    static void NotifyUpdateError(ErrorCode err = Err_Generic);

    /**
        Notifies the UI that a new version is available.

        If the UI isn't running yet, it will be launched.
     */
    static void NotifyUpdateAvailable(const Appcast& info, bool installAutomatically);

    /**
        Notifies the UI about download progress.
     */
    static void NotifyDownloadProgress(size_t downloaded, size_t total);

    /**
        Notifies the UI that an update was downloaded.
     */
    static void NotifyUpdateDownloaded(const std::wstring& updateFile, const Appcast &appcast);

    /**
        Shows the WinSparkle window in "checking for updates..." state.
     */
    static void ShowCheckingUpdates();

    /**
        Shows the dialog asking user for permission to check for updates.
     */
    static void AskForPermission();

#ifdef _WIN32
    /**
        Sets HINSTANCE (HMODULE) of the DLL.

        Must be called on DLL initialization when built as a DLL.
     */
    static void SetDllHINSTANCE(void *h) { ms_hInstance = h; }

    static void *GetDllHINSTANCE() { return ms_hInstance; }

private:
    static void *ms_hInstance;
#endif // _WIN32

private:
    UI() = delete;
};

} // namespace winsparkle

#endif // _ui_h_
