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
