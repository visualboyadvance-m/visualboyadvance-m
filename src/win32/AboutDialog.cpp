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

// AboutDialog.cpp : implementation file
//

#include "stdafx.h"
#include "AboutDialog.h"
#include "../AutoBuild.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// AboutDialog dialog


AboutDialog::AboutDialog(CWnd* pParent /*=NULL*/)
  : CDialog(AboutDialog::IDD, pParent)
{
  //{{AFX_DATA_INIT(AboutDialog)
  m_version = _T(VERSION);
  //}}AFX_DATA_INIT
}


void AboutDialog::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(AboutDialog)
  DDX_Text(pDX, IDC_VERSION, m_version);
  DDX_Control(pDX, IDC_URL, m_link);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(AboutDialog, CDialog)
  //{{AFX_MSG_MAP(AboutDialog)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// AboutDialog message handlers

BOOL AboutDialog::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  CWnd *p = GetDlgItem(IDC_TRANSLATOR_URL);
  if(p) {
    m_translator.SubclassDlgItem(IDC_TRANSLATOR_URL, this);
  }

  m_link.SetWindowText("http://vba.ngemu.com");

  return TRUE;  // return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE
}
