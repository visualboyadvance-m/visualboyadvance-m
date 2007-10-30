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

// Throttle.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "Throttle.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Throttle dialog


Throttle::Throttle(CWnd* pParent /*=NULL*/)
  : CDialog(Throttle::IDD, pParent)
{
  //{{AFX_DATA_INIT(Throttle)
  m_throttle = 0;
  //}}AFX_DATA_INIT
}


void Throttle::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(Throttle)
  DDX_Text(pDX, IDC_THROTTLE, m_throttle);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Throttle, CDialog)
  //{{AFX_MSG_MAP(Throttle)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// Throttle message handlers

BOOL Throttle::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void Throttle::OnCancel() 
{
  EndDialog(false);
}

void Throttle::OnOk() 
{
  UpdateData();

  if(m_throttle < 5 || m_throttle > 1000)
    systemMessage(IDS_INVALID_THROTTLE_VALUE, "Invalid throttle value. Please enter a number between 5 and 1000");
  else
    EndDialog(m_throttle);
}
