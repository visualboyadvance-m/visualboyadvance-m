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

// GBDisassemble.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "GBDisassemble.h"

#include "../System.h"
#include "../gb/GB.h"
#include "../gb/gbGlobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern gbRegister AF;
extern gbRegister BC;
extern gbRegister DE;
extern gbRegister HL;
extern gbRegister SP;
extern gbRegister PC;
extern u16 IFF;
extern int gbDis(char *, u16);

/////////////////////////////////////////////////////////////////////////////
// GBDisassemble dialog


GBDisassemble::GBDisassemble(CWnd* pParent /*=NULL*/)
  : ResizeDlg(GBDisassemble::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBDisassemble)
  m_c = FALSE;
  m_h = FALSE;
  m_n = FALSE;
  m_z = FALSE;
  //}}AFX_DATA_INIT
  address = 0;
  autoUpdate = false;
  count = 1;
  lastAddress = 0;
}


void GBDisassemble::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBDisassemble)
  DDX_Control(pDX, IDC_ADDRESS, m_address);
  DDX_Control(pDX, IDC_DISASSEMBLE, m_list);
  DDX_Check(pDX, IDC_C, m_c);
  DDX_Check(pDX, IDC_H, m_h);
  DDX_Check(pDX, IDC_N, m_n);
  DDX_Check(pDX, IDC_Z, m_z);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GBDisassemble, CDialog)
  //{{AFX_MSG_MAP(GBDisassemble)
  ON_BN_CLICKED(IDC_CLOSE, OnClose)
  ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
  ON_BN_CLICKED(IDC_NEXT, OnNext)
  ON_BN_CLICKED(IDC_GO, OnGo)
  ON_BN_CLICKED(IDC_GOPC, OnGopc)
  ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
  ON_WM_VSCROLL()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GBDisassemble message handlers

void GBDisassemble::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}

void GBDisassemble::OnRefresh() 
{
  refresh();
}

void GBDisassemble::OnNext() 
{
  gbEmulate(1);
  if(PC.W < address || PC.W >= lastAddress)
    OnGopc();
  refresh();
}

void GBDisassemble::OnGo() 
{
  CString buffer;
  m_address.GetWindowText(buffer);
  sscanf(buffer, "%x", &address);
  refresh();
}

void GBDisassemble::OnGopc() 
{
  address = PC.W;

  refresh();
}

void GBDisassemble::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }  
}

BOOL GBDisassemble::OnInitDialog() 
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
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\GBDisassembleView",
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
  m_list.SetFont(font);
  
  for(int i = 0; i < 6; i++)
    GetDlgItem(IDC_R0+i)->SetFont(font);

  m_address.LimitText(4);
  refresh();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GBDisassemble::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
  char buffer[80];
  
  switch(nSBCode) {
  case SB_LINEDOWN:
    address += gbDis(buffer, address);
    break;
  case SB_LINEUP:
    address--;
    break;
  case SB_PAGEDOWN:
    address = lastAddress;
    break;
  case SB_PAGEUP:
    address -= count;
    break;
  }
  refresh();
  
  CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}

void GBDisassemble::refresh()
{
  if(gbRom == NULL)
    return;
  
  int h = m_list.GetItemHeight(0);
  RECT r;
  m_list.GetClientRect(&r);
  count = (r.bottom - r.top+1)/h;

  m_list.ResetContent();
  if(!emulating || theApp.cartridgeType != 1)
    return;
  
  char buffer[80];
  u16 addr = address;
  int i;
  int sel = -1;
  for(i = 0; i < count; i++) {
    if(addr == PC.W)
      sel = i;
    addr += gbDis(buffer, addr);
    m_list.InsertString(-1, buffer);
  }
  lastAddress = addr-1;
  if(sel != -1)
    m_list.SetCurSel(sel);

  sprintf(buffer, "%04x", AF.W);
  GetDlgItem(IDC_R0)->SetWindowText(buffer);
  sprintf(buffer, "%04x", BC.W);
  GetDlgItem(IDC_R1)->SetWindowText(buffer);  
  sprintf(buffer, "%04x", DE.W);
  GetDlgItem(IDC_R2)->SetWindowText(buffer);  
  sprintf(buffer, "%04x", HL.W);
  GetDlgItem(IDC_R3)->SetWindowText(buffer);  
  sprintf(buffer, "%04x", SP.W);
  GetDlgItem(IDC_R4)->SetWindowText(buffer);  
  sprintf(buffer, "%04x", PC.W);
  GetDlgItem(IDC_R5)->SetWindowText(buffer);  
  sprintf(buffer, "%04x", IFF);
  GetDlgItem(IDC_R6)->SetWindowText(buffer);  

  m_z = (AF.B.B0 & 0x80) != 0;
  m_n = (AF.B.B0 & 0x40) != 0;
  m_h = (AF.B.B0 & 0x20) != 0;
  m_c = (AF.B.B0 & 0x10) != 0;
  UpdateData(FALSE);
}

void GBDisassemble::update()
{
  OnGopc();
  refresh();
}

void GBDisassemble::PostNcDestroy() 
{
  delete this;
}
