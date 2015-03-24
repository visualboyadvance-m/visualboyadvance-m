#pragma once

/////////////////////////////////////////////////////////////////////////////
// LinkOptions dialog

class LinkOptions : public CDialog
{
// Construction
public:
	LinkOptions(CWnd* pParent = NULL);   // standard constructor
// Dialog Data
	//{{AFX_DATA(LinkOptions)
	enum { IDD = IDD_LINKTAB };
	int m_type;
	CEdit m_timeout;
	CComboBox  m_mode;
	CEdit m_serverip;
	BOOL m_server;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(LinkOptions)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	void AddMode(LPCTSTR name, int value);
	void UpdateAvailability();

	// Generated message map functions
	//{{AFX_MSG(LinkOptions)
	afx_msg void OnCbnSelchangeLinkMode();
	virtual BOOL OnInitDialog();
	afx_msg void OnOk();
	afx_msg void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedLinkServer();
	afx_msg void OnBnClickedLinkClient();
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
	//}}AFX_DATA

	bool m_userAborted;

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

