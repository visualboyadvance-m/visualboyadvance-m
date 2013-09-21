#if !defined(AFX_TILEVIEW_H__055751EC_2DF3_495B_B643_29025465CD2E__INCLUDED_)
#define AFX_TILEVIEW_H__055751EC_2DF3_495B_B643_29025465CD2E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TileView.h : header file
//

#include "BitmapControl.h"
#include "ColorControl.h"
#include "IUpdate.h"
#include "ResizeDlg.h"
#include "ZoomControl.h"

/////////////////////////////////////////////////////////////////////////////
// TileView dialog

class TileView : public ResizeDlg, IUpdateListener
{
  int charBase;
  int is256Colors;
  int palette;
  BitmapControl tileView;
  BITMAPINFO bmpInfo;
  u8 *data;
  ZoomControl zoom;
  ColorControl color;
  int w;
  int h;
  bool autoUpdate;
  // Construction
 public:
  void paint();
  void render();
  void renderTile16(int tile, int x, int y, u8 *charBase, u16 *palette);
  void renderTile256(int tile, int x, int y, u8 *charBase, u16 *palette);
  void savePNG(const char *name);
  void saveBMP(const char *name);
  TileView(CWnd* pParent = NULL);   // standard constructor
  virtual ~TileView();

  virtual void update();

  // Dialog Data
  //{{AFX_DATA(TileView)
  enum { IDD = IDD_TILE_VIEWER };
  CSliderCtrl  m_slider;
  int    m_colors;
  int    m_charBase;
  BOOL  m_stretch;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(TileView)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:
  virtual afx_msg LRESULT OnMapInfo(WPARAM wParam, LPARAM lParam);
  virtual afx_msg LRESULT OnColInfo(WPARAM wParam, LPARAM lParam);

  // Generated message map functions
  //{{AFX_MSG(TileView)
  afx_msg void OnSave();
  virtual BOOL OnInitDialog();
  afx_msg void OnClose();
  afx_msg void OnAutoUpdate();
  afx_msg void On16Colors();
  afx_msg void On256Colors();
  afx_msg void OnCharbase0();
  afx_msg void OnCharbase1();
  afx_msg void OnCharbase2();
  afx_msg void OnCharbase3();
  afx_msg void OnCharbase4();
  afx_msg void OnStretch();
  afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TILEVIEW_H__055751EC_2DF3_495B_B643_29025465CD2E__INCLUDED_)
