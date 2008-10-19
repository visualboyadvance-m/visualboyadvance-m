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

#if !defined(AFX_MEMORYVIEWERADDRESSSIZE_H__04605262_2B1D_4EED_A467_B6C56AC2CACD__INCLUDED_)
#define AFX_MEMORYVIEWERADDRESSSIZE_H__04605262_2B1D_4EED_A467_B6C56AC2CACD__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MemoryViewerAddressSize.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// MemoryViewerAddressSize dialog

class MemoryViewerAddressSize : public CDialog
{
  u32 address;
  int size;
  // Construction
 public:
  int getSize();
  u32 getAddress();
  void setSize(int s);
  void setAddress(u32 a);
  MemoryViewerAddressSize(u32 a=0xffffff, int s=-1, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(MemoryViewerAddressSize)
  enum { IDD = IDD_ADDR_SIZE };
  CEdit  m_size;
  CEdit  m_address;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(MemoryViewerAddressSize)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(MemoryViewerAddressSize)
  virtual BOOL OnInitDialog();
  afx_msg void OnOk();
  afx_msg void OnCancel();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MEMORYVIEWERADDRESSSIZE_H__04605262_2B1D_4EED_A467_B6C56AC2CACD__INCLUDED_)
