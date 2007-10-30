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

// OamView.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "FileDlg.h"
#include "OamView.h"
#include "Reg.h"
#include "WinResUtil.h"

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../NLS.h"
#include "../Util.h"

extern "C" {
#include <png.h>
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// OamView dialog


OamView::OamView(CWnd* pParent /*=NULL*/)
  : ResizeDlg(OamView::IDD, pParent)
{
  //{{AFX_DATA_INIT(OamView)
  m_stretch = FALSE;
  //}}AFX_DATA_INIT
  autoUpdate = false;
  
  memset(&bmpInfo.bmiHeader, 0, sizeof(bmpInfo.bmiHeader));
  
  bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
  bmpInfo.bmiHeader.biWidth = 32;
  bmpInfo.bmiHeader.biHeight = 32;
  bmpInfo.bmiHeader.biPlanes = 1;
  bmpInfo.bmiHeader.biBitCount = 24;
  bmpInfo.bmiHeader.biCompression = BI_RGB;
  data = (u8 *)calloc(1, 3 * 64 * 64);

  oamView.setData(data);
  oamView.setBmpInfo(&bmpInfo);

  number = 0;
}


void OamView::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(OamView)
  DDX_Control(pDX, IDC_SPRITE, m_sprite);
  DDX_Check(pDX, IDC_STRETCH, m_stretch);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_COLOR, color);
  DDX_Control(pDX, IDC_OAM_VIEW, oamView);
  DDX_Control(pDX, IDC_OAM_VIEW_ZOOM, oamZoom);
}


BEGIN_MESSAGE_MAP(OamView, CDialog)
  //{{AFX_MSG_MAP(OamView)
  ON_BN_CLICKED(IDC_SAVE, OnSave)
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
// OamView message handlers

OamView::~OamView()
{
  free(data);
  data = NULL;
}

void OamView::paint()
{
  if(oam == NULL || paletteRAM == NULL || vram == NULL)
    return;
  
  render();
  oamView.setSize(w,h);
  oamView.refresh();
}

void OamView::update()
{
  paint();
}



void OamView::setAttributes(u16 a0, u16 a1, u16 a2)
{
  CString buffer;
  
  int y = a0 & 255;
  int rot = a0 & 512;
  int mode = (a0 >> 10) & 3;
  int mosaic = a0 & 4096;
  int color = a0 & 8192;
  int duple = a0 & 1024;
  int shape = (a0 >> 14) & 3;
  int x = a1 & 511;
  int rotParam = (a1 >> 9) & 31;
  int flipH = a1 & 4096;
  int flipV = a1 & 8192;
  int size = (a1 >> 14) & 3;
  int tile = a2 & 1023;
  int prio = (a2 >> 10) & 3;
  int pal = (a2 >> 12) & 15;

  buffer.Format("%d,%d", x,y);
  GetDlgItem(IDC_POS)->SetWindowText(buffer);

  buffer.Format("%d", mode);
  GetDlgItem(IDC_MODE)->SetWindowText(buffer);

  GetDlgItem(IDC_COLORS)->SetWindowText(color ? "256" : "16");

  buffer.Format("%d", pal);
  GetDlgItem(IDC_PALETTE)->SetWindowText(buffer);

  buffer.Format("%d", tile);
  GetDlgItem(IDC_TILE)->SetWindowText(buffer);

  buffer.Format("%d", prio);
  GetDlgItem(IDC_PRIO)->SetWindowText(buffer);

  buffer.Format("%d,%d", w,h);
  GetDlgItem(IDC_SIZE2)->SetWindowText(buffer);

  if(rot) {
    buffer.Format("%d", rotParam);
  } else
    buffer.Empty();
  GetDlgItem(IDC_ROT)->SetWindowText(buffer);

  buffer.Empty();

  if(rot)
    buffer += 'R';
  else buffer += ' ';
  if(!rot) {
    if(flipH)
      buffer += 'H';
    else
      buffer += ' ';
    if(flipV)
      buffer += 'V';
    else
      buffer += ' ';
  } else {
    buffer += ' ';
    buffer += ' ';
  }
  if(mosaic)
    buffer += 'M';
  else
    buffer += ' ';
  if(duple)
    buffer += 'D';
  else
    buffer += ' ';
  
  GetDlgItem(IDC_FLAGS)->SetWindowText(buffer);
}

void OamView::render()
{
  int m=0;
  if(oam == NULL || paletteRAM == NULL || vram == NULL)
    return;
  
  u16 *sprites = &((u16 *)oam)[4*number];
  u16 *spritePalette = &((u16 *)paletteRAM)[0x100];
  u8 *bmp = data;
  
  u16 a0 = *sprites++;
  u16 a1 = *sprites++;
  u16 a2 = *sprites++;
  
  int sizeY = 8;
  int sizeX = 8;
  
  switch(((a0 >>12) & 0x0c)|(a1>>14)) {
  case 0:
    break;
  case 1:
    sizeX = sizeY = 16;
    break;
  case 2:
    sizeX = sizeY = 32;
    break;
  case 3:
    sizeX = sizeY = 64;
    break;
  case 4:
    sizeX = 16;
    break;
  case 5:
    sizeX = 32;
    break;
  case 6:
    sizeX = 32;
    sizeY = 16;
    break;
  case 7:
    sizeX = 64;
    sizeY = 32;
    break;
  case 8:
    sizeY = 16;
    break;
  case 9:
    sizeY = 32;
    break;
  case 10:
    sizeX = 16;
    sizeY = 32;
    break;
  case 11:
    sizeX = 32;
    sizeY = 64;
    break;
  default:
    return;
  }

  w = sizeX;
  h = sizeY;

  setAttributes(a0,a1,a2);
  
  int sy = (a0 & 255);
  
  if(a0 & 0x2000) {
    int c = (a2 & 0x3FF);
    //          if((DISPCNT & 7) > 2 && (c < 512))
    //            return;
    int inc = 32;
    if(DISPCNT & 0x40)
      inc = sizeX >> 2;
    else
      c &= 0x3FE;
    
    for(int y = 0; y < sizeY; y++) {
      for(int x = 0; x < sizeX; x++) {
        u32 color = vram[0x10000 + (((c + (y>>3) * inc)*
                                     32 + (y & 7) * 8 + (x >> 3) * 64 +
                                     (x & 7))&0x7FFF)];
        color = spritePalette[color];
        *bmp++ = ((color >> 10) & 0x1f) << 3;
        *bmp++ = ((color >> 5) & 0x1f) << 3;
        *bmp++ = (color & 0x1f) << 3;
      }
    }
  } else {
    int c = (a2 & 0x3FF);
    //      if((DISPCNT & 7) > 2 && (c < 512))
    //          continue;
    
    int inc = 32;
    if(DISPCNT & 0x40)
      inc = sizeX >> 3;
    int palette = (a2 >> 8) & 0xF0;
    for(int y = 0; y < sizeY; y++) {
      for(int x = 0; x < sizeX; x++) {
        u32 color = vram[0x10000 + (((c + (y>>3) * inc)*
                                     32 + (y & 7) * 4 + (x >> 3) * 32 +
                                     ((x & 7)>>1))&0x7FFF)];
        if(x & 1)
          color >>= 4;
        else
          color &= 0x0F;
        
        color = spritePalette[palette+color];
        *bmp++ = ((color >> 10) & 0x1f) << 3;
        *bmp++ = ((color >> 5) & 0x1f) << 3;
        *bmp++ = (color & 0x1f) << 3;            
      }
    }
  }
}

void OamView::saveBMP(const char *name)
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



void OamView::savePNG(const char *name)
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

void OamView::OnSave() 
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

BOOL OamView::OnInitDialog() 
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
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\OamView",
            NULL);
  m_sprite.SetWindowText("0");

  updateScrollInfo();

  m_stretch = regQueryDwordValue("oamViewStretch", 0);
  if(m_stretch)
    oamView.setStretch(true);
  UpdateData(FALSE);
  
  paint();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void OamView::OnStretch() 
{
  oamView.setStretch(!oamView.getStretch());
  paint();
  regSetDwordValue("oamViewStretch", oamView.getStretch());  
}


void OamView::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }  
}

void OamView::OnChangeSprite() 
{
  CString buffer;
  m_sprite.GetWindowText(buffer);
  int n = atoi(buffer);
  if(n < 0 || n > 127) {
    buffer.Format("%d", number);
    m_sprite.SetWindowText(buffer);
    return;
  }
  number = n;
  paint();
  updateScrollInfo();
}

void OamView::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}

LRESULT OamView::OnMapInfo(WPARAM wParam, LPARAM lParam)
{
  u8 *colors = (u8 *)lParam;
  oamZoom.setColors(colors);
  
  return TRUE;
}

LRESULT OamView::OnColInfo(WPARAM wParam, LPARAM lParam)
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

void OamView::updateScrollInfo()
{
  SCROLLINFO si;
  ZeroMemory(&si, sizeof(si));
  si.cbSize = sizeof(si);
  si.fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL | SIF_POS;
  si.nMin = 0;
  si.nMax = 127;
  si.nPage = 1;
  si.nPos = number;
  GetDlgItem(IDC_SCROLLBAR)->SetScrollInfo(SB_CTL,
                                           &si,
                                           TRUE);    
}

void OamView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
  switch(nSBCode) {
  case SB_BOTTOM:
    number = 127;
    break;
  case SB_LINEDOWN:
    number++;
    if(number > 127)
      number = 127;
    break;
  case SB_LINEUP:
    number--;
    if(number < 0)
      number = 0;
    break;
  case SB_PAGEDOWN:
    number += 16;
    if(number > 127)
      number = 127;
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
    if(number > 127)
      number = 127;
    break;
  }

  updateScrollInfo();
  
  CString buffer;
  buffer.Format("%d", number);
  m_sprite.SetWindowText(buffer);
  paint();
}

void OamView::PostNcDestroy() 
{
  delete this;
}
