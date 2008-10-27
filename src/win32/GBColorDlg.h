#if !defined(AFX_GBCOLORDLG_H__8D6126EF_06BB_48CF_ABB3_2CC4B1B60358__INCLUDED_)
#define AFX_GBCOLORDLG_H__8D6126EF_06BB_48CF_ABB3_2CC4B1B60358__INCLUDED_

#include "ColorButton.h"  // Added by ClassView
#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBColorDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// GBColorDlg dialog

class GBColorDlg : public CDialog
{
  // Construction
 public:
  int getWhich();
  afx_msg void OnColorClicked(UINT id);
  u16 * getColors();
  void setWhich(int w);
  u16 colors[24];
  ColorButton colorControls[8];
  GBColorDlg(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(GBColorDlg)
	enum { IDD = IDD_GB_COLORS };
	CComboBox	m_predefined;
  int    which;
	//}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBColorDlg)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBColorDlg)
  afx_msg void OnDefault();
  afx_msg void OnReset();
  afx_msg void OnUser1();
  afx_msg void OnUser2();
  afx_msg void OnCancel();
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
	afx_msg void OnSelchangePredefined();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBCOLORDLG_H__8D6126EF_06BB_48CF_ABB3_2CC4B1B60358__INCLUDED_)
