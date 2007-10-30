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

#if !defined(AFX_LOGGING_H__222FC21A_D40D_450D_8A1C_D33305E47B85__INCLUDED_)
#define AFX_LOGGING_H__222FC21A_D40D_450D_8A1C_D33305E47B85__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Logging.h : header file
//

#include "ResizeDlg.h"

/////////////////////////////////////////////////////////////////////////////
// Logging dialog

class Logging : public ResizeDlg
{
  // Construction
 public:
  void log(const char *);
  Logging(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(Logging)
  enum { IDD = IDD_LOGGING };
  CEdit  m_log;
  BOOL  m_swi;
  BOOL  m_unaligned_access;
  BOOL  m_illegal_write;
  BOOL  m_illegal_read;
  BOOL  m_dma0;
  BOOL  m_dma1;
  BOOL  m_dma2;
  BOOL  m_dma3;
  BOOL  m_agbprint;
  BOOL  m_undefined;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(Logging)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(Logging)
  afx_msg void OnOk();
  afx_msg void OnClear();
  afx_msg void OnVerboseAgbprint();
  afx_msg void OnVerboseDma0();
  afx_msg void OnVerboseDma1();
  afx_msg void OnVerboseDma2();
  afx_msg void OnVerboseDma3();
  afx_msg void OnVerboseIllegalRead();
  afx_msg void OnVerboseIllegalWrite();
  afx_msg void OnVerboseSwi();
  afx_msg void OnVerboseUnalignedAccess();
  afx_msg void OnVerboseUndefined();
  afx_msg void OnSave();
  afx_msg void OnErrspaceLog();
  afx_msg void OnMaxtextLog();
  virtual BOOL OnInitDialog();
  afx_msg void OnClose();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    public:
  static Logging *instance;
  static CString text;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LOGGING_H__222FC21A_D40D_450D_8A1C_D33305E47B85__INCLUDED_)
