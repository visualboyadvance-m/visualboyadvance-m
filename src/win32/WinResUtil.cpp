// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "stdafx.h"

static HINSTANCE winResGetInstance(LPCTSTR resType, LPCTSTR resName)
{
  // TODO: make language DLL first
  return AfxFindResourceHandle(resName, resType);
}


UCHAR *winResGetResource(LPCTSTR resType, LPCTSTR resName)
{
  HINSTANCE winResInstance = winResGetInstance(resType, resName);

  HRSRC hRsrc = FindResourceEx(winResInstance, resType, resName, 0);

  if(hRsrc != NULL) {
    HGLOBAL hGlobal = LoadResource(winResInstance, hRsrc);

    if(hGlobal != NULL) {
      UCHAR * b = (UCHAR *)LockResource(hGlobal);

      return b;
    }
  }
  return NULL;
}

HMENU winResLoadMenu(LPCTSTR menuName)
{
  UCHAR * b = winResGetResource(RT_MENU, menuName);
  
  if(b != NULL) {
    HMENU menu = LoadMenuIndirect((CONST MENUTEMPLATE *)b);
    
    if(menu != NULL)
      return menu;
  }

  return LoadMenu(NULL, menuName);
}

int winResDialogBox(LPCTSTR boxName,
                    HWND parent,
                    DLGPROC dlgProc,
                    LPARAM lParam)
{
  /*
    UCHAR * b = winResGetResource(RT_DIALOG, boxName);
  
    if(b != NULL) {
    
    return DialogBoxIndirectParam(hInstance,
    (LPCDLGTEMPLATE)b,
    parent,
    dlgProc,
    lParam);
    }

    return DialogBoxParam(hInstance,
    boxName,
    parent,
    dlgProc,
    lParam);
  */
  return 0;
}

int winResDialogBox(LPCTSTR boxName,
                    HWND parent,
                    DLGPROC dlgProc)
{
  return winResDialogBox(boxName,
                         parent,
                         dlgProc,
                         0);
}

CString winResLoadString(UINT id)
{
  int stId = id / 16 + 1;
  HINSTANCE inst = winResGetInstance(RT_STRING, MAKEINTRESOURCE(stId));
  
  CString res;
  if(res.LoadString(id))
    return res;

  // TODO: handle case where string is only in the default English
  res = "";

  return res;
}
