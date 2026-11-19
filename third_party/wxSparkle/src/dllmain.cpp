/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

// Only compiled when building a Windows DLL.

#include "ui.h"

#include <windows.h>

/*--------------------------------------------------------------------------*
                               DllMain() hook
 *--------------------------------------------------------------------------*/

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if ( dwReason == DLL_PROCESS_ATTACH )
    {
        winsparkle::UI::SetDllHINSTANCE(hInstance);
    }

    return TRUE;
}
