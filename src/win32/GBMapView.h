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

#if !defined(AFX_GBMAPVIEW_H__4CD23D38_F2CD_4B95_AE76_2781591DD077__INCLUDED_)
#define AFX_GBMAPVIEW_H__4CD23D38_F2CD_4B95_AE76_2781591DD077__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBMapView.h : header file
//

#include "BitmapControl.h"
#include "ColorControl.h"
#include "ZoomControl.h"
#include "ResizeDlg.h"
#include "IUpdate.h"
#include "..\System.h"  // Added by ClassView

/////////////////////////////////////////////////////////////////////////////
// GBMapView dialog

class GBMapView : public ResizeDlg, IUpdateListener
{
 private:
  BITMAPINFO bmpInfo;
  u8 *data;
  int bank;
  int bg;
  int w;
  int h;
  BitmapControl mapView;
  ZoomControl mapViewZoom;
  ColorControl color;
  bool autoUpdate;
  // Construction
 public:
  afx_msg LRESULT OnColInfo(WPARAM wParam, LPARAM lParam);
  afx_msg LRESULT OnMapInfo(WPARAM wParam, LPARAM lParam);
  u32 GetClickAddress(int x, int y);
  void update();
  void paint();
  void render();
  void savePNG(const char *name);
  void saveBMP(const char *name);
  ~GBMapView();
  GBMapView(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(GBMapView)
  enum { IDD = IDD_GB_MAP_VIEW };
  // NOTE: the ClassWizard will add data members here
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBMapView)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBMapView)
  afx_msg void OnSave();
  afx_msg void OnRefresh();
  virtual BOOL OnInitDialog();
  afx_msg void OnBg0();
  afx_msg void OnBg1();
  afx_msg void OnBank0();
  afx_msg void OnBank1();
  afx_msg void OnStretch();
  afx_msg void OnAutoUpdate();
  afx_msg void OnClose();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBMAPVIEW_H__4CD23D38_F2CD_4B95_AE76_2781591DD077__INCLUDED_)
