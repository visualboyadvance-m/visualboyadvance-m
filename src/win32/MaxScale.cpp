// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

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

// MaxScale.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "MaxScale.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// MaxScale dialog


MaxScale::MaxScale(CWnd* pParent /*=NULL*/)
	: CDialog(MaxScale::IDD, pParent)
{
	//{{AFX_DATA_INIT(MaxScale)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void MaxScale::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(MaxScale)
	DDX_Control(pDX, IDC_VALUE, m_value);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(MaxScale, CDialog)
	//{{AFX_MSG_MAP(MaxScale)
	ON_BN_CLICKED(ID_OK, OnOk)
	ON_BN_CLICKED(ID_CANCEL, OnCancel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// MaxScale message handlers

void MaxScale::OnCancel() 
{
  EndDialog(FALSE);
}

void MaxScale::OnOk() 
{
  CString tmp;
  m_value.GetWindowText(tmp);
  theApp.fsMaxScale = atoi(tmp);
  EndDialog(TRUE);
}

BOOL MaxScale::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	CString temp;

  temp.Format("%d", theApp.fsMaxScale);

  m_value.SetWindowText(temp);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
