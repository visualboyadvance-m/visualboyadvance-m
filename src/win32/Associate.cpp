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

// Associate.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "Associate.h"
#include "Reg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Associate dialog


Associate::Associate(CWnd* pParent /*=NULL*/)
  : CDialog(Associate::IDD, pParent)
{
  //{{AFX_DATA_INIT(Associate)
  m_agb = FALSE;
  m_bin = FALSE;
  m_cgb = FALSE;
  m_gb = FALSE;
  m_gba = FALSE;
  m_gbc = FALSE;
  m_sgb = FALSE;
  //}}AFX_DATA_INIT
}


void Associate::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(Associate)
  DDX_Check(pDX, IDC_AGB, m_agb);
  DDX_Check(pDX, IDC_BIN, m_bin);
  DDX_Check(pDX, IDC_CGB, m_cgb);
  DDX_Check(pDX, IDC_GB, m_gb);
  DDX_Check(pDX, IDC_GBA, m_gba);
  DDX_Check(pDX, IDC_GBC, m_gbc);
  DDX_Check(pDX, IDC_SGB, m_sgb);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Associate, CDialog)
  //{{AFX_MSG_MAP(Associate)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// Associate message handlers

BOOL Associate::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void Associate::OnCancel() 
{
  EndDialog(FALSE);
}

void Associate::OnOk() 
{
  UpdateData();

  int mask = 0;
  if(m_gb)
    mask |= 1;
  if(m_sgb)
    mask |= 2;
  if(m_cgb)
    mask |= 4;
  if(m_gbc)
    mask |= 8;
  if(m_gba)
    mask |= 16;
  if(m_agb)
    mask |= 32;
  if(m_bin)
    mask |= 64;
  if(mask) {
    char applicationPath[2048];
    CString commandPath;
    LPCTSTR types[] = { ".gb", ".sgb", ".cgb", ".gbc", ".gba", ".agb", ".bin" };
    GetModuleFileName(NULL, applicationPath, 2048);
    commandPath.Format("\"%s\" \"%%1\"", applicationPath);
    regAssociateType("VisualBoyAdvance.Binary",
                     "Binary",
                     commandPath);
  
    for(int i = 0; i < 7; i++) {
      if(mask & (1<<i)) {
        regCreateFileType(types[i],"VisualBoyAdvance.Binary");
      }
    }
  }
  EndDialog(TRUE);
}
