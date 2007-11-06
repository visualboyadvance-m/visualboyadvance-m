// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team

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

// IOViewer.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "IOViewer.h"

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"

#include "IOViewerRegs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// IOViewer dialog


IOViewer::IOViewer(CWnd* pParent /*=NULL*/)
  : ResizeDlg(IOViewer::IDD, pParent)
{
  //{{AFX_DATA_INIT(IOViewer)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  selected = 0;
  autoUpdate = false;
}


void IOViewer::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(IOViewer)
	DDX_Control(pDX, IDC_VALUE, m_value);
  DDX_Control(pDX, IDC_ADDRESSES, m_address);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(IOViewer, CDialog)
  //{{AFX_MSG_MAP(IOViewer)
  ON_BN_CLICKED(IDC_CLOSE, OnClose)
  ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
  ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
  ON_CBN_SELCHANGE(IDC_ADDRESSES, OnSelchangeAddresses)
	ON_BN_CLICKED(IDC_APPLY, OnApply)
  ON_BN_CLICKED(IDC_BIT_0, bitChange)
  ON_BN_CLICKED(IDC_BIT_1, bitChange)
  ON_BN_CLICKED(IDC_BIT_2, bitChange)
  ON_BN_CLICKED(IDC_BIT_3, bitChange)
  ON_BN_CLICKED(IDC_BIT_4, bitChange)
  ON_BN_CLICKED(IDC_BIT_5, bitChange)
  ON_BN_CLICKED(IDC_BIT_6, bitChange)
  ON_BN_CLICKED(IDC_BIT_7, bitChange)
  ON_BN_CLICKED(IDC_BIT_8, bitChange)
  ON_BN_CLICKED(IDC_BIT_9, bitChange)
  ON_BN_CLICKED(IDC_BIT_10, bitChange)
  ON_BN_CLICKED(IDC_BIT_11, bitChange)
  ON_BN_CLICKED(IDC_BIT_12, bitChange)
  ON_BN_CLICKED(IDC_BIT_13, bitChange)
  ON_BN_CLICKED(IDC_BIT_14, bitChange)
  ON_BN_CLICKED(IDC_BIT_15, bitChange)
	//}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// IOViewer message handlers

void IOViewer::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}

void IOViewer::bitChange() 
{
  CString buffer;
  u16 data = 0;

  for(int i = 0; i < 16; i++) {
    CButton *pWnd = (CButton *)GetDlgItem(IDC_BIT_0 + i);
	  
    if(pWnd) {
      if(pWnd->GetCheck())
        data |= (1 << i);
    }
  }

  buffer.Format("%04X", data);
  m_value.SetWindowText(buffer);
}

void IOViewer::OnRefresh() 
{
  // TODO: Add your control notification handler code here

  update();  
}

void IOViewer::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }  
}

void IOViewer::OnSelchangeAddresses() 
{
  selected = m_address.GetCurSel();
 
  update();
}

void IOViewer::PostNcDestroy() 
{
  delete this;
}

BOOL IOViewer::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  // winCenterWindow(getHandle());
  DIALOG_SIZER_START( sz )
    DIALOG_SIZER_END()
    SetData(sz,
            TRUE,
            HKEY_CURRENT_USER,
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\IOView",
            NULL);

  CFont *font = CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FIXED_FONT));
  int i;
  for(i = 0; i < sizeof(ioViewRegisters)/sizeof(IOData); i++) {
    m_address.AddString(ioViewRegisters[i].name);
  }
  m_address.SetFont(font);
  for(i = 0; i < 16; i++) {
    GetDlgItem(IDC_BIT_0+i)->SetFont(font);
  }

  RECT cbSize;
  int Height;
  
  m_address.GetClientRect(&cbSize);
  Height = m_address.GetItemHeight(0);
  Height += m_address.GetItemHeight(0) * (10);
  
  // Note: The use of SM_CYEDGE assumes that we're using Windows '95
  // Now add on the height of the border of the edit box
  Height += GetSystemMetrics(SM_CYEDGE) * 2;  // top & bottom edges
  
  // The height of the border of the drop-down box
  Height += GetSystemMetrics(SM_CYEDGE) * 2;  // top & bottom edges
  
  // now set the size of the window
  m_address.SetWindowPos(NULL,
                         0, 0,
                         cbSize.right, Height,
                         SWP_NOMOVE | SWP_NOZORDER);

  m_address.SetCurSel(0);
  update();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void IOViewer::update()
{
  CString buffer;

  const IOData *sel = &ioViewRegisters[selected];
  u16 data = sel->address ? *sel->address : 
    (ioMem ? *((u16 *)&ioMem[sel->offset]) : 0);

  for(int i = 0; i < 16; i++) {
    CButton *pWnd = (CButton *)GetDlgItem(IDC_BIT_0 + i);

    if(pWnd) {
      if(!(sel->write & (1 << i)))
        pWnd->EnableWindow(FALSE);
      else
        pWnd->EnableWindow(TRUE);
      pWnd->SetCheck(((data & (1 << i)) >> i));
      buffer.Format("%2d %s", i, sel->bits[i]);
      pWnd->SetWindowText(buffer);
    }
  }

  buffer.Format("%04X", data);
  m_value.SetWindowText(buffer);
}

void IOViewer::OnApply() 
{
  if(rom != NULL)
  {
  const IOData *sel = &ioViewRegisters[selected];
  u16 res = 0;
  for(int i = 0; i < 16; i++) {
    CButton *pWnd = (CButton *)GetDlgItem(IDC_BIT_0 + i);
	  
    if(pWnd) {
      if(pWnd->GetCheck())
        res |= (1 << i);
    }
  }
  CPUWriteHalfWord(0x4000000+sel->offset, res);
  update();
  }
}
