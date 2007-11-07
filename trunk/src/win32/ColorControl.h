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

#if !defined(AFX_COLORCONTROL_H__747E1E47_DDFA_4D67_B337_A473F2BACB86__INCLUDED_)
#define AFX_COLORCONTROL_H__747E1E47_DDFA_4D67_B337_A473F2BACB86__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ColorControl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ColorControl window

class ColorControl : public CWnd
{
  // Construction
 public:
  ColorControl();

  // Attributes
 public:
  // Operations
  static bool isRegistered;

  // Operations
 public:

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(ColorControl)
  //}}AFX_VIRTUAL

  // Implementation
 public:
  void setColor(u16 c);
  u16 color;
  virtual ~ColorControl();

  // Generated message map functions
 protected:
  //{{AFX_MSG(ColorControl)
  afx_msg void OnPaint();
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    private:
  void registerClass();
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COLORCONTROL_H__747E1E47_DDFA_4D67_B337_A473F2BACB86__INCLUDED_)
