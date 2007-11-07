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

// GBMapView.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "FileDlg.h"
#include "GBMapView.h"
#include "Reg.h"
#include "WinResUtil.h"

#include "../System.h"
#include "../NLS.h"
#include "../Util.h"
#include "../gb/gbGlobals.h"

extern "C" {
#include <png.h>
}

extern u8 gbInvertTab[256];

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// GBMapView dialog


GBMapView::GBMapView(CWnd* pParent /*=NULL*/)
  : ResizeDlg(GBMapView::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBMapView)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  autoUpdate = false;
  
  memset(&bmpInfo.bmiHeader, 0, sizeof(bmpInfo.bmiHeader));
  
  bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
  bmpInfo.bmiHeader.biWidth = 1024;
  bmpInfo.bmiHeader.biHeight = -1024;
  bmpInfo.bmiHeader.biPlanes = 1;
  bmpInfo.bmiHeader.biBitCount = 24;
  bmpInfo.bmiHeader.biCompression = BI_RGB;
  data = (u8 *)calloc(1, 3 * 1024 * 1024);

  mapView.setData(data);
  mapView.setBmpInfo(&bmpInfo);
  
  bg = 0;
  bank = 0;
}


void GBMapView::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBMapView)
  // NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_MAP_VIEW, mapView);
  DDX_Control(pDX, IDC_MAP_VIEW_ZOOM, mapViewZoom);
  DDX_Control(pDX, IDC_COLOR, color);
}


BEGIN_MESSAGE_MAP(GBMapView, CDialog)
  //{{AFX_MSG_MAP(GBMapView)
  ON_BN_CLICKED(IDC_SAVE, OnSave)
  ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
  ON_BN_CLICKED(IDC_BG0, OnBg0)
  ON_BN_CLICKED(IDC_BG1, OnBg1)
  ON_BN_CLICKED(IDC_BANK_0, OnBank0)
  ON_BN_CLICKED(IDC_BANK_1, OnBank1)
  ON_BN_CLICKED(IDC_STRETCH, OnStretch)
  ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
  ON_BN_CLICKED(IDC_CLOSE, OnClose)
  //}}AFX_MSG_MAP
  ON_MESSAGE(WM_MAPINFO, OnMapInfo)
  ON_MESSAGE(WM_COLINFO, OnColInfo)
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GBMapView message handlers

GBMapView::~GBMapView()
{
  free(data);
  data = NULL;
}

void GBMapView::saveBMP(const char *name)
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

void GBMapView::savePNG(const char *name)
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

void GBMapView::OnSave() 
{
  CString filename;

  if(theApp.captureFormat == 0)
    filename = "map.png";
  else
    filename = "map.bmp";

  LPCTSTR exts[] = {".png", ".bmp" };
  CString title = winResLoadString(IDS_SELECT_CAPTURE_NAME);
  CString filter = theApp.winLoadFilter(IDS_FILTER_PNG);

  FileDlg dlg(this,
              filename,
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

  if(dlg.getFilterIndex() == 2)
    saveBMP(dlg.GetPathName());
  else
    savePNG(dlg.GetPathName());
}

void GBMapView::render()
{
  u8 * bank0;
  u8 * bank1;
  if(gbCgbMode) {
    bank0 = &gbVram[0x0000];
    bank1 = &gbVram[0x2000];
  } else {
    bank0 = &gbMemory[0x8000];
    bank1 = NULL;
  }

  int tile_map_address = 0x1800;
  if(bg == 1)
    tile_map_address = 0x1c00;

  int tile_pattern = 0x0000;
  if(bank == 1)
    tile_pattern = 0x0800;
  
  w = 256;
  h = 256;
  
  int tile = 0;
  for(int y = 0; y < 32; y++) {
    for(int x = 0; x < 32; x++) {
      u8 *bmp = &data[y * 8 * 32 * 24 + x*24];      
      u8 attrs = 0;
      if(bank1 != NULL)
        attrs = bank1[tile_map_address];
      u8 tile = bank0[tile_map_address];
      tile_map_address++;

      if(bank == 1) {
        if(tile < 128) tile += 128;
        else tile -= 128;
      }
      for(int j = 0; j < 8; j++) {
        int tile_pattern_address = attrs & 0x40 ?
          tile_pattern + tile*16 + (7-j)*2:
          tile_pattern + tile*16+j*2;
        
        u8 tile_a = 0;
        u8 tile_b = 0;
        
        if(attrs & 0x08) {
          tile_a = bank1[tile_pattern_address++];
          tile_b = bank1[tile_pattern_address];
        } else {
          tile_a = bank0[tile_pattern_address++];
          tile_b = bank0[tile_pattern_address];
        }
        
        if(attrs & 0x20) {
          tile_a = gbInvertTab[tile_a];
          tile_b = gbInvertTab[tile_b];
        }
        
        u8 mask = 0x80;
        
        while(mask > 0) {
          u8 c = (tile_a & mask) ? 1 : 0;
          c += (tile_b & mask) ? 2 : 0;
          
          if(gbCgbMode)
            c = c + (attrs & 7)*4;
          
          u16 color = gbPalette[c];
          
          *bmp++ = ((color >> 10) & 0x1f) << 3;
          *bmp++ = ((color >> 5) & 0x1f) << 3;
          *bmp++ = (color & 0x1f) << 3;
          
          mask >>= 1;
        }
        bmp += 31*24;
      }
    }
  }
}

void GBMapView::paint()
{
  if(gbRom == NULL)
    return;
  render();
  
  SIZE s;
  if(mapView.getStretch()) {
    mapView.setSize(w, h);
    s.cx = s.cy = 1;
    mapView.SetScrollSizes(MM_TEXT, s);
  } else {
    mapView.setSize(w, h);
    s.cx = w;
    s.cy = h;
    mapView.SetScrollSizes(MM_TEXT, s);
  }

  mapView.refresh();
}

void GBMapView::OnRefresh() 
{
  paint();
}

void GBMapView::update()
{
  paint();
}

BOOL GBMapView::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  DIALOG_SIZER_START( sz )
    DIALOG_SIZER_ENTRY( IDC_MAP_VIEW, DS_SizeX | DS_SizeY )
    DIALOG_SIZER_ENTRY( IDC_REFRESH, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_CLOSE, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_SAVE,  DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_COLOR, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_R, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_G, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_B, DS_MoveY)    
    DIALOG_SIZER_END()
    SetData(sz,
            TRUE,
            HKEY_CURRENT_USER,
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\GBMapView",
            NULL);

  int s = regQueryDwordValue("mapViewStretch", 0);
  if(s)
    mapView.setStretch(true);
  ((CButton *)GetDlgItem(IDC_STRETCH))->SetCheck(s);

  UINT id = IDC_BANK_0;
  if(bank == 1)
    id = IDC_BANK_1;
  CheckRadioButton(IDC_BANK_0, IDC_BANK_1, id);
  id = IDC_BG0;
  if(bg == 1)
    id = IDC_BG1;
  CheckRadioButton(IDC_BG0, IDC_BG1, id);
  paint();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GBMapView::OnBg0() 
{
  bg = 0;
  paint();
}

void GBMapView::OnBg1() 
{
  bg = 1;
  paint();
}

void GBMapView::OnBank0() 
{
  bank = 0;
  paint();
}

void GBMapView::OnBank1() 
{
  bank = 1;
  paint();
}

void GBMapView::OnStretch() 
{
  mapView.setStretch(!mapView.getStretch());
  paint();
  regSetDwordValue("mapViewStretch", mapView.getStretch());  
}

void GBMapView::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }
}

void GBMapView::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}

u32 GBMapView::GetClickAddress(int x, int y)
{
  u32 base = 0x9800;
  if(bg == 1)
    base = 0x9c00;

  return base + (y >> 3)*32 + (x >> 3);
}

LRESULT GBMapView::OnMapInfo(WPARAM wParam, LPARAM lParam)
{
  u8 *colors = (u8 *)lParam;
  mapViewZoom.setColors(colors);

  int x = (int)(wParam & 0xffff);
  int y = (int)(wParam >> 16);
  
  CString buffer;
  buffer.Format("(%d,%d)", x, y);
  GetDlgItem(IDC_XY)->SetWindowText(buffer);

  u32 address = GetClickAddress(x,y);
  buffer.Format("0x%08X", address);
  GetDlgItem(IDC_ADDRESS)->SetWindowText(buffer);

  u8 attrs = 0;

  u8 tile = gbMemoryMap[9][address & 0xfff];
  if(gbCgbMode) {
    attrs = gbVram[0x2000 + address - 0x8000];
    tile = gbVram[address & 0x1fff];
  }

  if(bank == 1) {
    if(tile > 128) tile -= 128;
    else tile += 128;
  }
  
  buffer.Format("%d", tile);
  GetDlgItem(IDC_TILE_NUM)->SetWindowText(buffer);
  
  buffer.Empty();
  buffer += attrs & 0x20 ? 'H' : '-';
  buffer += attrs & 0x40 ? 'V' : '-';
  GetDlgItem(IDC_FLIP)->SetWindowText(buffer);
  
  if(gbCgbMode) {
    buffer.Format("%d", (attrs & 7));
  } else
    buffer = "---";
  GetDlgItem(IDC_PALETTE_NUM)->SetWindowText(buffer);
  
  buffer.Empty();
  if(gbCgbMode)
    buffer += attrs & 0x80 ? 'P' : '-';
  else
    buffer += '-';
  GetDlgItem(IDC_PRIORITY)->SetWindowText(buffer);

  return TRUE;
}

LRESULT GBMapView::OnColInfo(WPARAM wParam, LPARAM)
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

void GBMapView::PostNcDestroy() 
{
  delete this;
  CDialog::PostNcDestroy();
}
