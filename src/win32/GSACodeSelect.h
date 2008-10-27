#if !defined(AFX_GSACODESELECT_H__189BD94D_288F_4E2A_9395_EAB16F104D87__INCLUDED_)
#define AFX_GSACODESELECT_H__189BD94D_288F_4E2A_9395_EAB16F104D87__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GSACodeSelect.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// GSACodeSelect dialog

class GSACodeSelect : public CDialog
{
  // Construction
 public:
  GSACodeSelect(FILE *file, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(GSACodeSelect)
  enum { IDD = IDD_CODE_SELECT };
  CListBox  m_games;
  //}}AFX_DATA
  FILE *m_file;


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GSACodeSelect)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GSACodeSelect)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  afx_msg void OnSelchangeGameList();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GSACODESELECT_H__189BD94D_288F_4E2A_9395_EAB16F104D87__INCLUDED_)
