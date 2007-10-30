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

// PaletteViewControl.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "PaletteViewControl.h"

#include "../Util.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool PaletteViewControl::isRegistered = false;

/////////////////////////////////////////////////////////////////////////////
// PaletteViewControl

PaletteViewControl::PaletteViewControl()
{
  memset(&bmpInfo.bmiHeader, 0, sizeof(bmpInfo.bmiHeader));
  
  bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
  bmpInfo.bmiHeader.biWidth = 256;
  bmpInfo.bmiHeader.biHeight = -256;
  bmpInfo.bmiHeader.biPlanes = 1;
  bmpInfo.bmiHeader.biBitCount = 24;
  bmpInfo.bmiHeader.biCompression = BI_RGB;
  data = (u8 *)malloc(3 * 256 * 256);

  w = 256;
  h = 256;

  colors = 256;

  paletteAddress = 0;
  
  ZeroMemory(palette, 512);

  selected = -1;
  registerClass();
}

PaletteViewControl::~PaletteViewControl()
{
  if(data)
    free(data);
}


BEGIN_MESSAGE_MAP(PaletteViewControl, CWnd)
  //{{AFX_MSG_MAP(PaletteViewControl)
  ON_WM_LBUTTONDOWN()
  ON_WM_ERASEBKGND()
  ON_WM_PAINT()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()


  /////////////////////////////////////////////////////////////////////////////
// PaletteViewControl message handlers

void PaletteViewControl::init(int c, int w, int h)
{
  this->w = w;
  this->h = h;
  this->colors = c;

  bmpInfo.bmiHeader.biWidth = w;
  bmpInfo.bmiHeader.biHeight = -h;  
}


bool PaletteViewControl::saveAdobe(const char *name)
{
  FILE *f = fopen(name, "wb");

  if(!f)
    return false;

  for(int i = 0; i < colors; i++) {
    u16 c = palette[i];
    int r = (c & 0x1f) << 3;
    int g = (c & 0x3e0) >> 2;
    int b = (c & 0x7c00) >> 7;

    u8 data[3] = { r, g, b };
    fwrite(data, 1, 3, f);
  }
  if(colors < 256) {
    for(int i = colors; i < 256; i++) {
      u8 data[3] = { 0, 0, 0 };
      fwrite(data, 1, 3, f);
    }
  }
  fclose(f);

  return true;
}


bool PaletteViewControl::saveMSPAL(const char *name)
{
  FILE *f = fopen(name, "wb");

  if(!f)
    return false;

  u8 data[4] = { 'R', 'I', 'F', 'F' };

  fwrite(data, 1, 4, f);
  utilPutDword(data, 256 * 4 + 16);
  fwrite(data, 1, 4, f);
  u8 data3[4] = { 'P', 'A', 'L', ' ' };
  fwrite(data3, 1, 4, f);
  u8 data4[4] = { 'd', 'a', 't', 'a' };
  fwrite(data4, 1, 4, f);
  utilPutDword(data, 256*4+4);
  fwrite(data, 1, 4, f);
  utilPutWord(&data[0], 0x0300);
  utilPutWord(&data[2], 256); // causes problems if not 16 or 256
  fwrite(data, 1, 4, f);
  
  for(int i = 0; i < colors; i++) {
    u16 c = palette[i];
    int r = (c & 0x1f) << 3;
    int g = (c & 0x3e0) >> 2;
    int b = (c & 0x7c00) >> 7;

    u8 data7[4] = { r, g, b, 0 };
    fwrite(data7, 1, 4, f);
  }
  if(colors < 256) {
    for(int i = colors; i < 256; i++) {
      u8 data7[4] = { 0, 0, 0, 0 };
      fwrite(data7, 1, 4, f);
    }
  }
  fclose(f);

  return true;
}


bool PaletteViewControl::saveJASCPAL(const char *name)
{
  FILE *f = fopen(name, "wb");

  if(!f)
    return false;

  fprintf(f, "JASC-PAL\r\n0100\r\n256\r\n");
  
  for(int i = 0; i < colors; i++) {
    u16 c = palette[i];
    int r = (c & 0x1f) << 3;
    int g = (c & 0x3e0) >> 2;
    int b = (c & 0x7c00) >> 7;

    fprintf(f, "%d %d %d\r\n", r, g, b);
  }
  if(colors < 256) {
    for(int i = colors; i < 256; i++)
      fprintf(f, "0 0 0\r\n");
  }
  fclose(f);

  return true;  
}

void PaletteViewControl::setPaletteAddress(int address)
{
  paletteAddress = address;
}


void PaletteViewControl::setSelected(int s)
{
  selected = s;
  InvalidateRect(NULL, FALSE);
}


void PaletteViewControl::render(u16 color, int x, int y)
{
  u8 *start = data + y*16*w*3 + x*16*3;
  int skip = w*3-16*3;

  int r = (color & 0x1f) << 3;
  int g = (color & 0x3e0) >> 2;
  int b = (color & 0x7c00) >> 7;

  for(int i = 0; i < 16; i++) {
    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;

    *start++ = b;
    *start++ = g;
    *start++ = r;
    
    start += skip;
  }
}

void PaletteViewControl::refresh()
{
  updatePalette();
  int sw = w/16;
  int sh = h/16;
  for(int i = 0; i < colors; i++) {
    render(palette[i], i & (sw-1), i/sw);
  }
  InvalidateRect(NULL, FALSE);
}

void PaletteViewControl::OnLButtonDown(UINT nFlags, CPoint point) 
{
  int x = point.x;
  int y = point.y;
  RECT rect;
  GetClientRect(&rect);
  int h = rect.bottom - rect.top;
  int w = rect.right - rect.left;
  int sw = (this->w/16);
  int sh = (this->h/16);
  int mult = w / sw;
  int multY = h / sh;

  setSelected(x/mult + (y/multY)*sw);
  
  GetParent()->SendMessage(WM_PALINFO,
                           palette[x/mult+(y/multY)*sw],
                           paletteAddress+(x/mult+(y/multY)*sw));
}

BOOL PaletteViewControl::OnEraseBkgnd(CDC* pDC) 
{
  return TRUE;
}


void PaletteViewControl::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
  
  RECT rect;
  GetClientRect(&rect);
  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top;
  
  CDC memDC;
  memDC.CreateCompatibleDC(&dc);
  CBitmap bitmap, *pOldBitmap;
  bitmap.CreateCompatibleBitmap(&dc, w, h);
  pOldBitmap = memDC.SelectObject(&bitmap);
  
  StretchDIBits(memDC.GetSafeHdc(),
                0,
                0,
                w,
                h,
                0,
                0,
                this->w,
                this->h,
                data,
                &bmpInfo,
                DIB_RGB_COLORS,
                SRCCOPY);
  int sw = this->w / 16;
  int sh = this->h / 16;
  int mult  = w / sw;
  int multY = h / sh;
  CPen pen;
  pen.CreatePen(PS_SOLID, 1, RGB(192,192,192));
  CPen *old = memDC.SelectObject(&pen);
  int i;
  for(i = 1; i < sh; i++) {
    memDC.MoveTo(0, i * multY);
    memDC.LineTo(w, i * multY);
  }
  for(i = 1; i < sw; i++) {
    memDC.MoveTo(i * mult, 0);
    memDC.LineTo(i * mult, h);
  }
  memDC.DrawEdge(&rect, EDGE_SUNKEN, BF_RECT);
  memDC.SelectObject(old);
  pen.DeleteObject();

  if(selected != -1) {
    pen.CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    old = memDC.SelectObject(&pen);

    int startX = (selected & (sw-1))*mult+1;
    int startY = (selected / sw)*multY+1;
    int endX = startX + mult-2;
    int endY = startY + multY-2;
    
    memDC.MoveTo(startX, startY);
    memDC.LineTo(endX, startY);
    memDC.LineTo(endX, endY);
    memDC.LineTo(startX, endY);
    memDC.LineTo(startX, startY-1);

    memDC.SelectObject(old);
    pen.DeleteObject();
  }
  
  dc.BitBlt(0,0,w,h,
            &memDC,0,0,SRCCOPY);

  memDC.SelectObject(pOldBitmap);
  bitmap.DeleteObject();
  memDC.DeleteDC();
}

void PaletteViewControl::registerClass()
{
  if(!isRegistered) {
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
    wc.lpfnWndProc = (WNDPROC)::DefWindowProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH )GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "VbaPaletteViewControl";
    AfxRegisterClass(&wc);
    isRegistered = true;
  }
}
