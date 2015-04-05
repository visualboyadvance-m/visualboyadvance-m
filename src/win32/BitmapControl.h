#if !defined(AFX_BITMAPCONTROL_H__2434AADB_B6A5_4E43_AA16_7B65B6F7FA26__INCLUDED_)
#define AFX_BITMAPCONTROL_H__2434AADB_B6A5_4E43_AA16_7B65B6F7FA26__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// BitmapControl.h : header file
//
#ifndef WM_MAPINFO
#define WM_MAPINFO WM_APP+101
#endif
#ifndef WM_BITMAPCLICK
#define WM_BITMAPCLICK WM_APP+102
#endif
/////////////////////////////////////////////////////////////////////////////
// BitmapControl view

class BitmapControl : public CScrollView
{
 public:
  BitmapControl();           // protected constructor used by dynamic creation
 protected:
  DECLARE_DYNCREATE(BitmapControl)

    // Attributes
    public:

  // Operations
 public:
  void setStretch(bool b);
  void refresh();
  void setSize(int w1, int h1);
  void setSelectedRectangle(int x, int y, int width, int height);
  void setData(u8 *d);
  void setBmpInfo(BITMAPINFO *info);
  static bool isRegistered;

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(BitmapControl)
 protected:
  virtual void OnDraw(CDC* pDC);      // overridden to draw this view
  virtual void OnInitialUpdate();     // first time after construct
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 public:
  bool getStretch();
  virtual ~BitmapControl();
 protected:
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

  // Generated message map functions
  //{{AFX_MSG(BitmapControl)
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    private:
  void registerClass();
  bool stretch;
  u8 colors[3*64];
  BITMAPINFO *bmpInfo;
  u8 * data;
  int h;
  int w;
  RECT boxreigon;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BITMAPCONTROL_H__2434AADB_B6A5_4E43_AA16_7B65B6F7FA26__INCLUDED_)
