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

#if !defined(AFX_PALETTEVIEWCONTROL_H__31F600AE_B7E5_4F6C_80B6_55E4B61FBD57__INCLUDED_)
#define AFX_PALETTEVIEWCONTROL_H__31F600AE_B7E5_4F6C_80B6_55E4B61FBD57__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PaletteViewControl.h : header file
//
#define WM_PALINFO WM_APP+1

/////////////////////////////////////////////////////////////////////////////
// PaletteViewControl window

class PaletteViewControl : public CWnd
{
  int w;
  int h;
  int colors;
  u8 *data;
  BITMAPINFO bmpInfo;
  static bool isRegistered;
  int selected;
 protected:
  u16 palette[256];
  int paletteAddress;  
  // Construction
 public:
  PaletteViewControl();

  virtual void updatePalette()=0;

  // Attributes
 public:

  // Operations
 public:

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(PaletteViewControl)
  //}}AFX_VIRTUAL

  // Implementation
 public:
  void registerClass();
  void refresh();
  void render(u16 color, int x, int y);
  void setSelected(int s);
  void setPaletteAddress(int address);
  bool saveJASCPAL(const char *name);
  bool saveMSPAL(const char *name);
  bool saveAdobe(const char *name);
  void init(int c, int w, int h);
  virtual ~PaletteViewControl();

  // Generated message map functions
 protected:
  //{{AFX_MSG(PaletteViewControl)
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnPaint();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PALETTEVIEWCONTROL_H__31F600AE_B7E5_4F6C_80B6_55E4B61FBD57__INCLUDED_)
