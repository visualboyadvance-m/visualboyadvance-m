// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team

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

#include "afxwin.h"
#if !defined(AFX_GAMEOVERRIDES_H__EEEFE37F_F477_455D_8682_705FB2DBCC0C__INCLUDED_)
#define AFX_GAMEOVERRIDES_H__EEEFE37F_F477_455D_8682_705FB2DBCC0C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GameOverrides.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// GameOverrides dialog

class GameOverrides : public CDialog
{
// Construction
public:
	GameOverrides(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(GameOverrides)
	enum { IDD = IDD_GAME_OVERRIDES };
	CEdit	m_name;
	CComboBox	m_mirroring;
	CComboBox	m_flashSize;
	CComboBox	m_saveType;
	CComboBox	m_rtc;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(GameOverrides)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(GameOverrides)
	virtual void OnOK();
	afx_msg void OnDefaults();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CEdit m_comment;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GAMEOVERRIDES_H__EEEFE37F_F477_455D_8682_705FB2DBCC0C__INCLUDED_)
