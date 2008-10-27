#if !defined(AFX_HYPERLINK_H__BECEAB7D_31FB_4727_A42B_8732162EEBCC__INCLUDED_)
#define AFX_HYPERLINK_H__BECEAB7D_31FB_4727_A42B_8732162EEBCC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Hyperlink.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// Hyperlink window

class Hyperlink : public CStatic
{
// Construction
public:
	Hyperlink();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Hyperlink)
	protected:
	virtual void PreSubclassWindow();
	//}}AFX_VIRTUAL

// Implementation
public:
	bool m_over;
	HCURSOR m_cursor;
	afx_msg void OnClicked();
	CFont m_underlineFont;
	virtual ~Hyperlink();

	// Generated message map functions
protected:
	//{{AFX_MSG(Hyperlink)
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_HYPERLINK_H__BECEAB7D_31FB_4727_A42B_8732162EEBCC__INCLUDED_)
