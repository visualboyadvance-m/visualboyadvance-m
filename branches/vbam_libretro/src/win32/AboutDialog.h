#pragma once

#include "stdafx.h"
#include "Hyperlink.h"
#include "resource.h"


class AboutDialog : public CDialog
{
public:
	AboutDialog(CWnd* pParent = NULL);
	enum { IDD = IDD_ABOUT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()

private:
	Hyperlink m_link;
	Hyperlink m_translator;
	CString   m_version;
	CString   m_date;
};
