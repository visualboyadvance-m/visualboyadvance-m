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

#if !defined(AFX_ACCELEDITOR_H__66F5C854_E28E_40D1_B763_1850374B46A2__INCLUDED_)
#define AFX_ACCELEDITOR_H__66F5C854_E28E_40D1_B763_1850374B46A2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AccelEditor.h : header file
//
#include "AcceleratorManager.h"
#include "KeyboardEdit.h"
#include "ResizeDlg.h"

/////////////////////////////////////////////////////////////////////////////
// AccelEditor dialog

class AccelEditor : public ResizeDlg
{
  // Construction
 public:
  CAcceleratorManager mgr;
  void InitCommands();
  AccelEditor(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(AccelEditor)
  enum { IDD = IDD_ACCEL_EDITOR };
  CListBox  m_currents;
  CStatic  m_alreadyAffected;
  CListBox  m_commands;
  CKeyboardEdit  m_key;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(AccelEditor)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(AccelEditor)
  virtual BOOL OnInitDialog();
  afx_msg void OnCancel();
  afx_msg void OnOk();
  afx_msg void OnSelchangeCommands();
  afx_msg void OnReset();
  afx_msg void OnAssign();
  afx_msg void OnRemove();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ACCELEDITOR_H__66F5C854_E28E_40D1_B763_1850374B46A2__INCLUDED_)
