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

// GBPaletteView.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "FileDlg.h"
#include "GBPaletteView.h"
#include "WinResUtil.h"

#include "../System.h"
#include "../gb/gbGlobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void GBPaletteViewControl::updatePalette()
{
  if(gbRom) {
    memcpy(palette, &gbPalette[paletteAddress], 64);
  }
}

/////////////////////////////////////////////////////////////////////////////
// GBPaletteView dialog


GBPaletteView::GBPaletteView(CWnd* pParent /*=NULL*/)
  : ResizeDlg(GBPaletteView::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBPaletteView)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  autoUpdate = false;
}

GBPaletteView::~GBPaletteView()
{
}

void GBPaletteView::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBPaletteView)
  // NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_PALETTE_VIEW, paletteView);
  DDX_Control(pDX, IDC_PALETTE_VIEW_OBJ, paletteViewOBJ);
  DDX_Control(pDX, IDC_COLOR, colorControl);
}


BEGIN_MESSAGE_MAP(GBPaletteView, CDialog)
  //{{AFX_MSG_MAP(GBPaletteView)
  ON_BN_CLICKED(IDC_SAVE_BG, OnSaveBg)
  ON_BN_CLICKED(IDC_SAVE_OBJ, OnSaveObj)
  ON_BN_CLICKED(IDC_REFRESH2, OnRefresh2)
  ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
  ON_BN_CLICKED(IDC_CLOSE, OnClose)
  //}}AFX_MSG_MAP
  ON_MESSAGE(WM_PALINFO, OnPalInfo)
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GBPaletteView message handlers

BOOL GBPaletteView::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  DIALOG_SIZER_START( sz )
    DIALOG_SIZER_END()
    SetData(sz,
            FALSE,
            HKEY_CURRENT_USER,
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\GBPaletteView",
            NULL);

  paletteView.init(32, 64, 128);
  paletteViewOBJ.init(32, 64, 128);
  
  paletteView.setPaletteAddress(0);
  paletteView.refresh();  
  
  paletteViewOBJ.setPaletteAddress(32);
  paletteViewOBJ.refresh();  
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GBPaletteView::save(int which)
{
  CString captureBuffer;

  if(which == 0)
    captureBuffer = "bg.pal";
  else
    captureBuffer = "obj.pal";

  LPCTSTR exts[] = {".pal", ".pal", ".act" };
  
  CString filter = theApp.winLoadFilter(IDS_FILTER_PAL);
  CString title = winResLoadString(IDS_SELECT_PALETTE_NAME);
  FileDlg dlg(this,
              captureBuffer,
              filter,
              1,
              "PAL",
              exts,
              "",
              title,
              true);

  if(dlg.DoModal() == IDCANCEL) {
    return;
  }

  captureBuffer = dlg.GetPathName();

  PaletteViewControl *p = NULL;

  if(which == 0)
    p = &paletteView;
  else
    p = &paletteViewOBJ;
  
  switch(dlg.getFilterIndex()) {
  case 0:
  case 1:
    p->saveMSPAL(captureBuffer);
    break;
  case 2:
    p->saveJASCPAL(captureBuffer);
    break;
  case 3:
    p->saveAdobe(captureBuffer);
    break;
  }
}


void GBPaletteView::OnSaveBg() 
{
  save(0);
}

void GBPaletteView::OnSaveObj() 
{
  save(1);
}

void GBPaletteView::OnRefresh2() 
{
  paletteView.refresh();
  paletteViewOBJ.refresh();  
}

void GBPaletteView::update()
{
  OnRefresh2();
}


void GBPaletteView::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }  
}


void GBPaletteView::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}

LRESULT GBPaletteView::OnPalInfo(WPARAM wParam, LPARAM lParam)
{
  u16 color = (u16)wParam;
  u32 address = (u32)lParam;
  CString buffer;

  bool isOBJ = address >= 32;
  address &= 31;
  
  buffer.Format("%d", address);
  GetDlgItem(IDC_ADDRESS)->SetWindowText(buffer);

  int r = (color & 0x1f);
  int g = (color & 0x3e0) >> 5;
  int b = (color & 0x7c00) >> 10;

  buffer.Format("%d", r);
  GetDlgItem(IDC_R)->SetWindowText(buffer);

  buffer.Format("%d", g);
  GetDlgItem(IDC_G)->SetWindowText(buffer);

  buffer.Format("%d", b);
  GetDlgItem(IDC_B)->SetWindowText(buffer);


  buffer.Format("0x%04X", color);
  GetDlgItem(IDC_VALUE)->SetWindowText(buffer);

  colorControl.setColor(color);

  if(isOBJ)
    paletteView.setSelected(-1);
  else
    paletteViewOBJ.setSelected(-1);
  
  return TRUE;
}

void GBPaletteView::PostNcDestroy() 
{
  delete this;
}
