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

// ModeConfirm.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "ModeConfirm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// ModeConfirm dialog


ModeConfirm::ModeConfirm(CWnd* pParent /*=NULL*/)
  : CDialog(ModeConfirm::IDD, pParent)
{
  //{{AFX_DATA_INIT(ModeConfirm)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}


void ModeConfirm::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(ModeConfirm)
  // NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(ModeConfirm, CDialog)
  //{{AFX_MSG_MAP(ModeConfirm)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_WM_DESTROY()
  ON_WM_TIMER()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// ModeConfirm message handlers

void ModeConfirm::OnCancel() 
{
  EndDialog(FALSE);
}

void ModeConfirm::OnOk() 
{
  EndDialog(TRUE);
}

void ModeConfirm::OnDestroy() 
{
  CDialog::OnDestroy();
  
  KillTimer(timer);
  timer = 0;
}

BOOL ModeConfirm::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  timer = SetTimer(0, 1000, NULL);

  count = 10;

  CString buffer;
  buffer.Format("%d", count);

  GetDlgItem(IDC_TIMER)->SetWindowText(buffer);

  CenterWindow(theApp.m_pMainWnd);
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void ModeConfirm::OnTimer(UINT nIDEvent) 
{
  CString buffer;  
  count--;
  if(count == 0)
    EndDialog(FALSE);
  buffer.Format("%d", count);
  GetDlgItem(IDC_TIMER)->SetWindowText(buffer);
  
  CDialog::OnTimer(nIDEvent);
}
