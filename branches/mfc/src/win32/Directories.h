// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2008 VBA-M development team

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


#pragma once


class Directories : public CDialog
{
public:
	Directories(CWnd* pParent = NULL);
	enum { IDD = IDD_DIRECTORIES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnBatteryDir();
	afx_msg void OnCaptureDir();
	afx_msg void OnGbromDir();
	afx_msg void OnGbcromDir();
	afx_msg void OnRomDir();
	afx_msg void OnSaveDir();
	virtual void OnCancel();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()

private:
	CEdit  m_savePath;
	CEdit  m_romPath;
	CEdit  m_gbcromPath;
	CEdit  m_gbromPath;
	CEdit  m_capturePath;
	CEdit  m_batteryPath;
	CString initialFolderDir;

	CString browseForDir(CString title);
	bool directoryDoesExist(const char *directory);
};
