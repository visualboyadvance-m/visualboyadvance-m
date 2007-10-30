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

// Disassemble.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "Disassemble.h"

#include "../System.h"
#include "../armdis.h"
#include "../GBA.h"
#include "../Globals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern int emulating;

extern void CPUUpdateCPSR();


/////////////////////////////////////////////////////////////////////////////
// Disassemble dialog


Disassemble::Disassemble(CWnd* pParent /*=NULL*/)
  : ResizeDlg(Disassemble::IDD, pParent)
{
  //{{AFX_DATA_INIT(Disassemble)
  m_c = FALSE;
  m_f = FALSE;
  m_i = FALSE;
  m_n = FALSE;
  m_t = FALSE;
  m_v = FALSE;
  m_z = FALSE;
  mode = -1;
  //}}AFX_DATA_INIT
  mode = 0;
  address = 0;
  autoUpdate = false;
  count = 1;
}


void Disassemble::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(Disassemble)
  DDX_Control(pDX, IDC_ADDRESS, m_address);
  DDX_Control(pDX, IDC_DISASSEMBLE, m_list);
  DDX_Check(pDX, IDC_C, m_c);
  DDX_Check(pDX, IDC_F, m_f);
  DDX_Check(pDX, IDC_I, m_i);
  DDX_Check(pDX, IDC_N, m_n);
  DDX_Check(pDX, IDC_T, m_t);
  DDX_Check(pDX, IDC_V, m_v);
  DDX_Check(pDX, IDC_Z, m_z);
  DDX_Radio(pDX, IDC_AUTOMATIC, mode);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Disassemble, CDialog)
  //{{AFX_MSG_MAP(Disassemble)
  ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
  ON_BN_CLICKED(IDC_AUTOMATIC, OnAutomatic)
  ON_BN_CLICKED(IDC_ARM, OnArm)
  ON_BN_CLICKED(IDC_CLOSE, OnClose)
  ON_BN_CLICKED(IDC_GO, OnGo)
  ON_BN_CLICKED(IDC_GOPC, OnGopc)
  ON_BN_CLICKED(IDC_NEXT, OnNext)
  ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
  ON_BN_CLICKED(IDC_THUMB, OnThumb)
  ON_WM_VSCROLL()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// Disassemble message handlers

void Disassemble::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }  
}

void Disassemble::OnAutomatic() 
{
  mode = 0;
  refresh();
}

void Disassemble::OnArm() 
{
  mode = 1;
  refresh();
}

void Disassemble::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}

void Disassemble::OnGo() 
{
  CString buffer;
  m_address.GetWindowText(buffer);
  sscanf(buffer, "%x", &address);
  refresh();
}

void Disassemble::OnGopc() 
{
  if(armState)
    address = armNextPC - 16;
  else
    address = armNextPC - 8;

  refresh();
}

void Disassemble::OnNext() 
{
  CPULoop(1);
  if(armState) {
    u32 total = address+count*4;
    if(armNextPC >= address && armNextPC < total) {
    } else {
      OnGopc();
    }
  } else {
    u32 total = address+count*2;
    if(armNextPC >= address && armNextPC < total) {
    } else {
      OnGopc();
    }
  }
  refresh();
}

void Disassemble::OnRefresh() 
{
  refresh();
}

void Disassemble::OnThumb() 
{
  mode = 2;
  refresh();
}

BOOL Disassemble::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  DIALOG_SIZER_START( sz )
    DIALOG_SIZER_ENTRY( IDC_DISASSEMBLE, DS_SizeY)
    DIALOG_SIZER_ENTRY( IDC_REFRESH, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_CLOSE, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_NEXT,  DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_AUTO_UPDATE, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_GOPC, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_VSCROLL, DS_SizeY)
    DIALOG_SIZER_END()
    SetData(sz,
            TRUE,
            HKEY_CURRENT_USER,
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\DisassembleView",
            NULL);

  SCROLLINFO si;
  ZeroMemory(&si, sizeof(si));
  si.cbSize = sizeof(si);
  si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
  si.nMin = 0;
  si.nMax = 100;
  si.nPos = 50;
  si.nPage = 0;
  GetDlgItem(IDC_VSCROLL)->SetScrollInfo(SB_CTL, &si, TRUE);
  
  CFont *font = CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FIXED_FONT));
  
  m_list.SetFont(font, FALSE);
  for(int i = 0; i < 17; i++)
    GetDlgItem(IDC_R0+i)->SetFont(font, FALSE);

  GetDlgItem(IDC_MODE)->SetFont(font, FALSE);
  

  m_address.LimitText(8);
  refresh();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void Disassemble::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
  switch(nSBCode) {
  case SB_LINEDOWN:
    if(mode == 0) {
      if(armState)
        address += 4;
      else
        address += 2;
    } else if(mode == 1)
      address += 4;
    else
      address += 2;
    break;
  case SB_LINEUP:
    if(mode == 0) {
      if(armState)
        address -= 4;
      else
        address -= 2;
    } else if(mode == 1)
      address -= 4;
    else
      address -= 2;
    break;
  case SB_PAGEDOWN:
    if(mode == 0) {
      if(armState)
        address += count*4;
      else
        address += count*2;
    } else if(mode == 1)
      address += count*4;
    else
      address += count*2;
    break;
  case SB_PAGEUP:
    if(mode == 0) {
      if(armState)
        address -= count*4;
      else
        address -= count*2;
    } else if(mode == 1)
      address -= count*4;
    else
      address -= count*2;
    break;
  }
  refresh();
  
  CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}

void Disassemble::refresh()
{
  if(rom == NULL)
    return;
  
  bool arm = armState;
  
  if(mode != 0) {
    if(mode == 1)
      arm = true;
    else
      arm = false;
  }
  
  int h = m_list.GetItemHeight(0);
  RECT r;
  m_list.GetClientRect(&r);
  count = (r.bottom - r.top+1)/h;

  m_list.ResetContent();
  if(!emulating && theApp.cartridgeType == 0)
    return;
  
  char buffer[80];
  u32 addr = address;
  int i;
  int sel = -1;
  for(i = 0; i < count; i++) {
    if(addr == armNextPC)
      sel = i;
    if(arm) {
      addr += disArm(addr, buffer, 3);
    } else {
      addr += disThumb(addr, buffer, 3);
    }
    m_list.InsertString(-1, buffer);
  }

  if(sel != -1)
    m_list.SetCurSel(sel);

  CPUUpdateCPSR();
  
  for(i = 0; i < 17; i++) {
    sprintf(buffer, "%08x", reg[i].I);
    GetDlgItem(IDC_R0+i)->SetWindowText(buffer);
  }

  m_n = (reg[16].I & 0x80000000) != 0;
  m_z = (reg[16].I & 0x40000000) != 0;
  m_c = (reg[16].I & 0x20000000) != 0;
  m_v = (reg[16].I & 0x10000000) != 0;
  m_i = (reg[16].I & 0x80) != 0;
  m_f = (reg[16].I & 0x40) != 0;
  m_t = (reg[16].I & 0x20) != 0;

  UpdateData(FALSE);

  int v = reg[16].I & 0x1f;
  sprintf(buffer, "%02x", v);
  GetDlgItem(IDC_MODE)->SetWindowText(buffer);
}

void Disassemble::update()
{
  OnGopc();
  refresh();
}

void Disassemble::PostNcDestroy() 
{
  delete this;
}
