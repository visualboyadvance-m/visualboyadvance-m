#include "stdafx.h"
#include "VBA.h"

#include "AudioCoreSettingsDlg.h"

#define MIN_VOLUME 0.0f
#define MAX_VOLUME 4.0f


// AudioCoreSettingsDlg dialog

IMPLEMENT_DYNAMIC(AudioCoreSettingsDlg, CDialog)

AudioCoreSettingsDlg::AudioCoreSettingsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(AudioCoreSettingsDlg::IDD, pParent)
	, m_enabled( false )
	, m_surround( false )
	, m_declicking( false )
	, m_sound_interpolation( false )
	, m_echo( 0.0f )
	, m_stereo( 0.0f )
	, m_volume( 0.0f )
	, m_sound_filtering( 0.0f )
	, m_sample_rate( 0 )
	, toolTip( NULL )
{
}

AudioCoreSettingsDlg::~AudioCoreSettingsDlg()
{
	delete toolTip;
}

void AudioCoreSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_ENHANCE_SOUND, enhance_sound);
	DDX_Control(pDX, IDC_SURROUND, surround);
	DDX_Control(pDX, IDC_ECHO, echo);
	DDX_Control(pDX, IDC_STEREO, stereo);
	DDX_Control(pDX, IDC_VOLUME, volume);
	DDX_Control(pDX, IDC_DECLICKING, declicking);
	DDX_Control(pDX, IDC_SOUND_INTERPOLATION, sound_interpolation);
	DDX_Control(pDX, IDC_SOUND_FILTERING, sound_filtering);
	DDX_Control(pDX, IDC_SAMPLE_RATE, sample_rate);

	if( pDX->m_bSaveAndValidate == TRUE ) {
		m_enabled = BST_CHECKED == enhance_sound.GetCheck();
		m_surround = BST_CHECKED == surround.GetCheck();
		m_declicking = BST_CHECKED == declicking.GetCheck();
		m_sound_interpolation = BST_CHECKED == sound_interpolation.GetCheck();
		m_echo = (float)echo.GetPos() / 100.0f;
		m_stereo = (float)stereo.GetPos() / 100.0f;
		m_volume = (float)volume.GetPos() / 100.0f;
		m_sound_filtering = (float)sound_filtering.GetPos() / 100.0f;
		m_sample_rate = (unsigned int)sample_rate.GetItemData( sample_rate.GetCurSel() );
	}
}

BOOL AudioCoreSettingsDlg::OnTtnNeedText(UINT id, NMHDR *pNMHDR, LRESULT *pResult)
{
	TOOLTIPTEXT *t3 = (TOOLTIPTEXT *)pNMHDR; // dirty Windows API
	BOOL i_provided_tooltip_with_text = TRUE;

	if( !( t3->uFlags & TTF_IDISHWND ) ) {
		return FALSE;
	}
	// even dirtier Windows API:
	// t3->hdr.idFrom is actually a HWND, holy cow, why?
	// The other case does not even occur.
	int controlID = ::GetDlgCtrlID( (HWND)t3->hdr.idFrom );
	CString res;
	TCHAR buf[0x400]; // Use own string buffer because szText has an 80 char limit.
	// We can't use a dynamic buffer size because Windows does some shady things with
	//  t3->lpszText at the end of this function, so we have no chance to free the buffer
	//  before the end of this function.

	switch( controlID ) {
		case IDC_VOLUME:
			_stprintf_s( t3->szText, _countof( t3->szText ), _T( "%i%%" ), volume.GetPos() );
			break;
		case IDC_ECHO:
			_stprintf_s( t3->szText, _countof( t3->szText ), _T( "%i%%" ), echo.GetPos() );
			break;
		case IDC_STEREO:
			_stprintf_s( t3->szText, _countof( t3->szText ), _T( "%i%%" ), stereo.GetPos() );
			break;
		case IDC_SOUND_FILTERING:
			_stprintf_s( t3->szText, _countof( t3->szText ), _T( "%i%%" ), sound_filtering.GetPos() );
			break;
		case IDC_DEFAULT_VOLUME:
			res.LoadString( IDS_TOOLTIP_DEFAULT_VOLUME );
			_tcscpy_s( buf, _countof( buf ), res.GetString() );
			t3->lpszText = buf;
			break;
		case IDC_ENHANCE_SOUND:
			res.LoadString( IDS_TOOLTIP_ENHANCE_SOUND );
			_tcscpy_s( buf, _countof( buf ), res.GetString() );
			t3->lpszText = buf;
			break;
		case IDC_SURROUND:
			res.LoadString( IDS_TOOLTIP_SURROUND );
			_tcscpy_s( buf, _countof( buf ), res.GetString() );
			t3->lpszText = buf;
			break;
		case IDC_DECLICKING:
			res.LoadString( IDS_TOOLTIP_DECLICKING );
			_tcscpy_s( buf, _countof( buf ), res.GetString() );
			t3->lpszText = buf;
			break;
		default:
			i_provided_tooltip_with_text = FALSE;
			break;
	}

	return i_provided_tooltip_with_text;
}


BEGIN_MESSAGE_MAP(AudioCoreSettingsDlg, CDialog)
	ON_NOTIFY_EX(TTN_NEEDTEXT, 0, &AudioCoreSettingsDlg::OnTtnNeedText)
	ON_BN_CLICKED(IDC_DEFAULT_VOLUME, &AudioCoreSettingsDlg::OnBnClickedDefaultVolume)
END_MESSAGE_MAP()


// AudioCoreSettingsDlg message handlers

BOOL AudioCoreSettingsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set up tooltip control
	toolTip = new CToolTipCtrl;
	toolTip->Create( this );
	toolTip->AddTool( GetDlgItem( IDC_DEFAULT_VOLUME ) );
	toolTip->AddTool( GetDlgItem( IDC_ENHANCE_SOUND ) );
	toolTip->AddTool( GetDlgItem( IDC_SURROUND ) );
	toolTip->AddTool( GetDlgItem( IDC_DECLICKING ) );
	toolTip->Activate( TRUE );

	enhance_sound.SetCheck( m_enabled ? BST_CHECKED : BST_UNCHECKED );

	surround.SetCheck( m_surround ? BST_CHECKED : BST_UNCHECKED );

	declicking.SetCheck( m_declicking ? BST_CHECKED : BST_UNCHECKED );

	sound_interpolation.SetCheck( m_sound_interpolation ? BST_CHECKED : BST_UNCHECKED );

	echo.SetRange( 0, 100 );
	echo.SetPos( (int)( m_echo * 100.0f ) );

	stereo.SetRange( 0, 100 );
	stereo.SetPos( (int)( m_stereo * 100.0f ) );

	sound_filtering.SetRange( 0, 100 );
	sound_filtering.SetPos( (int)( m_sound_filtering * 100.0f ) );

	volume.SetRange( (int)( MIN_VOLUME * 100.0f ), (int)( MAX_VOLUME * 100.0f ) );
	volume.SetPos( (int)( m_volume * 100.0f ) );

	unsigned int rate = 44100;
	CString temp;
	for( int i = 0 ; i <= 2 ; i++ ) {
		temp.Format( _T("%u Hz"), rate );
		int id = sample_rate.AddString( temp.GetString() );
		sample_rate.SetItemData( id, rate );
		if( rate == m_sample_rate ) {
			sample_rate.SetCurSel( id );
		}
		rate /= 2;
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

BOOL AudioCoreSettingsDlg::PreTranslateMessage(MSG* pMsg)
{
	// Required for enabling ToolTips in a modal dialog box.
	if( NULL != toolTip ) {
		toolTip->RelayEvent( pMsg );
	}

	return CDialog::PreTranslateMessage(pMsg);
}

void AudioCoreSettingsDlg::OnBnClickedDefaultVolume()
{
	volume.SetPos( 100 );
}
