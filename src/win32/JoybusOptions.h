#pragma once
#include "afxwin.h"

// JoybusOptions dialog

class JoybusOptions : public CDialog
{
	DECLARE_DYNAMIC(JoybusOptions)

public:
	JoybusOptions(CWnd* pParent = NULL);   // standard constructor
	virtual ~JoybusOptions();

// Dialog Data
	enum { IDD = IDD_JOYBUS_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedJoybusEnable();
	CButton enable_check;
	CEdit hostname;
	afx_msg void OnBnClickedOk();
};
