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

#if !defined(AFX_ROMINFO_H__9888A45C_3E71_4C0F_B119_EFC74DFF8CD3__INCLUDED_)
#define AFX_ROMINFO_H__9888A45C_3E71_4C0F_B119_EFC74DFF8CD3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RomInfo.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// RomInfoGB dialog

class RomInfoGB : public CDialog
{
  // Construction
 public:
  RomInfoGB(u8 *rom, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(RomInfoGB)
  enum { IDD = IDD_GB_ROM_INFO };
  // NOTE: the ClassWizard will add data members here
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(RomInfoGB)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL
  u8 *rom;

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(RomInfoGB)
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////
// RomInfoGBA dialog

class RomInfoGBA : public CDialog
{
  // Construction
 public:
  RomInfoGBA(u8 *rom, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(RomInfoGBA)
  enum { IDD = IDD_GBA_ROM_INFO };
  // NOTE: the ClassWizard will add data members here
  //}}AFX_DATA
  u8 *rom;


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(RomInfoGBA)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(RomInfoGBA)
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ROMINFO_H__9888A45C_3E71_4C0F_B119_EFC74DFF8CD3__INCLUDED_)
