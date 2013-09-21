#if !defined(AFX_LANGSELECT_H__63619E13_A375_4ED4_952F_DCF8998C2914__INCLUDED_)
#define AFX_LANGSELECT_H__63619E13_A375_4ED4_952F_DCF8998C2914__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LangSelect.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// LangSelect dialog

class LangSelect : public CDialog
{
  // Construction
 public:
  LangSelect(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(LangSelect)
  enum { IDD = IDD_LANG_SELECT };
  CEdit  m_langString;
  CStatic  m_langName;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(LangSelect)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(LangSelect)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LANGSELECT_H__63619E13_A375_4ED4_952F_DCF8998C2914__INCLUDED_)
