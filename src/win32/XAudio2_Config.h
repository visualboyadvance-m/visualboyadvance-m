// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team
// Copyright (C) 2007-2008 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


#ifndef NO_XAUDIO2

#pragma once
#include "afxwin.h"
#include "afxcmn.h"


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
