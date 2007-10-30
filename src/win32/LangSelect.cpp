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

// LangSelect.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "LangSelect.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// LangSelect dialog


LangSelect::LangSelect(CWnd* pParent /*=NULL*/)
  : CDialog(LangSelect::IDD, pParent)
{
  //{{AFX_DATA_INIT(LangSelect)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}


void LangSelect::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(LangSelect)
  DDX_Control(pDX, IDC_LANG_STRING, m_langString);
  DDX_Control(pDX, IDC_LANG_NAME, m_langName);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(LangSelect, CDialog)
  //{{AFX_MSG_MAP(LangSelect)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// LangSelect message handlers

void LangSelect::OnCancel() 
{
  EndDialog(FALSE);
}

void LangSelect::OnOk() 
{
  m_langString.GetWindowText(theApp.languageName);
  EndDialog(TRUE);
}

BOOL LangSelect::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  char lbuffer[10];
  if(GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SABBREVLANGNAME,
                   lbuffer, 10)) {
    m_langName.SetWindowText(lbuffer);
  } else {
    m_langName.SetWindowText("???");
  }
  
  if(!theApp.languageName.IsEmpty())
    m_langString.SetWindowText(theApp.languageName);
      
  m_langString.LimitText(3);
  
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}
