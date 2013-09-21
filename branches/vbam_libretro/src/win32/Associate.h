#if !defined(AFX_ASSOCIATE_H__3326525B_B405_40A7_82C4_B2594669A930__INCLUDED_)
#define AFX_ASSOCIATE_H__3326525B_B405_40A7_82C4_B2594669A930__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Associate.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// Associate dialog

class Associate : public CDialog
{
  // Construction
 public:
  Associate(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(Associate)
  enum { IDD = IDD_ASSOCIATIONS };
  BOOL  m_agb;
  BOOL  m_bin;
  BOOL  m_cgb;
  BOOL  m_dmg;
  BOOL  m_gb;
  BOOL  m_gba;
  BOOL  m_gbc;
  BOOL  m_sgb;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(Associate)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(Associate)
  virtual BOOL OnInitDialog();
  afx_msg void OnCancel();
  afx_msg void OnOk();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ASSOCIATE_H__3326525B_B405_40A7_82C4_B2594669A930__INCLUDED_)
