#ifndef NO_XAUDIO2

#pragma once


class XAudio2_Config : public CDialog
{
	DECLARE_DYNAMIC(XAudio2_Config)
	DECLARE_MESSAGE_MAP()

public:
	UINT32 m_selected_device_index;
	UINT32 m_buffer_count;
	bool m_enable_upmixing;

private:
	CComboBox m_combo_dev;
	CSliderCtrl m_slider_buffer;
	CStatic m_info_buffer;
	CButton m_check_upmix;


public:
	XAudio2_Config(CWnd* pParent = NULL);   // standard constructor
	virtual ~XAudio2_Config();

// Dialog Data
	enum { IDD = IDD_XAUDIO2_CONFIG };

	virtual BOOL OnInitDialog();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
};

#endif
