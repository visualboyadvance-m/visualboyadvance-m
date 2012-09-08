#if !defined(AFX_COLORCONTROL_H__747E1E47_DDFA_4D67_B337_A473F2BACB86__INCLUDED_)
#define AFX_COLORCONTROL_H__747E1E47_DDFA_4D67_B337_A473F2BACB86__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ColorControl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ColorControl window

class ColorControl : public CWnd
{
  // Construction
 public:
  ColorControl();

  // Attributes
 public:
  // Operations
  static bool isRegistered;

  // Operations
 public:

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(ColorControl)
  //}}AFX_VIRTUAL

  // Implementation
 public:
  void setColor(u16 c);
  u16 color;
  virtual ~ColorControl();

  // Generated message map functions
 protected:
  //{{AFX_MSG(ColorControl)
  afx_msg void OnPaint();
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    private:
  void registerClass();
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COLORCONTROL_H__747E1E47_DDFA_4D67_B337_A473F2BACB86__INCLUDED_)
