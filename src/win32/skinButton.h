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

#if !defined(AFX_SKINBUTTON_H__E51B4507_EAD7_43EE_9F54_204BC485D59C__INCLUDED_)
#define AFX_SKINBUTTON_H__E51B4507_EAD7_43EE_9F54_204BC485D59C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// skinButton.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// SkinButton window

class SkinButton : public CWnd
{
  // Construction
 public:
  SkinButton();

  // Attributes
 private:
  HBITMAP normalBmp;
  HBITMAP downBmp;
  HBITMAP overBmp;
  RECT rect;
  bool mouseOver;
  CString id;
  HRGN region;
  WORD idCommand;
  int buttonMask;
  int menu;

  // Operations
 public:
  BOOL CreateButton(const char *, DWORD, const RECT&, CWnd *, UINT);

  void SetNormalBitmap(HBITMAP);
  void SetDownBitmap(HBITMAP);
  void SetOverBitmap(HBITMAP);
  void SetRect(const RECT &);
  void GetRect(RECT& r);
  void SetId(const char *);
  void SetRegion(HRGN);

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(SkinButton)
  //}}AFX_VIRTUAL

  // Implementation
 public:
  afx_msg LRESULT OnMouseLeaveMsg(WPARAM wParam, LPARAM lParam);
  afx_msg LRESULT OnMouseMoveMsg(WPARAM wParam, LPARAM lParam);
  afx_msg LRESULT OnLButtonDownMsg(WPARAM wParam, LPARAM lParam);
  afx_msg LRESULT OnLButtonUpMsg(WPARAM wParam, LPARAM lParam);
  virtual ~SkinButton();

  // Generated message map functions
 protected:
  //{{AFX_MSG(SkinButton)
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnPaint();
  afx_msg void OnKillFocus(CWnd* pNewWnd);
  afx_msg void OnCaptureChanged(CWnd *pWnd);
  afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SKINBUTTON_H__E51B4507_EAD7_43EE_9F54_204BC485D59C__INCLUDED_)
