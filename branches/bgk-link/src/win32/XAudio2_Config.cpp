#include "stdafx.h"
#ifndef NO_XAUDIO2
#include "VBA.h"

#include "XAudio2_Config.h"

#include <xaudio2.h>


// XAudio2_Config dialog

IMPLEMENT_DYNAMIC(XAudio2_Config, CDialog)

XAudio2_Config::XAudio2_Config(CWnd* pParent /*=NULL*/)
	: CDialog(XAudio2_Config::IDD, pParent)
	, m_selected_device_index(0)
	, m_enable_upmixing(false)
	, m_buffer_count(0)
{
}

XAudio2_Config::~XAudio2_Config()
{
}

void XAudio2_Config::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_DEV, m_combo_dev);
	DDX_Control(pDX, IDC_SLIDER_BUFFER, m_slider_buffer);
	DDX_Control(pDX, IDC_INFO_BUFFER, m_info_buffer);
	DDX_Control(pDX, IDC_CHECK_UPMIX, m_check_upmix);

	if( pDX->m_bSaveAndValidate == TRUE ) {
		if( CB_ERR != m_combo_dev.GetCurSel() ) {
			if( CB_ERR != m_combo_dev.GetItemData( m_combo_dev.GetCurSel() ) ) {
				m_selected_device_index = m_combo_dev.GetItemData( m_combo_dev.GetCurSel() );
			}
		}

		m_enable_upmixing = ( m_check_upmix.GetCheck() == BST_CHECKED );

		m_buffer_count = (UINT32)m_slider_buffer.GetPos();
	} else {
		m_check_upmix.SetCheck( m_enable_upmixing ? BST_CHECKED : BST_UNCHECKED );
	}
}


BEGIN_MESSAGE_MAP(XAudio2_Config, CDialog)
	ON_WM_HSCROLL()
END_MESSAGE_MAP()


// XAudio2_Config message handlers

BOOL XAudio2_Config::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_combo_dev.ResetContent();

	m_slider_buffer.SetRange( 2, 10, FALSE );
	m_slider_buffer.SetTicFreq( 1 );
	m_slider_buffer.SetPos( (int)m_buffer_count );

	CString info;
	int pos = m_slider_buffer.GetPos();
	info.Format( _T("%i frames = %.2f ms"), pos, (float)pos / 60.0f * 1000.0f );
	m_info_buffer.SetWindowText( info );

	HRESULT hr;
	IXAudio2 *xa = NULL;
	UINT32 flags = 0;
#ifdef _DEBUG
	flags = XAUDIO2_DEBUG_ENGINE;
#endif

	hr = XAudio2Create( &xa, flags );
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_FAILURE, NULL );
	} else {
		UINT32 dev_count = 0;
		hr = xa->GetDeviceCount( &dev_count );
		if( hr != S_OK ) {
			systemMessage( IDS_XAUDIO2_CANNOT_ENUMERATE_DEVICES, NULL );
		} else {
			XAUDIO2_DEVICE_DETAILS dd;
			for( UINT32 i = 0; i < dev_count; i++ ) {
				hr = xa->GetDeviceDetails( i, &dd );
				if( hr != S_OK ) {
					continue;
				} else {
#ifdef _UNICODE
					int id = m_combo_dev.AddString( dd.DisplayName );
#else
					CHAR temp[256];
					ZeroMemory( temp, sizeof( temp ) );
					WideCharToMultiByte(
						CP_ACP,
						WC_NO_BEST_FIT_CHARS,
						dd.DisplayName,
						-1,
						temp,
						sizeof( temp ) - 1,
						NULL,
						NULL );
					
					int id = m_combo_dev.AddString( temp );
#endif
					if( id < 0 ) {
						systemMessage( IDS_XAUDIO2_CANNOT_ENUMERATE_DEVICES, NULL );
						break;
					} else {
						m_combo_dev.SetItemData( id, i );
					}
				}
			}

			// select the currently configured device {
			int count = m_combo_dev.GetCount();
			if( count > 0 ) {
				for( int i = 0; i < count; i++ ) {
					if( m_combo_dev.GetItemData( i ) == m_selected_device_index ) {
						m_combo_dev.SetCurSel( i );
						break;
					}
				}
			}
			// }

		}
		xa->Release();
		xa = NULL;
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void XAudio2_Config::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CString info;
	int pos = m_slider_buffer.GetPos();
	info.Format( _T("%i frames = %.2f ms"), pos, (float)pos / 60.0f * 1000.0f );
	m_info_buffer.SetWindowText( info );

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

#endif
