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

#if !defined(AFX_GBMEMORYVIEWERDLG_H__23AD2804_EFA5_4900_AEC5_47196A41C50D__INCLUDED_)
#define AFX_GBMEMORYVIEWERDLG_H__23AD2804_EFA5_4900_AEC5_47196A41C50D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBMemoryViewerDlg.h : header file
//
#include "MemoryViewer.h"
#include "ResizeDlg.h"
#include "IUpdate.h"

class GBMemoryViewer : public MemoryViewer {
 public:
  GBMemoryViewer();
  virtual void readData(u32, int, u8 *);
  virtual void editData(u32, int, int, u32);
};

/////////////////////////////////////////////////////////////////////////////
// GBMemoryViewerDlg dialog

class GBMemoryViewerDlg : public ResizeDlg, IUpdateListener, IMemoryViewerDlg
{
  GBMemoryViewer m_viewer;
  bool autoUpdate;
  // Construction
 public:
  void setCurrentAddress(u32 address);
  GBMemoryViewerDlg(CWnd* pParent = NULL);   // standard constructor

  virtual void update();

  // Dialog Data
  //{{AFX_DATA(GBMemoryViewerDlg)
  enum { IDD = IDD_MEM_VIEWER };
  CEdit  m_current;
  CEdit  m_address;
  CComboBox  m_addresses;
  int    m_size;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBMemoryViewerDlg)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBMemoryViewerDlg)
  virtual BOOL OnInitDialog();
  afx_msg void OnClose();
  afx_msg void OnRefresh();
  afx_msg void On8Bit();
  afx_msg void On16Bit();
  afx_msg void On32Bit();
  afx_msg void OnAutoUpdate();
  afx_msg void OnGo();
  afx_msg void OnSelchangeAddresses();
  afx_msg void OnSave();
  afx_msg void OnLoad();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBMEMORYVIEWERDLG_H__23AD2804_EFA5_4900_AEC5_47196A41C50D__INCLUDED_)
