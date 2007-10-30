// -*- C++ -*-
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

// FileDlg.h: interface for the FileDlg class.
//
//////////////////////////////////////////////////////////////////////
#if !defined(AFX_FILEDLG_H__7E4F8B92_1B63_4126_8261_D9334C645940__INCLUDED_)
#define AFX_FILEDLG_H__7E4F8B92_1B63_4126_8261_D9334C645940__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FileDlg.h : header file
//

struct OPENFILENAMEEX : public OPENFILENAME {
  void *        pvReserved;
  DWORD         dwReserved;
  DWORD         FlagsEx;
};

/////////////////////////////////////////////////////////////////////////////
// FileDlg dialog

class FileDlg
{
 private:
  CString m_file;
  CString m_filter;
 public:
  OPENFILENAMEEX m_ofn;
  int DoModal();
  LPCTSTR GetPathName();
  virtual int getFilterIndex();
  virtual void OnTypeChange(HWND hwnd);
  FileDlg(CWnd *parent, LPCTSTR file, LPCTSTR filter,
          int filterIndex, LPCTSTR ext, LPCTSTR *exts, LPCTSTR initialDir, 
          LPCTSTR title, bool save);
  virtual ~FileDlg();
  
 protected:
  bool isSave;
  LPCTSTR *extensions;

 protected:
    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.
};

#endif // !defined(AFX_FILEDLG_H__7E4F8B92_1B63_4126_8261_D9334C645940__INCLUDED_)
