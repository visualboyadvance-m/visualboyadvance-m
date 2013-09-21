#pragma once

// AudioCoreSettingsDlg dialog

class AudioCoreSettingsDlg : public CDialog
{
	DECLARE_DYNAMIC(AudioCoreSettingsDlg)

public:
	bool m_enabled;
	bool m_surround;
	bool m_declicking;
	bool m_sound_interpolation;
	float m_echo;
	float m_stereo;
	float m_volume;
	float m_sound_filtering;
	unsigned int m_sample_rate;

	AudioCoreSettingsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~AudioCoreSettingsDlg();

	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnBnClickedDefaultVolume();

// Dialog Data
	enum { IDD = IDD_AUDIO_CORE_SETTINGS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg BOOL OnTtnNeedText(UINT id, NMHDR *pNMHDR, LRESULT *pResult); // Retrieve text for ToolTip

	DECLARE_MESSAGE_MAP()

private:
	CButton enhance_sound;
	CButton surround;
	CButton declicking;
	CButton sound_interpolation;
	CSliderCtrl echo;
	CSliderCtrl stereo;
	CSliderCtrl volume;
	CSliderCtrl sound_filtering;
	CToolTipCtrl *toolTip;
	CComboBox sample_rate;
};
