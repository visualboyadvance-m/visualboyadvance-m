/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _updatedownloader_h_
#define _updatedownloader_h_

#include "threads.h"
#include "appcast.h"

#include <string>

namespace winsparkle
{

/**
    This class performs application update.

    If an update is found, then UpdateChecker initializes the GUI thread
    and shows information about available update to the user.
 */
class UpdateDownloader : public Thread
{
public:
    /// Creates updater thread.
    UpdateDownloader(const Appcast& appcast);

    /**
        Perform any necessary cleanup after previous updates.

        Should be called on launch to get rid of leftover junk from previous
        updates, such as the installer files. Call it as soon as possible,
        before using other WinSparkle functionality.
     */
    static void CleanLeftovers();

protected:
    // Thread methods:
    virtual void Run();
    virtual bool IsJoinable() const { return true; }

private:
    Appcast m_appcast;
};

} // namespace winsparkle

#endif // _updatedownloader_h_
