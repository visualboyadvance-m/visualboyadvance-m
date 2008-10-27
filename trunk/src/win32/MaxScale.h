#if !defined(AFX_MAXSCALE_H__3F42C0CC_DD5E_4A96_A60D_33AB7CBDE406__INCLUDED_)
#define AFX_MAXSCALE_H__3F42C0CC_DD5E_4A96_A60D_33AB7CBDE406__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MaxScale.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// MaxScale dialog

class MaxScale : public CDialog
{
// Construction
public:
	MaxScale(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(MaxScale)
	enum { IDD = IDD_MAX_SCALE };
	CEdit	m_value;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(MaxScale)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(MaxScale)
	afx_msg void OnCancel();
	afx_msg void OnOk();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAXSCALE_H__3F42C0CC_DD5E_4A96_A60D_33AB7CBDE406__INCLUDED_)
