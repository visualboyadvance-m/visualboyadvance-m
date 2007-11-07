// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005-2006 Forgotten and the VBA development team

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

// GameOverrides.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "GameOverrides.h"
#include "../GBA.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// GameOverrides dialog


GameOverrides::GameOverrides(CWnd* pParent /*=NULL*/)
  : CDialog(GameOverrides::IDD, pParent)
{
  //{{AFX_DATA_INIT(GameOverrides)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}


void GameOverrides::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(GameOverrides)
	DDX_Control(pDX, IDC_NAME, m_name);
	DDX_Control(pDX, IDC_MIRRORING, m_mirroring);
	DDX_Control(pDX, IDC_FLASH_SIZE, m_flashSize);
	DDX_Control(pDX, IDC_SAVE_TYPE, m_saveType);
	DDX_Control(pDX, IDC_RTC, m_rtc);
	DDX_Control(pDX, IDC_COMMENT, m_comment);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GameOverrides, CDialog)
  //{{AFX_MSG_MAP(GameOverrides)
  ON_BN_CLICKED(IDC_DEFAULTS, OnDefaults)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GameOverrides message handlers

void GameOverrides::OnOK() 
{
  char tempName[2048];
  
  GetModuleFileName(NULL, tempName, 2048);
  
  char *p = strrchr(tempName, '\\');
  if(p)
    *p = 0;
  
  char buffer[5];
  strncpy(buffer, (const char *)&rom[0xac], 4);
  buffer[4] = 0;
  
  strcat(tempName, "\\vba-over.ini");

  char comment[0xFF];
  m_comment.GetWindowText(comment, 0xFF);
  WritePrivateProfileString(buffer, "comment", !strncmp(comment, "", 0xFF) ? NULL : comment, tempName);

  int rtc = m_rtc.GetCurSel();
  int flash = m_flashSize.GetCurSel();
  int save = m_saveType.GetCurSel();
  int mirroring = m_mirroring.GetCurSel();
  if(rtc == 0 && flash == 0 && save == 0 && mirroring == 0)
    WritePrivateProfileString(buffer, NULL, NULL, tempName);
  else {
    char *value = NULL;
    switch(rtc) {
    case 1:
      value = "0";
      break;
    case 2:
      value = "1";
      break;
    }
    WritePrivateProfileString(buffer, "rtcEnabled", value, tempName);
    value = NULL;
    switch(flash) {
    case 1:
      value = "0x10000";
      break;
    case 2:
      value = "0x20000";
      break;
    }
    WritePrivateProfileString(buffer, "flashSize", value, tempName);
    value = NULL;
    switch(save) {
    case 1:
      value = "0";
      break;
    case 2:
      value = "1";
      break;
    case 3:
      value = "2";
      break;
    case 4:
      value = "3";
      break;
    case 5:
      value = "4";
      break;
    case 6:
      value = "5";
      break;
    }
    WritePrivateProfileString(buffer, "saveType", value, tempName);
    value = NULL;
    switch(mirroring) {
    case 1:
      value = "0";
      break;
    case 2:
      value = "1";
      break;
    }
    WritePrivateProfileString(buffer, "mirroringEnabled", value, tempName);
  }
  CDialog::OnOK();
}

void GameOverrides::OnDefaults() 
{
  m_rtc.SetCurSel(0);
  m_flashSize.SetCurSel(0);
  m_saveType.SetCurSel(0);
  m_mirroring.SetCurSel(0);
}

void GameOverrides::OnCancel() 
{
  CDialog::OnCancel();
}

BOOL GameOverrides::OnInitDialog() 
{
  CDialog::OnInitDialog();

  char tempName[2048];
  
  const char *rtcValues[] = {
    "Default",
    "Disabled",
    "Enabled"
  };
  const char *flashValues[] = {
    "Default",
    "64K",
    "128K"
  };
  const char *saveValues[] = {
    "Default",
    "Automatic",
    "EEPROM",
    "SRAM",
    "Flash",
    "EEPROM+Sensor",
    "None"
  };
  const char *mirroringValues[] = {
    "Default",
    "Disabled",
    "Enabled"
  };

  int i;

  for(i = 0; i < 3; i++) {
    m_rtc.AddString(rtcValues[i]);
  }
  for(i = 0; i < 3; i++) {
    m_flashSize.AddString(flashValues[i]);
  }
  for(i = 0; i < 7; i++) {
    m_saveType.AddString(saveValues[i]);
  }
  for(i = 0; i < 3; i++) {
    m_mirroring.AddString(mirroringValues[i]);
  }

  GetModuleFileName(NULL, tempName, 2048);
  
  char *p = strrchr(tempName, '\\');
  if(p)
    *p = 0;
  
  char buffer[5];
  strncpy(buffer, (const char *)&rom[0xac], 4);
  buffer[4] = 0;
  
  strcat(tempName, "\\vba-over.ini");

  m_name.SetWindowText(buffer);

  char comment[0xFF];
  GetPrivateProfileString(buffer,
	  "comment",
	  "",
	  comment,
	  0xFF,
	  tempName);
  m_comment.SetWindowText(comment);

  UINT v = GetPrivateProfileInt(buffer,
                                "rtcEnabled",
                                -1,
                                tempName);
  switch(v) {
  case 0:
    m_rtc.SetCurSel(1);
    break;
  case 1:
    m_rtc.SetCurSel(2);
    break;
  default:
    m_rtc.SetCurSel(0);
  }
  v = GetPrivateProfileInt(buffer,
                           "flashSize",
                           -1,
                           tempName);
  switch(v) {
  case 0x10000:
    m_flashSize.SetCurSel(1);
    break;
  case 0x20000:
    m_flashSize.SetCurSel(2);
    break;
  default:
    m_flashSize.SetCurSel(0);
  }
  
  v = GetPrivateProfileInt(buffer,
                           "saveType",
                           -1,
                           tempName);
  if(v != (UINT)-1 && (v > 5))
    v = (UINT)-1;
  if(v != (UINT)-1)
    m_saveType.SetCurSel(v+1);
  else
    m_saveType.SetCurSel(0);
  
  v = GetPrivateProfileInt(buffer,
                           "mirroringEnabled",
                            -1,
                            tempName);
  switch(v) {
  case 0:
    m_mirroring.SetCurSel(1);
    break;
  case 1:
    m_mirroring.SetCurSel(2);
    break;
  default:
    m_mirroring.SetCurSel(0);
  }

  return TRUE;  // return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE
}
