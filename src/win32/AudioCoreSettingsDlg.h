#pragma once

#include "stdafx.h"


// AudioCoreSettingsDlg dialog

class AudioCoreSettingsDlg : public CDialog
{
	DECLARE_DYNAMIC(AudioCoreSettingsDlg)

public:
	bool m_enabled;
	bool m_surround;
	float m_echo;
	float m_stereo;
	float m_volume;

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
	CSliderCtrl echo;
	CSliderCtrl stereo;
	CSliderCtrl volume;
	CToolTipCtrl *toolTip;
};
