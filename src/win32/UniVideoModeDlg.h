#pragma once

#include "stdafx.h"
#include <d3d9.h>
#include "VBA.h"
#include "MaxScale.h"

// UniVideoModeDlg-Dialogfeld

class UniVideoModeDlg : public CDialog
{
	DECLARE_DYNAMIC(UniVideoModeDlg)

public:
	UniVideoModeDlg(CWnd* pParent = NULL, int *width=0, int *height=0, int *BPP=0, int *freq=0, int *adapt=0);   // Standardkonstruktor
	virtual ~UniVideoModeDlg();

// Dialogfelddaten
	enum { IDD = IDD_UNIVIDMODE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV-Unterstützung

	DECLARE_MESSAGE_MAP()
public:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnBnClickedOk();
	BOOL OnInitDialog();
	afx_msg void OnBnClickedCancel();

private:
	DWORD       *WidthList, *HeightList, *BPPList, *FreqList;
	DWORD       iDisplayDevice;
	HINSTANCE   d3dDLL;
	LPDIRECT3D9 pD3D;
	UINT        nAdapters;
public:
	int
		*SelectedWidth,
		*SelectedHeight,
		*SelectedBPP,
		*SelectedFreq,
		*SelectedAdapter;
	afx_msg void OnStnClickedDisplaydevice();
	afx_msg void OnBnClickedButtonMaxscale();
	afx_msg void OnBnClickedCheckStretchtofit();
};
