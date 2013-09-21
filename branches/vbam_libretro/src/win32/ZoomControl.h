#if !defined(AFX_ZOOMCONTROL_H__BC193230_D2D6_4240_93AE_28C2EF2C641A__INCLUDED_)
#define AFX_ZOOMCONTROL_H__BC193230_D2D6_4240_93AE_28C2EF2C641A__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ZoomControl.h : header file
//
#ifndef WM_COLINFO
#define WM_COLINFO WM_APP+100
#endif

/////////////////////////////////////////////////////////////////////////////
// ZoomControl window

class ZoomControl : public CWnd
{
  // Construction
 public:
  ZoomControl();

  // Attributes
 public:

  // Operations
 public:

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(ZoomControl)
  //}}AFX_VIRTUAL

  // Implementation
 public:
  void setColors(const u8 *c);
  static bool isRegistered;
  virtual ~ZoomControl();

  // Generated message map functions
 protected:
  //{{AFX_MSG(ZoomControl)
  afx_msg void OnPaint();
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    private:
  int selected;
  u8 colors[3*64];
  void registerClass();
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ZOOMCONTROL_H__BC193230_D2D6_4240_93AE_28C2EF2C641A__INCLUDED_)
