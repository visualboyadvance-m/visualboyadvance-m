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

#if !defined(AFX_VIDEOMODE_H__074B2426_32EA_4D69_9215_AB5E90F885D0__INCLUDED_)
#define AFX_VIDEOMODE_H__074B2426_32EA_4D69_9215_AB5E90F885D0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// VideoMode.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// VideoMode dialog

class VideoMode : public CDialog
{
  // Construction
 public:
  VideoMode(LPDIRECTDRAW7 pDraw,CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(VideoMode)
  enum { IDD = IDD_MODES };
  CListBox  m_modes;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(VideoMode)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(VideoMode)
  afx_msg void OnSelchangeModes();
  afx_msg void OnCancel();
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    private:
  LPDIRECTDRAW7 pDirectDraw;
};

/////////////////////////////////////////////////////////////////////////////
// VideoDriverSelect dialog

class VideoDriverSelect : public CDialog
{
  // Construction
 public:
  VideoDriverSelect(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(VideoDriverSelect)
  enum { IDD = IDD_DRIVERS };
  CListBox  m_drivers;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(VideoDriverSelect)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(VideoDriverSelect)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeDrivers();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
#endif // !defined(AFX_VIDEOMODE_H__074B2426_32EA_4D69_9215_AB5E90F885D0__INCLUDED_)
