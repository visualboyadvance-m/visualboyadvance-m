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

#if !defined(AFX_LANGSELECT_H__63619E13_A375_4ED4_952F_DCF8998C2914__INCLUDED_)
#define AFX_LANGSELECT_H__63619E13_A375_4ED4_952F_DCF8998C2914__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LangSelect.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// LangSelect dialog

class LangSelect : public CDialog
{
  // Construction
 public:
  LangSelect(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(LangSelect)
  enum { IDD = IDD_LANG_SELECT };
  CEdit  m_langString;
  CStatic  m_langName;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(LangSelect)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(LangSelect)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LANGSELECT_H__63619E13_A375_4ED4_952F_DCF8998C2914__INCLUDED_)
