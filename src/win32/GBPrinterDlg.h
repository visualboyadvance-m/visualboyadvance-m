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

#if !defined(AFX_GBPRINTER_H__3180CC5A_1F9D_47E5_B044_407442CB40A4__INCLUDED_)
#define AFX_GBPRINTER_H__3180CC5A_1F9D_47E5_B044_407442CB40A4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBPrinter.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// GBPrinter dialog

class GBPrinterDlg : public CDialog
{
 private:
  u8 bitmapHeader[sizeof(BITMAPINFO)+4*sizeof(RGBQUAD)];
  BITMAPINFO *bitmap;
  u8 bitmapData[160*144];
  int scale;
  // Construction
 public:
  void processData(u8 *data);
  void saveAsPNG(const char *name);
  void saveAsBMP(const char *name);
  GBPrinterDlg(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(GBPrinterDlg)
  enum { IDD = IDD_GB_PRINTER };
  int    m_scale;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBPrinterDlg)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBPrinterDlg)
  afx_msg void OnSave();
  afx_msg void OnPrint();
  virtual BOOL OnInitDialog();
  afx_msg void OnOk();
  afx_msg void On1x();
  afx_msg void On2x();
  afx_msg void On3x();
  afx_msg void On4x();
  afx_msg void OnPaint();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBPRINTER_H__3180CC5A_1F9D_47E5_B044_407442CB40A4__INCLUDED_)
