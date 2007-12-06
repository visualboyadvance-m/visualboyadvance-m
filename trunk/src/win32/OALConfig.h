#ifndef NO_OAL

#pragma once
#include "afxwin.h"


// OALConfig dialog

class OALConfig : public CDialog
{
	DECLARE_DYNAMIC(OALConfig)

public:
	OALConfig(CWnd* pParent = NULL);   // standard constructor
	virtual ~OALConfig();

// Dialog Data
	enum { IDD = IDD_OAL_CONFIG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
private:
	CComboBox cbDevice;
public:
	CString selectedDevice;
};

#endif
