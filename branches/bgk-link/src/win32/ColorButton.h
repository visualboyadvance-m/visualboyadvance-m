#if !defined(AFX_COLORBUTTON_H__DF02109B_B91C_49FD_954F_74A48B83C314__INCLUDED_)
#define AFX_COLORBUTTON_H__DF02109B_B91C_49FD_954F_74A48B83C314__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ColorButton.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ColorButton window

class ColorButton : public CButton
{
  // Construction
 public:
  ColorButton();

  // Attributes
 public:
  // Operations
  static bool isRegistered;
 public:
  void PreSubclassWindow();
  void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(ColorButton)
  //}}AFX_VIRTUAL

  // Implementation
 public:
  void setColor(u16 c);
  u16 color;
  virtual ~ColorButton();

  void registerClass();

  // Generated message map functions
 protected:
  //{{AFX_MSG(ColorButton)
  // NOTE - the ClassWizard will add and remove member functions here.
  //}}AFX_MSG

  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COLORBUTTON_H__DF02109B_B91C_49FD_954F_74A48B83C314__INCLUDED_)
