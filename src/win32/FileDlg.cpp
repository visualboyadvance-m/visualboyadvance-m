// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team

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

// FileDlg.cpp: implementation of the FileDlg class.
//

#include "stdafx.h"
#include <commdlg.h>
#include <dlgs.h>

#include "VBA.h"
#include "FileDlg.h"
#include "../System.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static FileDlg *instance = NULL;

static UINT_PTR CALLBACK HookFunc(HWND hwnd,
                                  UINT msg,
                                  WPARAM wParam,
                                  LPARAM lParam)
{
  if(instance) {
    if(msg == WM_NOTIFY) {
      OFNOTIFY *notify = (OFNOTIFY *)lParam;
      if(notify) {
        if(notify->hdr.code == CDN_TYPECHANGE) {
          instance->OnTypeChange(hwnd);
          return 1;
        }
      }
    }
  }
  return 0;
}

static UINT_PTR CALLBACK HookFuncOldStyle(HWND hwnd,
                                          UINT msg,
                                          WPARAM wParam,
                                          LPARAM lParam)
{
  if(instance) {
    if(msg == WM_COMMAND) {
      if(HIWORD(wParam) == CBN_SELCHANGE) {
        if(LOWORD(wParam) == cmb1) {
          // call method with combobox handle to keep 
          // behaviour there
          instance->OnTypeChange((HWND)lParam);
          return 1;
        }
      }
    }
  }
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// FileDlg

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FileDlg::FileDlg(CWnd *parent, LPCTSTR file, LPCTSTR filter,
                 int filterIndex, LPCTSTR ext, LPCTSTR *exts, LPCTSTR initialDir, 
                 LPCTSTR title, bool save)
{
  OSVERSIONINFO info;
  info.dwOSVersionInfoSize = sizeof(info);
  GetVersionEx(&info);
  m_file = file;
  int size = sizeof(OPENFILENAME);
  
  // avoid problems if OPENFILENAME is already defined with the extended fields
  // needed for the enhanced open/save dialog
#if _WIN32_WINNT < 0x0500
  if(info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    if(info.dwMajorVersion >= 5)
      size = sizeof(OPENFILENAMEEX);
  }
#endif

  ZeroMemory(&m_ofn, sizeof(m_ofn));
  m_ofn.lpstrFile = m_file.GetBuffer(MAX_PATH);
  m_ofn.nMaxFile = MAX_PATH;
  m_ofn.lStructSize = size;
  m_ofn.hwndOwner = parent ? parent->GetSafeHwnd() : NULL;
  m_ofn.nFilterIndex = filterIndex;
  m_ofn.lpstrInitialDir = initialDir;
  m_ofn.lpstrTitle = title;
  m_ofn.lpstrDefExt = ext;
  m_ofn.lpfnHook = HookFunc;
  m_ofn.Flags = OFN_PATHMUSTEXIST | OFN_ENABLESIZING | OFN_ENABLEHOOK;
  m_ofn.Flags |= OFN_EXPLORER;
  m_filter = filter;
  
  char *p = m_filter.GetBuffer(0);
  
  while ((p = strchr(p, '|')) != NULL)
    *p++ = 0;
  m_ofn.lpstrFilter = m_filter;

  if(theApp.videoOption == VIDEO_320x240) {
    m_ofn.lpTemplateName = MAKEINTRESOURCE(IDD_OPENDLG);
    m_ofn.lpfnHook = HookFuncOldStyle;
    m_ofn.Flags |= OFN_ENABLETEMPLATE;
    m_ofn.Flags &= ~OFN_EXPLORER;
  }

  isSave = save;
  extensions = exts;

  instance = this;
}

FileDlg::~FileDlg()
{
  instance = NULL;
}

void FileDlg::OnTypeChange(HWND hwnd)
{
  HWND parent = GetParent(hwnd);

  HWND fileNameControl = ::GetDlgItem(parent, cmb13);

  if(fileNameControl == NULL)
    fileNameControl = ::GetDlgItem(parent, edt1);

  if(fileNameControl == NULL)
    return;
  
  CString filename;
  GetWindowText(fileNameControl, filename.GetBuffer(MAX_PATH), MAX_PATH);
  filename.ReleaseBuffer();

  HWND typeControl = ::GetDlgItem(parent, cmb1);

  ASSERT(typeControl != NULL);

  LRESULT sel = ::SendMessage(typeControl, CB_GETCURSEL, 0, 0);

  ASSERT(sel != -1);
  
  LPCTSTR typeName = extensions[sel];
  
  if(filename.GetLength() == 0) {
    if(strlen(typeName) != 0)
      filename.Format("*%s", typeName);
  } else {
    if(strlen(typeName) != 0) {
      int index = filename.Find('.');
      if (index == -1) {
        filename = filename + typeName;
      } else {
        filename = filename.Left(index) + typeName;
      }
    }
  }
  SetWindowText(fileNameControl, filename);
}

int FileDlg::getFilterIndex()
{
  return m_ofn.nFilterIndex;
}

int FileDlg::DoModal()
{
  BOOL res = isSave ? GetSaveFileName(&m_ofn) :
    GetOpenFileName(&m_ofn);

  return res ? IDOK : IDCANCEL;
}

LPCTSTR FileDlg::GetPathName()
{
  return (LPCTSTR)m_file;
}
