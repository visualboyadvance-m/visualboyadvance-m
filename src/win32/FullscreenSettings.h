#pragma once

#include "Display.h"


#ifndef NO_D3D
#pragma comment( lib, "d3d9" )
#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif
#define DIRECT3D_VERSION 0x0900
#include <d3d9.h>
#endif


// FullscreenSettings dialog

class FullscreenSettings : public CDialog
{
	DECLARE_DYNAMIC(FullscreenSettings)

public:
	FullscreenSettings(CWnd* pParent = NULL);   // standard constructor
	virtual ~FullscreenSettings();

// Dialog Data
	enum { IDD = IDD_FULLSCREEN };

	// Call this function to select the API for the display mode enumeration.
	void setAPI( DISPLAY_TYPE type );
	afx_msg void OnCbnSelchangeComboDevice();
	afx_msg void OnCbnSelchangeComboColorDepth();
	afx_msg void OnCbnSelchangeComboResolution();

	unsigned int m_device;
	unsigned int m_colorDepth;
	unsigned int m_width;
	unsigned int m_height;
	unsigned int m_refreshRate;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()

private:
	DISPLAY_TYPE api;
	bool failed;

#ifndef NO_D3D
	LPDIRECT3D9 pD3D;
#endif

	virtual BOOL OnInitDialog();

	CComboBox combo_device;
	CComboBox combo_resolution;
	CComboBox combo_color_depth;
	CComboBox combo_refresh_rate;
};
