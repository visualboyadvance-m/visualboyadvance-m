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

// GBOamView.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "FileDlg.h"
#include "GBOamView.h"
#include "Reg.h"
#include "WinResUtil.h"

#include "../System.h"
#include "../NLS.h"
#include "../Util.h"
#include "../gb/gbGlobals.h"

extern "C" {
#include <png.h>
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// GBOamView dialog


GBOamView::GBOamView(CWnd* pParent /*=NULL*/)
  : ResizeDlg(GBOamView::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBOamView)
  m_stretch = FALSE;
  //}}AFX_DATA_INIT
  autoUpdate = false;
  
  memset(&bmpInfo.bmiHeader, 0, sizeof(bmpInfo.bmiHeader));
  
  bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
  bmpInfo.bmiHeader.biWidth = 8;
  bmpInfo.bmiHeader.biHeight = 16;
  bmpInfo.bmiHeader.biPlanes = 1;
  bmpInfo.bmiHeader.biBitCount = 24;
  bmpInfo.bmiHeader.biCompression = BI_RGB;
  data = (u8 *)calloc(1, 3 * 8 * 16);

  oamView.setData(data);
  oamView.setBmpInfo(&bmpInfo);

  number = 0;
}


void GBOamView::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBOamView)
  DDX_Control(pDX, IDC_SPRITE, m_sprite);
  DDX_Check(pDX, IDC_STRETCH, m_stretch);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_COLOR, color);
  DDX_Control(pDX, IDC_OAM_VIEW, oamView);
  DDX_Control(pDX, IDC_OAM_VIEW_ZOOM, oamZoom);
}


BEGIN_MESSAGE_MAP(GBOamView, CDialog)
  //{{AFX_MSG_MAP(GBOamView)
  ON_BN_CLICKED(IDC_STRETCH, OnStretch)
  ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
  ON_EN_CHANGE(IDC_SPRITE, OnChangeSprite)
  ON_BN_CLICKED(IDC_CLOSE, OnClose)
  ON_WM_HSCROLL()
  //}}AFX_MSG_MAP
  ON_MESSAGE(WM_MAPINFO, OnMapInfo)
  ON_MESSAGE(WM_COLINFO, OnColInfo)
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GBOamView message handlers

GBOamView::~GBOamView()
{
  free(data);
  data = NULL;
}


void GBOamView::paint()
{
  if(gbRom == NULL)
    return;
  
  render();
  oamView.setSize(w,h);
  oamView.refresh();
}

void GBOamView::update()
{
  paint();
}


void GBOamView::setAttributes(int y, int x, int tile, int flags)
{
  CString buffer;
  
  int flipH = flags & 0x20;
  int flipV = flags & 0x40;
  int prio = (flags & 0x80) >> 7;
  int pal = flags & 0x7;
  int oap = (flags & 0x08) >> 3;
  int bank = (flags & 0x10) >> 4;

  buffer.Format("%d,%d", x,y);
  GetDlgItem(IDC_POS)->SetWindowText(buffer);

  buffer.Format("%d", pal);
  GetDlgItem(IDC_PALETTE)->SetWindowText(buffer);

  buffer.Format("%d", tile);
  GetDlgItem(IDC_TILE)->SetWindowText(buffer);

  buffer.Format("%d", prio);
  GetDlgItem(IDC_PRIO)->SetWindowText(buffer);

  buffer.Format("%d", bank);
  GetDlgItem(IDC_BANK)->SetWindowText(buffer);
  
  buffer.Empty();
  if(flipH)
    buffer += 'H';
  else
    buffer += ' ';
  if(flipV)
    buffer += 'V';
  else
    buffer += ' ';
  GetDlgItem(IDC_FLAGS)->SetWindowText(buffer);

  buffer.Format("%d", oap);
  GetDlgItem(IDC_OAP)->SetWindowText(buffer);
}

void GBOamView::render()
{
  int m=0;
  if(gbRom == NULL)
    return;

  u16 addr = number * 4 + 0xfe00;

  int size = register_LCDC & 4;
  
  u8 y = gbMemory[addr++];
  u8 x = gbMemory[addr++];
  u8 tile = gbMemory[addr++];
  if(size)
    tile &= 254;
  u8 flags = gbMemory[addr++];
  
  u8 *bmp = data;
  
  w = 8;
  h = size ? 16 : 8;

  setAttributes(y, x, tile, flags);

  u8 * bank0;
  u8 * bank1;
  if(gbCgbMode) {
    if(register_VBK & 1) {
      bank0 = &gbVram[0x0000];
      bank1 = &gbVram[0x2000];
    } else {
      bank0 = &gbVram[0x0000];
      bank1 = &gbVram[0x2000];
    }
  } else {
    bank0 = &gbMemory[0x8000];
    bank1 = NULL;
  }
  
  int init = 0x0000;

  u8 *pal = gbObp0;

  if((flags & 0x10))
    pal = gbObp1;
  
  for(int yy = 0; yy < h; yy++) {
    int address = init + tile * 16 + 2*yy;
    int a = 0;
    int b = 0;
    
    if(gbCgbMode && flags & 0x08) {
      a = bank1[address++];
      b = bank1[address++];
    } else {
      a = bank0[address++];
      b = bank0[address++];
    }
    
    for(int xx = 0; xx < 8; xx++) {
      u8 mask = 1 << (7-xx);
      u8 c = 0;
      if( (a & mask))
        c++;
      if( (b & mask))
        c+=2;
      
      // make sure that sprites will work even in CGB mode
      if(gbCgbMode) {
        c = c + (flags & 0x07)*4 + 32;
      } else {
        c = pal[c];
      }
      
      u16 color = gbPalette[c];
      *bmp++ = ((color >> 10) & 0x1f) << 3;
      *bmp++ = ((color >> 5) & 0x1f) << 3;
      *bmp++ = (color & 0x1f) << 3;
    }
  }
}

void GBOamView::saveBMP(const char *name)
{
  u8 writeBuffer[1024 * 3];
  
  FILE *fp = fopen(name,"wb");

  if(!fp) {
    systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s", name);
    return;
  }

  struct {
    u8 ident[2];
    u8 filesize[4];
    u8 reserved[4];
    u8 dataoffset[4];
    u8 headersize[4];
    u8 width[4];
    u8 height[4];
    u8 planes[2];
    u8 bitsperpixel[2];
    u8 compression[4];
    u8 datasize[4];
    u8 hres[4];
    u8 vres[4];
    u8 colors[4];
    u8 importantcolors[4];
    u8 pad[2];
  } bmpheader;
  memset(&bmpheader, 0, sizeof(bmpheader));

  bmpheader.ident[0] = 'B';
  bmpheader.ident[1] = 'M';

  u32 fsz = sizeof(bmpheader) + w*h*3;
  utilPutDword(bmpheader.filesize, fsz);
  utilPutDword(bmpheader.dataoffset, 0x38);
  utilPutDword(bmpheader.headersize, 0x28);
  utilPutDword(bmpheader.width, w);
  utilPutDword(bmpheader.height, h);
  utilPutDword(bmpheader.planes, 1);
  utilPutDword(bmpheader.bitsperpixel, 24);
  utilPutDword(bmpheader.datasize, 3*w*h);

  fwrite(&bmpheader, 1, sizeof(bmpheader), fp);

  u8 *b = writeBuffer;

  int sizeX = w;
  int sizeY = h;

  u8 *pixU8 = (u8 *)data+3*w*(h-1);
  for(int y = 0; y < sizeY; y++) {
    for(int x = 0; x < sizeX; x++) {
      *b++ = *pixU8++; // B
      *b++ = *pixU8++; // G
      *b++ = *pixU8++; // R
    }
    pixU8 -= 2*3*w;
    fwrite(writeBuffer, 1, 3*w, fp);
    
    b = writeBuffer;
  }

  fclose(fp);
}


void GBOamView::savePNG(const char *name)
{
  u8 writeBuffer[1024 * 3];
  
  FILE *fp = fopen(name,"wb");

  if(!fp) {
    systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s", name);
    return;
  }
  
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                NULL,
                                                NULL,
                                                NULL);
  if(!png_ptr) {
    fclose(fp);
    return;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);

  if(!info_ptr) {
    png_destroy_write_struct(&png_ptr,NULL);
    fclose(fp);
    return;
  }

  if(setjmp(png_ptr->jmpbuf)) {
    png_destroy_write_struct(&png_ptr,NULL);
    fclose(fp);
    return;
  }

  png_init_io(png_ptr,fp);

  png_set_IHDR(png_ptr,
               info_ptr,
               w,
               h,
               8,
               PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr,info_ptr);

  u8 *b = writeBuffer;

  int sizeX = w;
  int sizeY = h;

  u8 *pixU8 = (u8 *)data;
  for(int y = 0; y < sizeY; y++) {
    for(int x = 0; x < sizeX; x++) {
      int blue = *pixU8++;
      int green = *pixU8++;
      int red = *pixU8++;
      
      *b++ = red;
      *b++ = green;
      *b++ = blue;
    }
    png_write_row(png_ptr,writeBuffer);
    
    b = writeBuffer;
  }
  
  png_write_end(png_ptr, info_ptr);

  png_destroy_write_struct(&png_ptr, &info_ptr);

  fclose(fp);
}


void GBOamView::save()
{
  CString captureBuffer;

  if(theApp.captureFormat == 0)
    captureBuffer = "oam.png";
  else
    captureBuffer = "oam.bmp";

  LPCTSTR exts[] = {".png", ".bmp" };

  CString filter = theApp.winLoadFilter(IDS_FILTER_PNG);
  CString title = winResLoadString(IDS_SELECT_CAPTURE_NAME);

  FileDlg dlg(this,
              captureBuffer,
              filter,
              theApp.captureFormat ? 2 : 1,
              theApp.captureFormat ? "BMP" : "PNG",
              exts,
              "",
              title, 
              true);

  if(dlg.DoModal() == IDCANCEL) {
    return;
  }
  captureBuffer = dlg.GetPathName();

  if(dlg.getFilterIndex() == 2)
    saveBMP(captureBuffer);
  else
    savePNG(captureBuffer);  
}

BOOL GBOamView::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  DIALOG_SIZER_START( sz )
    DIALOG_SIZER_ENTRY( IDC_OAM_VIEW, DS_SizeX | DS_SizeY )
    DIALOG_SIZER_ENTRY( IDC_OAM_VIEW_ZOOM, DS_MoveX)
    DIALOG_SIZER_ENTRY( IDC_REFRESH, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_SAVE,  DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_CLOSE, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_COLOR, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_R, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_G, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_B, DS_MoveY)    
    DIALOG_SIZER_END()
    SetData(sz,
            TRUE,
            HKEY_CURRENT_USER,
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\GBOamView",
            NULL);
  m_sprite.SetWindowText("0");

  updateScrollInfo();

  m_stretch = regQueryDwordValue("GBOamViewStretch", 0);
  if(m_stretch)
    oamView.setStretch(true);
  UpdateData(FALSE);
  
  paint();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GBOamView::OnStretch() 
{
  oamView.setStretch(!oamView.getStretch());
  paint();
  regSetDwordValue("GBOamViewStretch", oamView.getStretch());  
}

void GBOamView::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }  
}


void GBOamView::OnChangeSprite() 
{
  CString buffer;
  m_sprite.GetWindowText(buffer);
  int n = atoi(buffer);
  if(n < 0 || n > 39) {
    buffer.Format("%d", number);
    m_sprite.SetWindowText(buffer);
    return;
  }
  number = n;
  paint();
  updateScrollInfo();
}

void GBOamView::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}

LRESULT GBOamView::OnMapInfo(WPARAM, LPARAM lParam)
{
  u8 *colors = (u8 *)lParam;
  oamZoom.setColors(colors);
  
  return TRUE;
}

LRESULT GBOamView::OnColInfo(WPARAM wParam, LPARAM lParam)
{
  u16 c = (u16)wParam;

  color.setColor(c);  

  int r = (c & 0x1f);
  int g = (c & 0x3e0) >> 5;
  int b = (c & 0x7c00) >> 10;

  CString buffer;
  buffer.Format("R: %d", r);
  GetDlgItem(IDC_R)->SetWindowText(buffer);

  buffer.Format("G: %d", g);
  GetDlgItem(IDC_G)->SetWindowText(buffer);

  buffer.Format("B: %d", b);
  GetDlgItem(IDC_B)->SetWindowText(buffer);

  return TRUE;
}


void GBOamView::updateScrollInfo()
{
  SCROLLINFO si;
  ZeroMemory(&si, sizeof(si));
  si.cbSize = sizeof(si);
  si.fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL | SIF_POS;
  si.nMin = 0;
  si.nMax = 39;
  si.nPage = 1;
  si.nPos = number;
  GetDlgItem(IDC_SCROLLBAR)->SetScrollInfo(SB_CTL,
                                           &si,
                                           TRUE);    
}


void GBOamView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
  switch(nSBCode) {
  case SB_BOTTOM:
    number = 39;
    break;
  case SB_LINEDOWN:
    number++;
    if(number > 39)
      number = 39;
    break;
  case SB_LINEUP:
    number--;
    if(number < 0)
      number = 0;
    break;
  case SB_PAGEDOWN:
    number += 16;
    if(number > 39)
      number = 39;
    break;
  case SB_PAGEUP:
    number -= 16;
    if(number < 0)
      number = 0;
    break;
  case SB_TOP:
    number = 0;
    break;
  case SB_THUMBTRACK:
    number = nPos;
    if(number < 0)
      number = 0;
    if(number > 39)
      number = 39;
    break;
  }

  updateScrollInfo();
  
  CString buffer;
  buffer.Format("%d", number);
  m_sprite.SetWindowText(buffer);
  paint();
}

void GBOamView::PostNcDestroy() 
{
  delete this;
}
