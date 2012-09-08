#if !defined(AFX_REWINDINTERVAL_H__C95AFF44_1F64_44C8_BAAB_A54B982D28EA__INCLUDED_)
#define AFX_REWINDINTERVAL_H__C95AFF44_1F64_44C8_BAAB_A54B982D28EA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RewindInterval.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// RewindInterval dialog

class RewindInterval : public CDialog
{
  // Construction
 public:
  int interval;
  RewindInterval(int interval, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(RewindInterval)
  enum { IDD = IDD_REWIND_INTERVAL };
  CEdit  m_interval;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(RewindInterval)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(RewindInterval)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REWINDINTERVAL_H__C95AFF44_1F64_44C8_BAAB_A54B982D28EA__INCLUDED_)
