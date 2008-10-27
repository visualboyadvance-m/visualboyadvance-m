#if !defined(AFX_OAMVIEW_H__E5369352_80F8_49C4_9F23_05EB6FC1345B__INCLUDED_)
#define AFX_OAMVIEW_H__E5369352_80F8_49C4_9F23_05EB6FC1345B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OamView.h : header file
//
#include "BitmapControl.h"
#include "ZoomControl.h"
#include "ColorControl.h"

#include "IUpdate.h"
#include "ResizeDlg.h"

/////////////////////////////////////////////////////////////////////////////
// OamView dialog

class OamView : public ResizeDlg, IUpdateListener
{
 private:
  BITMAPINFO bmpInfo;
  u8 *data;
  int w;
  int h;
  int number;
  bool autoUpdate;
  BitmapControl oamView;
  ZoomControl oamZoom;
  ColorControl color;

  // Construction
 public:
  void updateScrollInfo();
  afx_msg LRESULT OnColInfo(WPARAM wParam, LPARAM lParam);
  afx_msg LRESULT OnMapInfo(WPARAM wParam, LPARAM lParam);
  void savePNG(const char *name);
  void saveBMP(const char *name);
  void render();
  void setAttributes(u16 a0, u16 a1, u16 a2);
  void paint();
  ~OamView();
  OamView(CWnd* pParent = NULL);   // standard constructor

  virtual void update();
  // Dialog Data
  //{{AFX_DATA(OamView)
  enum { IDD = IDD_OAM_VIEW };
  CEdit  m_sprite;
  BOOL  m_stretch;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(OamView)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(OamView)
  afx_msg void OnSave();
  virtual BOOL OnInitDialog();
  afx_msg void OnStretch();
  afx_msg void OnAutoUpdate();
  afx_msg void OnChangeSprite();
  afx_msg void OnClose();
  afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OAMVIEW_H__E5369352_80F8_49C4_9F23_05EB6FC1345B__INCLUDED_)
