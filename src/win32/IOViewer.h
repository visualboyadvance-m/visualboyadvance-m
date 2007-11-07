// -*- C++ -*-
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

#if !defined(AFX_IOVIEWER_H__9C266B78_FC02_4572_9062_0241802B0E76__INCLUDED_)
#define AFX_IOVIEWER_H__9C266B78_FC02_4572_9062_0241802B0E76__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// IOViewer.h : header file
//

#include "ResizeDlg.h"
#include "IUpdate.h"

/////////////////////////////////////////////////////////////////////////////
// IOViewer dialog

class IOViewer : public ResizeDlg, IUpdateListener
{
  // Construction
 public:
  void update();
  void bitChange();
  bool autoUpdate;
  int selected;
  IOViewer(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(IOViewer)
	enum { IDD = IDD_IO_VIEWER };
	CStatic	m_value;
  CComboBox  m_address;
	//}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(IOViewer)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(IOViewer)
  afx_msg void OnClose();
  afx_msg void OnRefresh();
  afx_msg void OnAutoUpdate();
  afx_msg void OnSelchangeAddresses();
  virtual BOOL OnInitDialog();
	afx_msg void OnApply();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IOVIEWER_H__9C266B78_FC02_4572_9062_0241802B0E76__INCLUDED_)
