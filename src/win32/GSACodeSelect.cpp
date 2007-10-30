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

// GSACodeSelect.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "GSACodeSelect.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// GSACodeSelect dialog


GSACodeSelect::GSACodeSelect(FILE *file, CWnd* pParent /*=NULL*/)
  : CDialog(GSACodeSelect::IDD, pParent)
{
  //{{AFX_DATA_INIT(GSACodeSelect)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  m_file = file;
}


void GSACodeSelect::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GSACodeSelect)
  DDX_Control(pDX, IDC_GAME_LIST, m_games);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GSACodeSelect, CDialog)
  //{{AFX_MSG_MAP(GSACodeSelect)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_LBN_SELCHANGE(IDC_GAME_LIST, OnSelchangeGameList)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GSACodeSelect message handlers

void GSACodeSelect::OnCancel() 
{
  EndDialog(-1);
}

void GSACodeSelect::OnOk() 
{
  EndDialog(m_games.GetCurSel());
}

void GSACodeSelect::OnSelchangeGameList() 
{
  int item = m_games.GetCurSel();
  CWnd *ok = GetDlgItem(ID_OK);

  ok->EnableWindow(item != -1);
}

BOOL GSACodeSelect::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  char buffer[1024];
  
  FILE *f = m_file;
  int games = 0;
  int len = 0;
  fseek(f, -4, SEEK_CUR);
  fread(&games, 1, 4, f);
  while(games > 0) {
    fread(&len, 1, 4, f);
    fread(buffer, 1, len, f);
    buffer[len] = 0;
    m_games.AddString(buffer);
    int codes = 0;
    fread(&codes, 1, 4, f);
    
    while(codes > 0) {
      fread(&len, 1, 4, f);
      fseek(f, len, SEEK_CUR);
      fread(&len, 1, 4, f);
      fseek(f, len, SEEK_CUR);
      fseek(f, 4, SEEK_CUR);
      fread(&len, 1, 4, f);
      fseek(f, len*12, SEEK_CUR);
      codes--;
    }
    games--;
  }
  GetDlgItem(ID_OK)->EnableWindow(FALSE);
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}
