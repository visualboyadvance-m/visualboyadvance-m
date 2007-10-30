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

// TileView.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "FileDlg.h"
#include "Reg.h"
#include "TileView.h"
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
// TileView dialog


TileView::TileView(CWnd* pParent /*=NULL*/)
  : ResizeDlg(TileView::IDD, pParent)
{
  //{{AFX_DATA_INIT(TileView)
  m_colors = -1;
  m_charBase = -1;
  m_stretch = FALSE;
  //}}AFX_DATA_INIT
  autoUpdate = false;
  
  memset(&bmpInfo, 0, sizeof(bmpInfo));

  bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
  bmpInfo.bmiHeader.biWidth = 32*8;
  bmpInfo.bmiHeader.biHeight = 32*8;
  bmpInfo.bmiHeader.biPlanes = 1;
  bmpInfo.bmiHeader.biBitCount = 24;
  bmpInfo.bmiHeader.biCompression = BI_RGB;
  data = (u8 *)calloc(1, 3 * 32*32 * 64);

  tileView.setData(data);
  tileView.setBmpInfo(&bmpInfo);

  charBase = 0;
  is256Colors = 0;
  palette = 0;
  w = h = 0;
}

TileView::~TileView()
{
  free(data);
  data = NULL;
}

void TileView::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(TileView)
  DDX_Control(pDX, IDC_PALETTE_SLIDER, m_slider);
  DDX_Radio(pDX, IDC_16_COLORS, m_colors);
  DDX_Radio(pDX, IDC_CHARBASE_0, m_charBase);
  DDX_Check(pDX, IDC_STRETCH, m_stretch);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_TILE_VIEW, tileView);
  DDX_Control(pDX, IDC_MAP_VIEW_ZOOM, zoom);
  DDX_Control(pDX, IDC_COLOR, color);
}


BEGIN_MESSAGE_MAP(TileView, CDialog)
  //{{AFX_MSG_MAP(TileView)
  ON_BN_CLICKED(IDC_SAVE, OnSave)
  ON_BN_CLICKED(IDC_CLOSE, OnClose)
  ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
  ON_BN_CLICKED(IDC_16_COLORS, On16Colors)
  ON_BN_CLICKED(IDC_256_COLORS, On256Colors)
  ON_BN_CLICKED(IDC_CHARBASE_0, OnCharbase0)
  ON_BN_CLICKED(IDC_CHARBASE_1, OnCharbase1)
  ON_BN_CLICKED(IDC_CHARBASE_2, OnCharbase2)
  ON_BN_CLICKED(IDC_CHARBASE_3, OnCharbase3)
  ON_BN_CLICKED(IDC_CHARBASE_4, OnCharbase4)
  ON_BN_CLICKED(IDC_STRETCH, OnStretch)
  ON_WM_HSCROLL()
  //}}AFX_MSG_MAP
  ON_MESSAGE(WM_MAPINFO, OnMapInfo)
  ON_MESSAGE(WM_COLINFO, OnColInfo)
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// TileView message handlers

void TileView::saveBMP(const char *name)
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


void TileView::savePNG(const char *name)
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


void TileView::OnSave() 
{
  CString captureBuffer;

  if(theApp.captureFormat == 0)
    captureBuffer = "tiles.png";
  else
    captureBuffer = "tiles.bmp";

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

void TileView::renderTile256(int tile, int x, int y, u8 *charBase, u16 *palette)
{
  u8 *bmp = &data[24*x + 8*32*24*y];

  for(int j = 0; j < 8; j++) {
    for(int i = 0; i < 8; i++) {
      u8 c = charBase[tile*64 + j * 8 + i];

      u16 color = palette[c];

      *bmp++ = ((color >> 10) & 0x1f) << 3;
      *bmp++ = ((color >> 5) & 0x1f) << 3;
      *bmp++ = (color & 0x1f) << 3;      

    }
    bmp += 31*24; // advance line
  }
}

void TileView::renderTile16(int tile, int x, int y, u8 *charBase, u16 *palette)
{
  u8 *bmp = &data[24*x + 8*32*24*y];

  int pal = this->palette;

  if(this->charBase == 4)
    pal += 16;

  for(int j = 0; j < 8; j++) {
    for(int i = 0; i < 8; i++) {
      u8 c = charBase[tile*32 + j * 4 + (i>>1)];

      if(i & 1)
        c = c>>4;
      else
        c = c & 15;

      u16 color = palette[pal*16+c];

      *bmp++ = ((color >> 10) & 0x1f) << 3;
      *bmp++ = ((color >> 5) & 0x1f) << 3;
      *bmp++ = (color & 0x1f) << 3;
    }
    bmp += 31*24; // advance line
  }
}


void TileView::render()
{
  u16 *palette = (u16 *)paletteRAM;
  u8 *charBase = &vram[this->charBase * 0x4000];
 
  int maxY;

  if(is256Colors) {
    int tile = 0;
    maxY = 16;
    for(int y = 0; y < maxY; y++) {
      for(int x = 0; x < 32; x++) {
        if(this->charBase == 4)
          renderTile256(tile, x, y, charBase, &palette[256]);
        else
          renderTile256(tile, x, y, charBase, palette);
        tile++;
      }
    }
    tileView.setSize(32*8, maxY*8);
    w = 32*8;
    h = maxY*8;
    SIZE s;
    s.cx = 32*8;
    s.cy = maxY*8;
    if(tileView.getStretch()) {
      s.cx = s.cy = 1;
    }
    tileView.SetScrollSizes(MM_TEXT,s);
  } else {
    int tile = 0;
    maxY = 32;
    if(this->charBase == 3)
      maxY = 16;
    for(int y = 0; y < maxY; y++) {
      for(int x = 0; x < 32; x++) {
        renderTile16(tile, x, y, charBase, palette);    
        tile++;
      }
    }
    tileView.setSize(32*8, maxY*8);
    w = 32*8;
    h = maxY*8;
    SIZE s;
    s.cx = 32*8;
    s.cy = maxY*8;
    if(tileView.getStretch()) {
      s.cx = s.cy = 1;
    }
    tileView.SetScrollSizes(MM_TEXT, s);
  }
}

void TileView::update()
{
  paint();
}


BOOL TileView::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  DIALOG_SIZER_START( sz )
    DIALOG_SIZER_ENTRY( IDC_TILE_VIEW, DS_SizeX | DS_SizeY )
    DIALOG_SIZER_ENTRY( IDC_COLOR, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_R, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_G, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_B, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_REFRESH, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_CLOSE, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_SAVE, DS_MoveY)
    DIALOG_SIZER_END()
    SetData(sz,
            TRUE,
            HKEY_CURRENT_USER,
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\TileView",
            NULL);

  m_colors = is256Colors;
  m_charBase = charBase;

  m_slider.SetRange(0, 15);
  m_slider.SetPageSize(4);
  m_slider.SetTicFreq(1);

  paint();

  m_stretch = regQueryDwordValue("tileViewStretch", 0);
  if(m_stretch)
    tileView.setStretch(true);
  UpdateData(FALSE);
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void TileView::OnClose() 
{
  theApp.winRemoveUpdateListener(this);
  
  DestroyWindow();
}


void TileView::OnAutoUpdate() 
{
  autoUpdate = !autoUpdate;
  if(autoUpdate) {
    theApp.winAddUpdateListener(this);
  } else {
    theApp.winRemoveUpdateListener(this);    
  }  
}


void TileView::paint()
{
  if(vram != NULL && paletteRAM != NULL) {
    render();
    tileView.refresh();
  }
}


void TileView::On16Colors() 
{
  is256Colors = 0;
  paint();
}

void TileView::On256Colors() 
{
  is256Colors = 1;
  paint();
}

void TileView::OnCharbase0() 
{
  charBase = 0;
  paint();
}

void TileView::OnCharbase1() 
{
  charBase = 1;
  paint();
}

void TileView::OnCharbase2() 
{
  charBase = 2;
  paint();
}

void TileView::OnCharbase3() 
{
  charBase = 3;
  paint();
}

void TileView::OnCharbase4() 
{
  charBase = 4;
  paint();
}

void TileView::OnStretch() 
{
  tileView.setStretch(!tileView.getStretch());
  paint();
  regSetDwordValue("tileViewStretch", tileView.getStretch());  
}

LRESULT TileView::OnMapInfo(WPARAM wParam, LPARAM lParam)
{
  u8 *colors = (u8 *)lParam;
  zoom.setColors(colors);

  int x = (wParam & 0xFFFF)/8;
  int y = ((wParam >> 16) & 0xFFFF)/8;

  u32 address = 0x6000000 + 0x4000 * charBase;
  int tile = 32 * y + x;
  if(is256Colors)
    tile *= 2;
  address += 32 * tile;

  CString buffer;
  buffer.Format("%d", tile);
  GetDlgItem(IDC_TILE_NUMBER)->SetWindowText(buffer);

  buffer.Format("%08x", address);
  GetDlgItem(IDC_ADDRESS)->SetWindowText(buffer);
  
  return TRUE;
}

LRESULT TileView::OnColInfo(WPARAM wParam, LPARAM)
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

void TileView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
  switch(nSBCode) {
  case TB_THUMBPOSITION:
    palette = nPos;
    break;
  default:
    palette = m_slider.GetPos();
    break;
  }
  paint();
}

void TileView::PostNcDestroy() 
{
  delete this;
}
