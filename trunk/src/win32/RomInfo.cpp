// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004-2006 Forgotten and the VBA development team

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

// RomInfo.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "RomInfo.h"
#include "WinResUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern int gbRomSize;

struct WinGBACompanyName {
  LPCTSTR code;
  LPCTSTR name;
};

static WinGBACompanyName winGBARomInfoCompanies[] = {
  { "01", "Nintendo" },
  { "02", "Rocket Games" },
  { "08", "Capcom" },
  { "09", "Hot B Co." },
  { "0A", "Jaleco" },
  { "0B", "Coconuts Japan" },
  { "0C", "Coconuts Japan/G.X.Media" },
  { "0H", "Starfish" },
  { "0L", "Warashi Inc." },
  { "0N", "Nowpro" },
  { "0P", "Game Village" },
  { "13", "Electronic Arts Japan" },
  { "18", "Hudson Soft Japan" },
  { "19", "S.C.P." },
  { "1A", "Yonoman" },
  { "1G", "SMDE" },
  { "1P", "Creatures Inc." },
  { "1Q", "TDK Deep Impresion" },
  { "20", "Destination Software" },
  { "22", "VR 1 Japan" },
  { "25", "San-X" },
  { "28", "Kemco Japan" },
  { "29", "Seta" },
  { "2H", "Ubisoft Japan" },
  { "2K", "NEC InterChannel" },
  { "2L", "Tam" },
  { "2M", "Jordan" },
  { "2N", "Smilesoft" },
  { "2Q", "Mediakite" },
  { "36", "Codemasters" },
  { "37", "GAGA Communications" },
  { "38", "Laguna" },
  { "39", "Telstar Fun and Games" },
  { "41", "Ubi Soft Entertainment" },
  { "42", "Sunsoft" },
  { "47", "Spectrum Holobyte" },
  { "49", "IREM" },
  { "4D", "Malibu Games" },
  { "4F", "Eidos/U.S. Gold" },
  { "4J", "Fox Interactive" },
  { "4K", "Time Warner Interactive" },
  { "4Q", "Disney" },
  { "4S", "Black Pearl" },
  { "4X", "GT Interactive" },
  { "4Y", "RARE" },
  { "4Z", "Crave Entertainment" },
  { "50", "Absolute Entertainment" },
  { "51", "Acclaim" },
  { "52", "Activision" },
  { "53", "American Sammy Corp." },
  { "54", "Take 2 Interactive" },
  { "55", "Hi Tech" },
  { "56", "LJN LTD." },
  { "58", "Mattel" },
  { "5A", "Mindscape/Red Orb Ent." },
  { "5C", "Taxan" },
  { "5D", "Midway" },
  { "5F", "American Softworks" },
  { "5G", "Majesco Sales Inc" },
  { "5H", "3DO" },
  { "5K", "Hasbro" },
  { "5L", "NewKidCo" },
  { "5M", "Telegames" },
  { "5N", "Metro3D" },
  { "5P", "Vatical Entertainment" },
  { "5Q", "LEGO Media" },
  { "5S", "Xicat Interactive" },
  { "5T", "Cryo Interactive" },
  { "5W", "Red Storm Ent./BKN Ent." },
  { "5X", "Microids" },
  { "5Z", "Conspiracy Entertainment Corp." },
  { "60", "Titus Interactive Studios" },
  { "61", "Virgin Interactive" },
  { "62", "Maxis" },
  { "64", "LucasArts Entertainment" },
  { "67", "Ocean" },
  { "69", "Electronic Arts" },
  { "6E", "Elite Systems Ltd." },
  { "6F", "Electro Brain" },
  { "6G", "The Learning Company" },
  { "6H", "BBC" },
  { "6J", "Software 2000" },
  { "6L", "BAM! Entertainment" },
  { "6M", "Studio 3" },
  { "6Q", "Classified Games" },
  { "6S", "TDK Mediactive" },
  { "6U", "DreamCatcher" },
  { "6V", "JoWood Productions" },
  { "6W", "SEGA" },
  { "6X", "Wannado Edition" },
  { "6Y", "LSP" },
  { "6Z", "ITE Media" },
  { "70", "Infogrames" },
  { "71", "Interplay" },
  { "72", "JVC Musical Industries Inc" },
  { "73", "Parker Brothers" },
  { "75", "SCI" },
  { "78", "THQ" },
  { "79", "Accolade" },
  { "7A", "Triffix Ent. Inc." },
  { "7C", "Microprose Software" },
  { "7D", "Universal Interactive Studios" },
  { "7F", "Kemco" },
  { "7G", "Rage Software" },
  { "7H", "Encore" },
  { "7J", "Zoo" },
  { "7K", "BVM" },
  { "7L", "Simon & Schuster Interactive" },
  { "7M", "Asmik Ace Entertainment Inc./AIA" },
  { "7N", "Empire Interactive" },
  { "7Q", "Jester Interactive" },
  { "7T", "Scholastic" },
  { "7U", "Ignition Entertainment" },
  { "7W", "Stadlbauer" },
  { "80", "Misawa" },
  { "83", "LOZC" },
  { "8B", "Bulletproof Software" },
  { "8C", "Vic Tokai Inc." },
  { "8J", "General Entertainment" },
  { "8N", "Success" },
  { "8P", "SEGA Japan" },
  { "91", "Chun Soft" },
  { "92", "Video System" },
  { "93", "BEC" },
  { "96", "Yonezawa/S'pal" },
  { "97", "Kaneko" },
  { "99", "Victor Interactive Software" },
  { "9A", "Nichibutsu/Nihon Bussan" },
  { "9B", "Tecmo" },
  { "9C", "Imagineer" },
  { "9F", "Nova" },
  { "9H", "Bottom Up" },
  { "9L", "Hasbro Japan" },
  { "9N", "Marvelous Entertainment" },
  { "9P", "Keynet Inc." },
  { "9Q", "Hands-On Entertainment" },
  { "A0", "Telenet" },
  { "A1", "Hori" },
  { "A4", "Konami" },
  { "A6", "Kawada" },
  { "A7", "Takara" },
  { "A9", "Technos Japan Corp." },
  { "AA", "JVC" },
  { "AC", "Toei Animation" },
  { "AD", "Toho" },
  { "AF", "Namco" },
  { "AG", "Media Rings Corporation" },
  { "AH", "J-Wing" },
  { "AK", "KID" },
  { "AL", "MediaFactory" },
  { "AP", "Infogrames Hudson" },
  { "AQ", "Kiratto. Ludic Inc" },
  { "B0", "Acclaim Japan" },
  { "B1", "ASCII" },
  { "B2", "Bandai" },
  { "B4", "Enix" },
  { "B6", "HAL Laboratory" },
  { "B7", "SNK" },
  { "B9", "Pony Canyon Hanbai" },
  { "BA", "Culture Brain" },
  { "BB", "Sunsoft" },
  { "BD", "Sony Imagesoft" },
  { "BF", "Sammy" },
  { "BG", "Magical" },
  { "BJ", "Compile" },
  { "BL", "MTO Inc." },
  { "BN", "Sunrise Interactive" },
  { "BP", "Global A Entertainment" },
  { "BQ", "Fuuki" },
  { "C0", "Taito" },
  { "C2", "Kemco" },
  { "C3", "Square Soft" },
  { "C5", "Data East" },
  { "C6", "Tonkin House" },
  { "C8", "Koei" },
  { "CA", "Konami/Palcom/Ultra" },
  { "CB", "Vapinc/NTVIC" },
  { "CC", "Use Co.,Ltd." },
  { "CD", "Meldac" },
  { "CE", "FCI/Pony Canyon" },
  { "CF", "Angel" },
  { "CM", "Konami Computer Entertainment Osaka" },
  { "CP", "Enterbrain" },
  { "D1", "Sofel" },
  { "D2", "Quest" },
  { "D3", "Sigma Enterprises" },
  { "D4", "Ask Kodansa" },
  { "D6", "Naxat" },
  { "D7", "Copya System" },
  { "D9", "Banpresto" },
  { "DA", "TOMY" },
  { "DB", "LJN Japan" },
  { "DD", "NCS" },
  { "DF", "Altron Corporation" },
  { "DH", "Gaps Inc." },
  { "DN", "ELF" },
  { "E2", "Yutaka" },
  { "E3", "Varie" },
  { "E5", "Epoch" },
  { "E7", "Athena" },
  { "E8", "Asmik Ace Entertainment Inc." },
  { "E9", "Natsume" },
  { "EA", "King Records" },
  { "EB", "Atlus" },
  { "EC", "Epic/Sony Records" },
  { "EE", "IGS" },
  { "EL", "Spike" },
  { "EM", "Konami Computer Entertainment Tokyo" },
  { "EN", "Alphadream Corporation" },
  { "F0", "A Wave" },
  { "G1", "PCCW" },
  { "G4", "KiKi Co Ltd" },
  { "G5", "Open Sesame Inc." },
  { "G6", "Sims" },
  { "G7", "Broccoli" },
  { "G8", "Avex" },
  { "G9", "D3 Publisher" },
  { "GB", "Konami Computer Entertainment Japan" },
  { "GD", "Square-Enix" },
  { "HY", "Sachen" },
  { NULL, NULL }
};

static LPCTSTR winGBARomInfoFindMakerCode(LPCTSTR code)
{
  int i = 0;
  while(winGBARomInfoCompanies[i].code) {
    if(!strcmp(winGBARomInfoCompanies[i].code, code))
      return winGBARomInfoCompanies[i].name;
    i++;
  }
  return (LPCTSTR)winResLoadString(IDS_UNKNOWN);
}


/////////////////////////////////////////////////////////////////////////////
// RomInfoGB dialog


RomInfoGB::RomInfoGB(u8 *rom, CWnd* pParent /*=NULL*/)
  : CDialog(RomInfoGB::IDD, pParent)
{
  //{{AFX_DATA_INIT(RomInfoGB)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  this->rom = rom;
}


void RomInfoGB::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(RomInfoGB)
  // NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(RomInfoGB, CDialog)
  //{{AFX_MSG_MAP(RomInfoGB)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// RomInfoGB message handlers

void RomInfoGB::OnOk() 
{
  EndDialog(TRUE);
}

BOOL RomInfoGB::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  char buffer[128];
  
  strncpy(buffer, (const char *)&rom[0x134], 15);
  buffer[15] = 0;
  GetDlgItem(IDC_ROM_TITLE)->SetWindowText(buffer);

  sprintf(buffer, "%02x", rom[0x143]);
  GetDlgItem(IDC_ROM_COLOR)->SetWindowText(buffer);
  
  strncpy(buffer, (const char *)&rom[0x144],2);
  buffer[2] = 0;
  GetDlgItem(IDC_ROM_MAKER_CODE)->SetWindowText(buffer);

  if(rom[0x14b] != 0x33) {
    sprintf(buffer, "%02X", rom[0x14b]);
    GetDlgItem(IDC_ROM_MAKER_CODE)->SetWindowText(buffer);    
  }
  GetDlgItem(IDC_ROM_MAKER_NAME2)->SetWindowText(winGBARomInfoFindMakerCode(buffer));
  
  sprintf(buffer, "%02x", rom[0x146]);
  GetDlgItem(IDC_ROM_UNIT_CODE)->SetWindowText(buffer);

  CString type = winResLoadString(IDS_UNKNOWN);
  switch(rom[0x147]) {
  case 0x00:
    type = "ROM";
    break;
  case 0x01:
    type = "ROM+MBC1";
    break;
  case 0x02:
    type = "ROM+MBC1+RAM";
    break;
  case 0x03:
    type = "ROM+MBC1+RAM+BATT";
    break;
  case 0x05:
    type = "ROM+MBC2";
    break;
  case 0x06:
    type = "ROM+MBC2+BATT";
    break;
  case 0x0b:
    type = "ROM+MMM01";
    break;
  case 0x0c:
    type = "ROM+MMM01+RAM";
    break;
  case 0x0d:
    type = "ROM+MMM01+RAM+BATT";
    break;
  case 0x0f:
    type = "ROM+MBC3+TIMER+BATT";
    break;
  case 0x10:
    type = "ROM+MBC3+TIMER+RAM+BATT";
    break;
  case 0x11:
    type = "ROM+MBC3";
    break;
  case 0x12:
    type = "ROM+MBC3+RAM";
    break;
  case 0x13:
    type = "ROM+MBC3+RAM+BATT";
    break;
  case 0x19:
    type = "ROM+MBC5";
    break;
  case 0x1a:
    type = "ROM+MBC5+RAM";
    break;
  case 0x1b:
    type = "ROM+MBC5+RAM+BATT";
    break;
  case 0x1c:
    type = "ROM+MBC5+RUMBLE";
    break;
  case 0x1d:
    type = "ROM+MBC5+RUMBLE+RAM";
    break;
  case 0x1e:
    type = "ROM+MBC5+RUMBLE+RAM+BATT";
    break;
  case 0x22:
    type = "ROM+MBC7+BATT";
    break;
  case 0x55:
    type = "GameGenie";
    break;
  case 0x56:
    type = "GameShark V3.0";
    break;
  case 0xfc:
    type = "ROM+POCKET CAMERA";
    break;
  case 0xfd:
    type = "ROM+BANDAI TAMA5";
    break;
  case 0xfe:
    type = "ROM+HuC-3";
    break;
  case 0xff:
    type = "ROM+HuC-1";
    break;
  }
  sprintf(buffer, "%02x (%s)", rom[0x147], (const char *)type);
  GetDlgItem(IDC_ROM_DEVICE_TYPE)->SetWindowText(buffer);

  type = winResLoadString(IDS_UNKNOWN);
  switch(rom[0x148]) {
  case 0:
    type = "32K";
    break;
  case 1:
    type = "64K";
    break;
  case 2:
    type = "128K";
    break;
  case 3:
    type = "256K";
    break;
  case 4:
    type = "512K";
    break;
  case 5:
    type = "1M";
    break;
  case 6:
    type = "2M";
    break;
  case 7:
    type = "4M";
    break;
  }

  sprintf(buffer, "%02x (%s)", rom[0x148], (const char *)type);
  GetDlgItem(IDC_ROM_SIZE)->SetWindowText(buffer);

  type = winResLoadString(IDS_UNKNOWN);
  switch(rom[0x149]) {
  case 0:
    type = winResLoadString(IDS_NONE);
    break;
  case 1:
    type = "2K";
    break;
  case 2:
    type = "8K";
    break;
  case 3:
    type = "32K";
    break;
  case 4:
    type = "128K";
    break;
  case 5:
    type = "64K";
    break;
  }

  sprintf(buffer, "%02x (%s)", rom[0x149], (const char *)type);
  GetDlgItem(IDC_ROM_RAM_SIZE)->SetWindowText(buffer);

  sprintf(buffer, "%02x", rom[0x14a]);
  GetDlgItem(IDC_ROM_DEST_CODE)->SetWindowText(buffer);

  sprintf(buffer, "%02x", rom[0x14b]);
  GetDlgItem(IDC_ROM_LIC_CODE)->SetWindowText(buffer);
  
  sprintf(buffer, "%02x", rom[0x14c]);
  GetDlgItem(IDC_ROM_VERSION)->SetWindowText(buffer);

  u8 crc = 25;
  int i;
  for(i = 0x134; i < 0x14d; i++) {
    crc += rom[i];
  }

  crc = 256 - crc;
  
  sprintf(buffer, "%02x (%02x)", crc, rom[0x14d]);
  GetDlgItem(IDC_ROM_CRC)->SetWindowText(buffer);

  u16 crc16 = 0;
  for(i = 0; i < gbRomSize; i++) {
    crc16 += rom[i];
  }

  crc16 -= rom[0x14e];
  crc16 -= rom[0x14f];
  sprintf(buffer, "%04x (%04x)", crc16, (rom[0x14e]<<8)|rom[0x14f]);
  GetDlgItem(IDC_ROM_CHECKSUM)->SetWindowText(buffer);

  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}
/////////////////////////////////////////////////////////////////////////////
// RomInfoGBA dialog


RomInfoGBA::RomInfoGBA(u8 *rom, CWnd* pParent /*=NULL*/)
  : CDialog(RomInfoGBA::IDD, pParent)
{
  //{{AFX_DATA_INIT(RomInfoGBA)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  this->rom = rom;
}


void RomInfoGBA::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(RomInfoGBA)
  // NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(RomInfoGBA, CDialog)
  //{{AFX_MSG_MAP(RomInfoGBA)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// RomInfoGBA message handlers

void RomInfoGBA::OnOk() 
{
  EndDialog(TRUE);
}

BOOL RomInfoGBA::OnInitDialog() 
{
  CDialog::OnInitDialog();

  char buffer[13];

  strncpy(buffer, (const char *)&rom[0xa0], 12);
  buffer[12] = 0;
  GetDlgItem(IDC_ROM_TITLE)->SetWindowText(buffer);

  strncpy(buffer, (const char *)&rom[0xac], 4);
  buffer[4] = 0;
  GetDlgItem(IDC_ROM_GAME_CODE)->SetWindowText(buffer);

  strncpy(buffer, (const char *)&rom[0xb0],2);
  buffer[2] = 0;
  GetDlgItem(IDC_ROM_MAKER_CODE)->SetWindowText(buffer);

  GetDlgItem(IDC_ROM_MAKER_NAME)->SetWindowText(winGBARomInfoFindMakerCode(buffer));
  
  sprintf(buffer, "%02x", rom[0xb3]);
  GetDlgItem(IDC_ROM_UNIT_CODE)->SetWindowText(buffer);

  sprintf(buffer, "%02x", rom[0xb4]);
  if( rom[0xb4] & 0x80 ) {
	  strcat(buffer, " (DACS)");
  }
  GetDlgItem(IDC_ROM_DEVICE_TYPE)->SetWindowText(buffer);

  sprintf(buffer, "%02x", rom[0xbc]);
  GetDlgItem(IDC_ROM_VERSION)->SetWindowText(buffer);

  u8 crc = 0x19;
  for(int i = 0xa0; i < 0xbd; i++) {
    crc += rom[i];
  }

  crc = (-crc) & 255;

  sprintf(buffer, "%02x (%02x)", crc, rom[0xbd]);
  GetDlgItem(IDC_ROM_CRC)->SetWindowText(buffer);
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}
