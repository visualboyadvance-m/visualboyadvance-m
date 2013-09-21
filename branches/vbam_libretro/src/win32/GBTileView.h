#if !defined(AFX_GBTILEVIEW_H__C8C8DEBB_17ED_4C5C_9DBE_D730A49B312C__INCLUDED_)
#define AFX_GBTILEVIEW_H__C8C8DEBB_17ED_4C5C_9DBE_D730A49B312C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBTileView.h : header file
//
#include "BitmapControl.h"
#include "ColorControl.h"
#include "IUpdate.h"
#include "ResizeDlg.h"
#include "ZoomControl.h"

/////////////////////////////////////////////////////////////////////////////
// GBTileView dialog

class GBTileView : public ResizeDlg, IUpdateListener
{
  int charBase;
  int palette;
  int bank;
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
  void renderTile(int tile, int x, int y, u8 *charBase);
  void savePNG(const char *name);
  void saveBMP(const char *name);
  GBTileView(CWnd* pParent = NULL);   // standard constructor
  virtual ~GBTileView();

  virtual void update();

  // Dialog Data
  //{{AFX_DATA(GBTileView)
  enum { IDD = IDD_GB_TILE_VIEWER };
  CSliderCtrl  m_slider;
  int    m_charBase;
  int    m_bank;
  BOOL  m_stretch;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBTileView)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:
  virtual afx_msg LRESULT OnMapInfo(WPARAM wParam, LPARAM lParam);
  virtual afx_msg LRESULT OnColInfo(WPARAM wParam, LPARAM lParam);

  // Generated message map functions
  //{{AFX_MSG(GBTileView)
  afx_msg void OnSave();
  virtual BOOL OnInitDialog();
  afx_msg void OnClose();
  afx_msg void OnAutoUpdate();
  afx_msg void OnCharbase0();
  afx_msg void OnCharbase1();
  afx_msg void OnBank0();
  afx_msg void OnBank1();
  afx_msg void OnStretch();
  afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBTILEVIEW_H__C8C8DEBB_17ED_4C5C_9DBE_D730A49B312C__INCLUDED_)
