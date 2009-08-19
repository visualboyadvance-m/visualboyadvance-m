#include "stdafx.h"
#ifndef NO_OAL
#include "VBA.h"

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
	, bufferCount(0)
{
	if( !LoadOAL10Library( NULL, &ALFunction ) ) {
		systemMessage( IDS_OAL_NODLL, "OpenAL32.dll could not be found on your system. Please install the runtime from http://openal.org" );
	}
}

OALConfig::~OALConfig()
{
}

void OALConfig::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DEVICE, m_cbDevice);

	if( !pDX->m_bSaveAndValidate ) {
		// enumerate devices
		const ALchar *devices = NULL;
		devices = ALFunction.alcGetString( NULL, ALC_DEVICE_SPECIFIER );
		if( strlen( devices ) ) {
			while( *devices ) {
				m_cbDevice.AddString( devices );
				devices += strlen( devices ) + 1;
			}
		} else {
			systemMessage( IDS_OAL_NODEVICE, "There are no sound devices present on this system." );
		}
	}
	DDX_CBString(pDX, IDC_DEVICE, selectedDevice);
	DDX_Control(pDX, IDC_SLIDER_BUFFERCOUNT, m_sliderBufferCount);
	DDX_Slider(pDX, IDC_SLIDER_BUFFERCOUNT, bufferCount);
	DDX_Control(pDX, IDC_BUFFERINFO, m_bufferInfo);
}


BOOL OALConfig::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_sliderBufferCount.SetRange( 2, 10, FALSE );
	m_sliderBufferCount.SetTicFreq( 1 );
	m_sliderBufferCount.SetPos( bufferCount );

	CString info;
	int pos = m_sliderBufferCount.GetPos();
	info.Format( _T("%i frames = %.2f ms"), pos, (float)pos / 60.0f * 1000.0f );
	m_bufferInfo.SetWindowText( info );

	return TRUE;
}


BEGIN_MESSAGE_MAP(OALConfig, CDialog)
	ON_WM_HSCROLL()
END_MESSAGE_MAP()


// OALConfig message handlers

void OALConfig::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CString info;
	int pos = m_sliderBufferCount.GetPos();
	info.Format( _T("%i frames = %.2f ms"), pos, (float)pos / 60.0f * 1000.0f );
	m_bufferInfo.SetWindowText( info );

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

#endif
