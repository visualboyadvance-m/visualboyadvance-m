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

#if !defined(AFX_BITMAPCONTROL_H__2434AADB_B6A5_4E43_AA16_7B65B6F7FA26__INCLUDED_)
#define AFX_BITMAPCONTROL_H__2434AADB_B6A5_4E43_AA16_7B65B6F7FA26__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// BitmapControl.h : header file
//
#ifndef WM_MAPINFO
#define WM_MAPINFO WM_APP+101
#endif

/////////////////////////////////////////////////////////////////////////////
// BitmapControl view

class BitmapControl : public CScrollView
{
 public:
  BitmapControl();           // protected constructor used by dynamic creation
 protected:
  DECLARE_DYNCREATE(BitmapControl)

    // Attributes
    public:

  // Operations
 public:
  void setStretch(bool b);
  void refresh();
  void setSize(int w1, int h1);
  void setData(u8 *d);
  void setBmpInfo(BITMAPINFO *info);
  static bool isRegistered;

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(BitmapControl)
 protected:
  virtual void OnDraw(CDC* pDC);      // overridden to draw this view
  virtual void OnInitialUpdate();     // first time after construct
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 public:
  bool getStretch();
  virtual ~BitmapControl();
 protected:
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

  // Generated message map functions
  //{{AFX_MSG(BitmapControl)
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    private:
  void registerClass();
  bool stretch;
  u8 colors[3*64];
  BITMAPINFO *bmpInfo;
  u8 * data;
  int h;
  int w;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BITMAPCONTROL_H__2434AADB_B6A5_4E43_AA16_7B65B6F7FA26__INCLUDED_)
