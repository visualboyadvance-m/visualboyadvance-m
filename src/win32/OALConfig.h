#ifndef NO_OAL

#pragma once
#include "afxwin.h"

// OpenAL
#include <al.h>
#include <alc.h>
#include "LoadOAL.h"
#include "afxcmn.h"


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
	OPENALFNTABLE ALFunction;
	CComboBox m_cbDevice;
	CSliderCtrl m_sliderBufferCount;
	CStatic m_bufferInfo;
public:
	CString selectedDevice;
	int bufferCount;
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};

#endif
