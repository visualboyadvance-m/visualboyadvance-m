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

#if !defined(AFX_JOYPAD_H__FFFB2470_9EEC_4D2D_A5F0_3BF31579999A__INCLUDED_)
#define AFX_JOYPAD_H__FFFB2470_9EEC_4D2D_A5F0_3BF31579999A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Joypad.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// JoypadEditControl window

class JoypadEditControl : public CEdit
{
  // Construction
 public:
  JoypadEditControl();

  // Attributes
 public:

  // Operations
 public:

  // Implementation
 public:
  virtual BOOL PreTranslateMessage(MSG *pMsg);
  afx_msg LRESULT OnJoyConfig(WPARAM wParam, LPARAM lParam);
  virtual ~JoypadEditControl();

  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// JoypadConfig dialog

class JoypadConfig : public CDialog
{
  // Construction
 public:
  void assignKeys();
  void assignKey(int id, LONG_PTR key);
  JoypadConfig(int w, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(JoypadConfig)
  enum { IDD = IDD_CONFIG };
  JoypadEditControl  up;
  JoypadEditControl  speed;
  JoypadEditControl  right;
  JoypadEditControl  left;
  JoypadEditControl  down;
  JoypadEditControl  capture;
  JoypadEditControl  buttonStart;
  JoypadEditControl  buttonSelect;
  JoypadEditControl  buttonR;
  JoypadEditControl  buttonL;
  JoypadEditControl  buttonGS;
  JoypadEditControl  buttonB;
  JoypadEditControl  buttonA;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(JoypadConfig)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:
  UINT_PTR timerId;
  int which;

  // Generated message map functions
  //{{AFX_MSG(JoypadConfig)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  afx_msg void OnDestroy();
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
    /////////////////////////////////////////////////////////////////////////////
// MotionConfig dialog

class MotionConfig : public CDialog
{
  // Construction
 public:
  void assignKeys();
  void assignKey(int id, LONG_PTR key);
  MotionConfig(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(MotionConfig)
  enum { IDD = IDD_MOTION_CONFIG };
  JoypadEditControl  up;
  JoypadEditControl  right;
  JoypadEditControl  left;
  JoypadEditControl  down;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(MotionConfig)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  afx_msg void OnCancel();
  afx_msg void OnOk();
  afx_msg void OnDestroy();
  virtual BOOL OnInitDialog();
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  DECLARE_MESSAGE_MAP()
    private:
  UINT_PTR timerId;
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_JOYPAD_H__FFFB2470_9EEC_4D2D_A5F0_3BF31579999A__INCLUDED_)
