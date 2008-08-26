#pragma once


// AudioEffectsDlg dialog

class AudioEffectsDlg : public CDialog
{
	DECLARE_DYNAMIC(AudioEffectsDlg)

public:
	bool m_enabled;
	bool m_surround;
	float m_echo;
	float m_stereo;

	AudioEffectsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~AudioEffectsDlg();

// Dialog Data
	enum { IDD = IDD_AUDIO_EFFECTS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
};
