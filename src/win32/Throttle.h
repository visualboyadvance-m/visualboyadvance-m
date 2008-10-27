#if !defined(AFX_THROTTLE_H__5F03B6E9_0C43_4933_A7BC_1618428C2B7F__INCLUDED_)
#define AFX_THROTTLE_H__5F03B6E9_0C43_4933_A7BC_1618428C2B7F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Throttle.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// Throttle dialog

class Throttle : public CDialog
{
  // Construction
 public:
  Throttle(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(Throttle)
  enum { IDD = IDD_THROTTLE };
  int    m_throttle;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(Throttle)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(Throttle)
  virtual BOOL OnInitDialog();
  afx_msg void OnCancel();
  afx_msg void OnOk();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_THROTTLE_H__5F03B6E9_0C43_4933_A7BC_1618428C2B7F__INCLUDED_)
