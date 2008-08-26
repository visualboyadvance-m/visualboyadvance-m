// ..\..\src\win32\AudioEffectsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "AudioEffectsDlg.h"

// Has to be in int range!
#define SLIDER_RESOLUTION ( 100 )
// We do everything in percents.


// AudioEffectsDlg dialog

IMPLEMENT_DYNAMIC(AudioEffectsDlg, CDialog)

AudioEffectsDlg::AudioEffectsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(AudioEffectsDlg::IDD, pParent)
	, m_enabled( false )
	, m_surround( false )
	, m_echo( 0.0f )
	, m_stereo( 0.0f )
{
}

AudioEffectsDlg::~AudioEffectsDlg()
{
}

void AudioEffectsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	CButton *enhance_sound = (CButton *)GetDlgItem( IDC_ENHANCE_SOUND );
	CButton *surround = (CButton * )GetDlgItem( IDC_SURROUND );
	CSliderCtrl *echo = (CSliderCtrl *)GetDlgItem( IDC_ECHO );
	CSliderCtrl *stereo = (CSliderCtrl *)GetDlgItem( IDC_STEREO );

	if( pDX->m_bSaveAndValidate == TRUE ) {
		m_enabled = BST_CHECKED == enhance_sound->GetCheck();
		m_surround = BST_CHECKED == surround->GetCheck();
		m_echo = (float)echo->GetPos() / (float)SLIDER_RESOLUTION;
		m_stereo = (float)stereo->GetPos() / (float)SLIDER_RESOLUTION;
	} else {
		enhance_sound->SetCheck( m_enabled ? BST_CHECKED : BST_UNCHECKED );
		surround->SetCheck( m_surround ? BST_CHECKED : BST_UNCHECKED );
		echo->SetPos( (int)( m_echo * (float)SLIDER_RESOLUTION ) );
		stereo->SetPos( (int)( m_stereo * (float)SLIDER_RESOLUTION ) );
	}
}


BEGIN_MESSAGE_MAP(AudioEffectsDlg, CDialog)
END_MESSAGE_MAP()


// AudioEffectsDlg message handlers

BOOL AudioEffectsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CSliderCtrl *echo = (CSliderCtrl *)GetDlgItem( IDC_ECHO );
	echo->SetRange( 0, SLIDER_RESOLUTION );

	CSliderCtrl *stereo = (CSliderCtrl *)GetDlgItem( IDC_STEREO );
	stereo->SetRange( 0, SLIDER_RESOLUTION );

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
