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

// MemoryViewerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "FileDlg.h"
#include "MemoryViewerAddressSize.h"
#include "MemoryViewerDlg.h"
#include "Reg.h"
#include "WinResUtil.h"

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern int emulating;

#define CPUReadByteQuick(addr) \
  ::map[(addr)>>24].address[(addr) & ::map[(addr)>>24].mask]
#define CPUWriteByteQuick(addr, b) \
  ::map[(addr)>>24].address[(addr) & ::map[(addr)>>24].mask] = (b)
#define CPUReadHalfWordQuick(addr) \
  *((u16 *)&::map[(addr)>>24].address[(addr) & ::map[(addr)>>24].mask])
#define CPUWriteHalfWordQuick(addr, b) \
  *((u16 *)&::map[(addr)>>24].address[(addr) & ::map[(addr)>>24].mask]) = (b)
#define CPUReadMemoryQuick(addr) \
  *((u32 *)&::map[(addr)>>24].address[(addr) & ::map[(addr)>>24].mask])
#define CPUWriteMemoryQuick(addr, b) \
  *((u32 *)&::map[(addr)>>24].address[(addr) & ::map[(addr)>>24].mask]) = (b)

/////////////////////////////////////////////////////////////////////////////
// GBAMemoryViewer control


GBAMemoryViewer::GBAMemoryViewer()
  : MemoryViewer()
{
  setAddressSize(0);
}

void GBAMemoryViewer::readData(u32 address, int len, u8 *data)
{
  if(emulating && rom != NULL) {
    for(int i = 0; i < len; i++) {
      *data++ = CPUReadByteQuick(address);
      address++;
    }
  } else {
    for(int i = 0; i < len; i++) {
      *data++ = 0;
      address++;
    }    
  }
}

void GBAMemoryViewer::editData(u32 address, int size, int mask, u32 value)
{
  u32 oldValue;
  
  switch(size) {
  case 8:
    oldValue = (CPUReadByteQuick(address) & mask) | value;
    CPUWriteByteQuick(address, oldValue);
    break;
  case 16:
    oldValue = (CPUReadHalfWordQuick(address) & mask) | value;
    CPUWriteHalfWordQuick(address, oldValue);
    break;
  case 32:
    oldValue = (CPUReadMemoryQuick(address) & mask) | value;
    CPUWriteMemoryQuick(address, oldValue);
    break;
  }
}


/////////////////////////////////////////////////////////////////////////////
// MemoryViewerDlg dialog


MemoryViewerDlg::MemoryViewerDlg(CWnd* pParent /*=NULL*/)
  : ResizeDlg(MemoryViewerDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(MemoryViewerDlg)
  m_size = -1;
  //}}AFX_DATA_INIT
  autoUpdate = false;
}


void MemoryViewerDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(MemoryViewerDlg)
  DDX_Control(pDX, IDC_CURRENT_ADDRESS, m_current);
  DDX_Control(pDX, IDC_ADDRESS, m_address);
  DDX_Control(pDX, IDC_ADDRESSES, m_addresses);
  DDX_Radio(pDX, IDC_8_BIT, m_size);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_VIEWER, m_viewer);
}


BEGIN_MESSAGE_MAP(MemoryViewerDlg, CDialog)
  //{{AFX_MSG_MAP(MemoryViewerDlg)
  ON_BN_CLICKED(IDC_CLOSE, OnClose)
  ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
  ON_BN_CLICKED(IDC_8_BIT, On8Bit)
  ON_BN_CLICKED(IDC_16_BIT, On16Bit)
  ON_BN_CLICKED(IDC_32_BIT, On32Bit)
  ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
  ON_BN_CLICKED(IDC_GO, OnGo)
  ON_CBN_SELCHANGE(IDC_ADDRESSES, OnSelchangeAddresses)
  ON_BN_CLICKED(IDC_SAVE, OnSave)
  ON_BN_CLICKED(IDC_LOAD, OnLoad)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// MemoryViewerDlg message handlers

BOOL MemoryViewerDlg::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  DIALOG_SIZER_START( sz )
    DIALOG_SIZER_ENTRY( IDC_VIEWER, DS_SizeX | DS_SizeY )
    DIALOG_SIZER_ENTRY( IDC_REFRESH, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_CLOSE, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_LOAD, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_SAVE, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_AUTO_UPDATE, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_CURRENT_ADDRESS_LABEL, DS_MoveY | DS_MoveX)
    DIALOG_SIZER_ENTRY( IDC_CURRENT_ADDRESS, DS_MoveY | DS_MoveX)
    DIALOG_SIZER_END()
    SetData(sz,
            TRUE,
            HKEY_CURRENT_USER,
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\MemoryView",
            NULL);
  
  m_viewer.setDialog(this);
  m_viewer.ShowScrollBar(SB_VERT, TRUE);
  m_viewer.EnableScrollBar(SB_VERT, ESB_ENABLE_BOTH);

  LPCTSTR s[] = {
    "0x00000000 - BIOS",
    "0x02000000 - WRAM",
    "0x03000000 - IRAM",
    "0x04000000 - I/O",
    "0x05000000 - PALETTE",
    "0x06000000 - VRAM",
    "0x07000000 - OAM",
    "0x08000000 - ROM"
  };

  for(int i = 0; i < 8; i++)
    m_addresses.AddString(s[i]);

  m_addresses.SetCurSel(0);

  RECT cbSize;
  int Height;
  
  m_addresses.GetClientRect(&cbSize);
  Height = m_addresses.GetItemHeight(-1);
  Height += m_addresses.GetItemHeight(0) * (9);
  
  // Note: The use of SM_CYEDGE assumes that we're using Windows '95
  // Now add on the height of the border of the edit box
  Height += GetSystemMetrics(SM_CYEDGE) * 2;  // top & bottom edges
  
  // The height of the border of the drop-down box
  Height += GetSystemMetrics(SM_CYEDGE) * 2;  // top & bottom edges
  
  // now set the size of the window
  m_addresses.SetWindowPos(NULL,
                           0, 0,
                           cbSize.right, Height,
                           SWP_NOMOVE | SWP_NOZORDER);

  m_address.LimitText(8);

  m_size = regQueryDwordValue("memViewerDataSize", 0);
  if(m_size < 0 || m_size > 2)
    m_size = 0;
  m_viewer.setSize(m_size);
  UpdateData(FALSE);

  m_current.SetFont(CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FIXED_FONT)));
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void MemoryViewerDlg::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}

void MemoryViewerDlg::OnRefresh() 
{
  m_viewer.Invalidate();
}

void MemoryViewerDlg::update()
{
  OnRefresh();
}


void MemoryViewerDlg::On8Bit() 
{
  m_viewer.setSize(0);
  regSetDwordValue("memViewerDataSize", 0);
}

void MemoryViewerDlg::On16Bit() 
{
  m_viewer.setSize(1);
  regSetDwordValue("memViewerDataSize", 1);
}

void MemoryViewerDlg::On32Bit() 
{
  m_viewer.setSize(2);
  regSetDwordValue("memViewerDataSize", 2);
}

void MemoryViewerDlg::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }  
}

void MemoryViewerDlg::OnGo() 
{
  CString buffer;
  
  m_address.GetWindowText(buffer);
  
  u32 address;
  sscanf(buffer, "%x", &address);
  if(m_viewer.getSize() == 1)
    address &= ~1;
  else if(m_viewer.getSize() == 2)
    address &= ~3;
  m_viewer.setAddress(address);
}

void MemoryViewerDlg::OnSelchangeAddresses() 
{
  int cur = m_addresses.GetCurSel();
  
  switch(cur) {
  case 0:
    m_viewer.setAddress(0);
    break;
  case 1:
    m_viewer.setAddress(0x2000000);
    break;
  case 2:
    m_viewer.setAddress(0x3000000);
    break;
  case 3:
    m_viewer.setAddress(0x4000000);
    break;
  case 4:
    m_viewer.setAddress(0x5000000);
    break;
  case 5:
    m_viewer.setAddress(0x6000000);
    break;
  case 6:
    m_viewer.setAddress(0x7000000);
    break;
  case 7:
    m_viewer.setAddress(0x8000000);
    break;
  }
}

void MemoryViewerDlg::setCurrentAddress(u32 address)
{
  CString buffer;

  buffer.Format("0x%08X", address);
  m_current.SetWindowText(buffer);
}

void MemoryViewerDlg::OnSave() 
{
  if(rom != NULL)
  {
    MemoryViewerAddressSize dlg;
    CString buffer;

    dlg.setAddress(m_viewer.getCurrentAddress());

    LPCTSTR exts[] = { ".dmp" };
  
    if(dlg.DoModal() == IDOK) {
      CString filter = theApp.winLoadFilter(IDS_FILTER_DUMP);
      CString title = winResLoadString(IDS_SELECT_DUMP_FILE);

      FileDlg file(this,
                   buffer,
                   filter,
                   0,
                   "DMP",
                   exts,
                   "",
                   title, 
                   true);
      if(file.DoModal() == IDOK) {
        buffer = file.GetPathName();

        FILE *f = fopen(buffer, "wb");
      
        if(f == NULL) {
          systemMessage(IDS_ERROR_CREATING_FILE, buffer);
          return;
        }

        int size = dlg.getSize();
        u32 addr = dlg.getAddress();

        for(int i = 0; i < size; i++) {
          fputc(CPUReadByteQuick(addr), f);
          addr++;
        }

        fclose(f);
      }
    }
  }
}

void MemoryViewerDlg::OnLoad() 
{
  if(rom != NULL)
  {
    CString buffer;
    LPCTSTR exts[] = { ".dmp" };

    CString filter = theApp.winLoadFilter(IDS_FILTER_DUMP);
    CString title = winResLoadString(IDS_SELECT_DUMP_FILE);

    FileDlg file(this,
                 buffer,
                 filter,
                 0,
                 "DMP",
                 exts,
                 "",
                 title,
                 false);
  
    if(file.DoModal() == IDOK) {
      buffer = file.GetPathName();
      FILE *f = fopen(buffer, "rb");
      if(f == NULL) {
        systemMessage(IDS_CANNOT_OPEN_FILE,
                      "Cannot open file %s",
                      buffer);
        return;
      }
    
      MemoryViewerAddressSize dlg;    

      fseek(f, 0, SEEK_END);
      int size = ftell(f);

      fseek(f, 0, SEEK_SET);
    
      dlg.setAddress(m_viewer.getCurrentAddress());
      dlg.setSize(size);
    
      if(dlg.DoModal() == IDOK) {
        int size = dlg.getSize();
        u32 addr = dlg.getAddress();

        for(int i = 0; i < size; i++) {
          int c = fgetc(f);
          if(c == -1)
            break;
          CPUWriteByteQuick(addr, c);
          addr++;
        }
        OnRefresh();
      }
      fclose(f);    
    }
  }
}

void MemoryViewerDlg::PostNcDestroy() 
{
  delete this;
}
