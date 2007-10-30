#if !defined(AFX_LINKOPTIONS_H__55E28DAC_10CC_4AB4_9FAC_B036AA8017D3__INCLUDED_)
#define AFX_LINKOPTIONS_H__55E28DAC_10CC_4AB4_9FAC_B036AA8017D3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LinkOptions.h : header file
//

class CMyTabCtrl : public CTabCtrl
{
public:
	CMyTabCtrl(void);
	~CMyTabCtrl(void);
	CDialog *m_tabdialog[3];
	void OnSwitchTabs(void);
};

/////////////////////////////////////////////////////////////////////////////
// LinkGeneral dialog

class LinkGeneral : public CDialog
{
// Construction
public:
	LinkGeneral(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(LinkGeneral)
	enum { IDD = IDD_LINKTAB1 };
	int m_type;
	CEdit m_timeout;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(LinkGeneral)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(LinkGeneral)
	virtual BOOL OnInitDialog();
	afx_msg void OnRadio1();
	afx_msg void OnRadio2();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// LinkOptions dialog

class LinkOptions : public CDialog
{
// Construction
public:
	LinkOptions(CWnd* pParent = NULL);   // standard constructor
	void GetAllData(LinkGeneral*);
// Dialog Data
	//{{AFX_DATA(LinkOptions)
	enum { IDD = IDD_LINKTAB };
	CMyTabCtrl	m_tabctrl;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(LinkOptions)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(LinkOptions)
	afx_msg void OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnOk();
	afx_msg void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// LinkServer dialog

class LinkServer : public CDialog
{
// Construction
public:
	LinkServer(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(LinkServer)
	enum { IDD = IDD_LINKTAB2 };
	int		m_numplayers;
	int		m_prottype;
	BOOL	m_speed;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(LinkServer)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(LinkServer)
		virtual BOOL OnInitDialog();
		afx_msg void OnServerStart();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class LinkClient : public CDialog
{
// Construction
public:
	LinkClient(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(LinkClient)
	enum { IDD = IDD_LINKTAB3 };
	CEdit m_serverip;
	int		m_prottype;
	int		m_hacks;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(LinkClient)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(LinkClient)
		virtual BOOL OnInitDialog();
		afx_msg void OnLinkConnect();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// ServerWait dialog

class ServerWait : public CDialog
{
// Construction
public:
	ServerWait(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(ServerWait)
	enum { IDD = IDD_SERVERWAIT };
	CProgressCtrl m_prgctrl;
	CString	m_serveraddress;
	CString	m_plconn[3];
	//CString	m_p2conn;
	//CString	m_p3conn;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(ServerWait)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg void OnCancel();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(ServerWait)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LINKOPTIONS_H__55E28DAC_10CC_4AB4_9FAC_B036AA8017D3__INCLUDED_)
