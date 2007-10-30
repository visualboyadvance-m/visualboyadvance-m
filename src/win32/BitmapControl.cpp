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

// BitmapControl.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "BitmapControl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool BitmapControl::isRegistered = false;

/////////////////////////////////////////////////////////////////////////////
// BitmapControl

IMPLEMENT_DYNCREATE(BitmapControl, CScrollView)

  BitmapControl::BitmapControl()
{
  w = 0;
  h = 0;
  data = NULL;
  bmpInfo = NULL;
  stretch = false;
  registerClass();
  CSize sizeTotal;
  sizeTotal.cx = sizeTotal.cy = 0;
  SetScrollSizes(MM_TEXT, sizeTotal);
}

BitmapControl::~BitmapControl()
{
}


BEGIN_MESSAGE_MAP(BitmapControl, CScrollView)
  //{{AFX_MSG_MAP(BitmapControl)
  ON_WM_ERASEBKGND()
  ON_WM_SIZE()
  ON_WM_LBUTTONDOWN()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// BitmapControl drawing

void BitmapControl::OnInitialUpdate()
{
  CScrollView::OnInitialUpdate();

  CSize sizeTotal;
  // TODO: calculate the total size of this view
  sizeTotal.cx = sizeTotal.cy = 100;
  SetScrollSizes(MM_TEXT, sizeTotal);
}

void BitmapControl::OnDraw(CDC* dc)
{
  RECT r;
  GetClientRect(&r);
  int w1 = r.right - r.left;
  int h1 = r.bottom - r.top;
  CDC memDC;
  memDC.CreateCompatibleDC(dc);
  if(!stretch) {
    if(w > w1)
      w1 = w;
    if(h > h1)
      h1 = h;
  }
  CBitmap bitmap, *pOldBitmap;
  bitmap.CreateCompatibleBitmap(dc, w1, h1);
  pOldBitmap = memDC.SelectObject(&bitmap);
  if(stretch) {
    bmpInfo->bmiHeader.biWidth = w;
    bmpInfo->bmiHeader.biHeight = -h;
    
    StretchDIBits(memDC.GetSafeHdc(),
                  0,
                  0,
                  w1,
                  h1, 
                  0,
                  0,
                  w,
                  h,
                  data,
                  bmpInfo,
                  DIB_RGB_COLORS,
                  SRCCOPY);
  } else {
    FillOutsideRect(&memDC, CBrush::FromHandle(GetSysColorBrush(COLOR_BTNFACE)));
    
    bmpInfo->bmiHeader.biWidth = w;
    bmpInfo->bmiHeader.biHeight = -h;
    SetDIBitsToDevice(memDC.GetSafeHdc(),
                      0,
                      0,
                      w,
                      h,
                      0,
                      0,
                      0,
                      h,
                      data,
                      bmpInfo,
                      DIB_RGB_COLORS);
  }

  dc->BitBlt(0,0,w1,h1,
             &memDC,0,0,SRCCOPY);
  memDC.SelectObject(pOldBitmap);

  bitmap.DeleteObject();
  memDC.DeleteDC();  
}

/////////////////////////////////////////////////////////////////////////////
// BitmapControl diagnostics

#ifdef _DEBUG
void BitmapControl::AssertValid() const
{
  CScrollView::AssertValid();
}

void BitmapControl::Dump(CDumpContext& dc) const
{
  CScrollView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// BitmapControl message handlers

BOOL BitmapControl::OnEraseBkgnd(CDC* pDC) 
{
  return TRUE;
}

void BitmapControl::OnSize(UINT nType, int cx, int cy) 
{
  if(!stretch)
    CScrollView::OnSize(nType, cx, cy);
}

void BitmapControl::OnLButtonDown(UINT nFlags, CPoint pt) 
{
  if(!data)
    return;
  int x = pt.x;
  int y = pt.y;

  WPARAM point;
  
  if(stretch) {
    RECT rect;
    GetClientRect(&rect);
  
    int height = rect.bottom - rect.top;
    int width = rect.right - rect.left;
  
    int xx = (x * w) / width;
    int yy = (y * h) / height;

    point = xx | (yy<<16);

    int xxx = xx / 8;
    int yyy = yy / 8;

    for(int i = 0; i < 8; i++) {
      memcpy(&colors[i*3*8], &data[xxx * 8 * 3 +
                                   w * yyy * 8 * 3 +
                                   i * w * 3], 8 * 3);
    }
  } else {
    POINT p;
    p.x = GetScrollPos(SB_HORZ);
    p.y = GetScrollPos(SB_VERT);

    p.x += x;
    p.y += y;

    if(p.x >= w ||
       p.y >= h)
      return;

    point = p.x | (p.y<<16);
    
    int xxx = p.x / 8;
    int yyy = p.y / 8;

    for(int i = 0; i < 8; i++) {
      memcpy(&colors[i*3*8], &data[xxx * 8 * 3 +
                                   w * yyy * 8 * 3 +
                                   i * w * 3], 8 * 3);
    }
  }
  
  GetParent()->SendMessage(WM_MAPINFO,
                           point,
                           (LPARAM)colors);
}

void BitmapControl::setBmpInfo(BITMAPINFO *info)
{
  bmpInfo = info;
}

void BitmapControl::setData(u8 *d)
{
  data = d;
}

void BitmapControl::setSize(int w1, int h1)
{
  if(w != w1 || h != h1) {
    w = w1;
    h = h1;
    SIZE s;
    s.cx = w;
    s.cy = h;
    SetScrollSizes(MM_TEXT, s);
  }
}

void BitmapControl::refresh()
{
  Invalidate();
}


void BitmapControl::registerClass()
{
  if(!isRegistered) {
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
    wc.lpfnWndProc = (WNDPROC)::DefWindowProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "VbaBitmapControl";
    AfxRegisterClass(&wc);
    isRegistered = true;
  }
}

void BitmapControl::setStretch(bool b)
{
  if(b != stretch) {
    stretch = b;
    Invalidate();
  }
}

bool BitmapControl::getStretch()
{
  return stretch;
}

void BitmapControl::PostNcDestroy() 
{
}
