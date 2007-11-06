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

#if !defined(AFX_COLORBUTTON_H__DF02109B_B91C_49FD_954F_74A48B83C314__INCLUDED_)
#define AFX_COLORBUTTON_H__DF02109B_B91C_49FD_954F_74A48B83C314__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ColorButton.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ColorButton window

class ColorButton : public CButton
{
  // Construction
 public:
  ColorButton();

  // Attributes
 public:
  // Operations
  static bool isRegistered;
 public:
  void PreSubclassWindow();
  void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(ColorButton)
  //}}AFX_VIRTUAL

  // Implementation
 public:
  void setColor(u16 c);
  u16 color;
  virtual ~ColorButton();

  void registerClass();

  // Generated message map functions
 protected:
  //{{AFX_MSG(ColorButton)
  // NOTE - the ClassWizard will add and remove member functions here.
  //}}AFX_MSG

  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COLORBUTTON_H__DF02109B_B91C_49FD_954F_74A48B83C314__INCLUDED_)
