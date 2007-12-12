// src/win32/OALConfig.cpp : implementation file
//

#ifndef NO_OAL

#include "stdafx.h"
#include "OALConfig.h"

// OpenAL
#include <al.h>
#include <alc.h>
#include "LoadOAL.h"


// OALConfig dialog

IMPLEMENT_DYNAMIC(OALConfig, CDialog)

OALConfig::OALConfig(CWnd* pParent /*=NULL*/)
	: CDialog(OALConfig::IDD, pParent)
	, selectedDevice(_T(""))
{

}

OALConfig::~OALConfig()
{
}

void OALConfig::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DEVICE, cbDevice);

	if( !pDX->m_bSaveAndValidate ) {
		// enumerate devices
		const ALchar *devices = NULL;
		devices = ALFunction.alcGetString( NULL, ALC_DEVICE_SPECIFIER );
		if( strlen( devices ) ) {
			while( *devices ) {
				cbDevice.AddString( devices );
				devices += strlen( devices ) + 1;
			}
		} else {
			systemMessage( IDS_OAL_NODEVICE, "There are no sound devices present on this system." );
		}
	}
	DDX_CBString(pDX, IDC_DEVICE, selectedDevice);

}


BOOL OALConfig::OnInitDialog()
{
	CDialog::OnInitDialog();

	if( !LoadOAL10Library( NULL, &ALFunction ) ) {
		systemMessage( IDS_OAL_NODLL, "OpenAL32.dll could not be found on your system. Please install the runtime from http://openal.org" );
		return false;
	}

	return TRUE;
}


BEGIN_MESSAGE_MAP(OALConfig, CDialog)
END_MESSAGE_MAP()


// OALConfig message handlers

#endif
