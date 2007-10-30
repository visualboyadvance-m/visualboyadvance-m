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

#if !defined(AFX_MAPVIEW_H__20F40C77_8E10_44B7_BB49_7865F73C3E75__INCLUDED_)
#define AFX_MAPVIEW_H__20F40C77_8E10_44B7_BB49_7865F73C3E75__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MapView.h : header file
//

#include "BitmapControl.h"
#include "ColorControl.h"
#include "ZoomControl.h"
#include "ResizeDlg.h"
#include "IUpdate.h"
#include "..\System.h"  // Added by ClassView

/////////////////////////////////////////////////////////////////////////////
// MapView dialog

class MapView : public ResizeDlg, IUpdateListener
{
 private:
  BITMAPINFO bmpInfo;
  u8 *data;
  int frame;
  u16 control;
  int bg;
  int w;
  int h;
  BitmapControl mapView;
  ZoomControl mapViewZoom;
  ColorControl color;
  bool autoUpdate;

  // Construction
 public:
  void savePNG(const char *name);
  void saveBMP(const char *name);
  afx_msg LRESULT OnColInfo(WPARAM wParam, LPARAM lParam);
  afx_msg LRESULT OnMapInfo(WPARAM wParam, LPARAM lParam);
  u32 GetClickAddress(int x, int y);
  u32 GetTextClickAddress(u32 base, int x, int y);
  void update();
  void enableButtons(int mode);
  void paint();
  void renderMode5();
  void renderMode4();
  void renderMode3();
  void renderMode2();
  void renderMode1();
  void renderMode0();
  void renderRotScreen(u16 control);
  void renderTextScreen(u16 control);
  MapView(CWnd* pParent = NULL);   // standard constructor
  ~MapView();

  // Dialog Data
  //{{AFX_DATA(MapView)
  enum { IDD = IDD_MAP_VIEW };
  CStatic  m_numcolors;
  CStatic  m_mode;
  CStatic  m_overflow;
  CStatic  m_mosaic;
  CStatic  m_priority;
  CStatic  m_dim;
  CStatic  m_charbase;
  CStatic  m_mapbase;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(MapView)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(MapView)
  afx_msg void OnRefresh();
  virtual BOOL OnInitDialog();
  afx_msg void OnFrame0();
  afx_msg void OnFrame1();
  afx_msg void OnBg0();
  afx_msg void OnBg1();
  afx_msg void OnBg2();
  afx_msg void OnBg3();
  afx_msg void OnStretch();
  afx_msg void OnAutoUpdate();
  afx_msg void OnClose();
  afx_msg void OnSave();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAPVIEW_H__20F40C77_8E10_44B7_BB49_7865F73C3E75__INCLUDED_)
