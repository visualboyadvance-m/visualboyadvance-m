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

#if !defined(AFX_GBDISASSEMBLE_H__3EFD5B47_6DBF_4F63_8F91_A9511EC590EB__INCLUDED_)
#define AFX_GBDISASSEMBLE_H__3EFD5B47_6DBF_4F63_8F91_A9511EC590EB__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBDisassemble.h : header file
//

#include "IUpdate.h"
#include "ResizeDlg.h"

/////////////////////////////////////////////////////////////////////////////
// GBDisassemble dialog

class GBDisassemble : public ResizeDlg, IUpdateListener
{
  // Construction
 public:
  void refresh();
  u16 lastAddress;
  int count;
  bool autoUpdate;
  u16 address;
  GBDisassemble(CWnd* pParent = NULL);   // standard constructor

  virtual void update();

  // Dialog Data
  //{{AFX_DATA(GBDisassemble)
  enum { IDD = IDD_GB_DISASSEMBLE };
  CEdit  m_address;
  CListBox  m_list;
  BOOL  m_c;
  BOOL  m_h;
  BOOL  m_n;
  BOOL  m_z;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBDisassemble)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBDisassemble)
  afx_msg void OnClose();
  afx_msg void OnRefresh();
  afx_msg void OnNext();
  afx_msg void OnGo();
  afx_msg void OnGopc();
  afx_msg void OnAutoUpdate();
  virtual BOOL OnInitDialog();
  afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBDISASSEMBLE_H__3EFD5B47_6DBF_4F63_8F91_A9511EC590EB__INCLUDED_)
