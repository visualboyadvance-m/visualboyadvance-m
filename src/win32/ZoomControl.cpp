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

// ZoomControl.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "ZoomControl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool ZoomControl::isRegistered = false;

/////////////////////////////////////////////////////////////////////////////
// ZoomControl

ZoomControl::ZoomControl()
{
  ZeroMemory(colors, 3*64);
  selected = -1;
  registerClass();
}

ZoomControl::~ZoomControl()
{
}


BEGIN_MESSAGE_MAP(ZoomControl, CWnd)
  //{{AFX_MSG_MAP(ZoomControl)
  ON_WM_PAINT()
  ON_WM_LBUTTONDOWN()
  ON_WM_ERASEBKGND()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()


  /////////////////////////////////////////////////////////////////////////////
// ZoomControl message handlers

void ZoomControl::registerClass()
{
  if(!isRegistered) {
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
    wc.lpfnWndProc = (WNDPROC)::DefWindowProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH )GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "VbaZoomControl";
    AfxRegisterClass(&wc);
    isRegistered = true;    
  }
}

void ZoomControl::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
  
  RECT rect;
  GetClientRect(&rect);

  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top;
  
  CDC memDC ;
  memDC.CreateCompatibleDC(&dc);
  CBitmap bitmap, *pOldBitmap;
  bitmap.CreateCompatibleBitmap(&dc, w, h);

  pOldBitmap = memDC.SelectObject(&bitmap);
  
  int multX = w / 8;
  int multY = h / 8;

  int i;
  for(i = 0; i < 64; i++) {
    CBrush b;
    b.CreateSolidBrush(RGB(colors[i*3+2], colors[i*3+1], colors[i*3]));

    RECT r;
    int x = i & 7;
    int y = i / 8;
    r.top = y*multY;
    r.left = x*multX;
    r.bottom = r.top + multY;
    r.right = r.left + multX;
    memDC.FillRect(&r, &b);
    b.DeleteObject();
  }

  CPen pen;
  pen.CreatePen(PS_SOLID, 1, RGB(192,192,192));
  CPen *old = (CPen *)memDC.SelectObject(&pen);

  for(i = 0; i < 8; i++) {
    memDC.MoveTo(0, i * multY);
    memDC.LineTo(w, i * multY);
    memDC.MoveTo(i * multX, 0);
    memDC.LineTo(i * multX, h);
  }

  if(selected != -1) {
    CPen pen2;
    pen2.CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    CPen *old2 = (CPen*)memDC.SelectObject(&pen2);

    int startX = (selected & 7)*multX+1;
    int startY = (selected / 8)*multY+1;
    int endX = startX + multX-2;
    int endY = startY + multY-2;
    
    memDC.MoveTo(startX, startY);
    memDC.LineTo(endX, startY);
    memDC.LineTo(endX, endY);
    memDC.LineTo(startX, endY);
    memDC.LineTo(startX, startY-1);
    memDC.SelectObject(old2);
    pen2.DeleteObject();
  }
  memDC.SelectObject(old);
  pen.DeleteObject();

  dc.BitBlt(0,0,w,h,
            &memDC,0,0, SRCCOPY);

  memDC.SelectObject(pOldBitmap);
  bitmap.DeleteObject();
  memDC.DeleteDC();
}

void ZoomControl::OnLButtonDown(UINT nFlags, CPoint point) 
{
  RECT rect;
  GetClientRect(&rect);
  
  int height = rect.bottom - rect.top;
  int width = rect.right - rect.left;

  int multX = width / 8;
  int multY = height / 8;

  selected = point.x / multX + 8 * (point.y / multY);
  
  int c = point.x / multX + 8 * (point.y/multY);
  u16 color = colors[c*3] << 7 |
    colors[c*3+1] << 2 |
    (colors[c*3+2] >> 3);
  
  GetParent()->PostMessage(WM_COLINFO,
                           color,
                           0);

  Invalidate();
}

BOOL ZoomControl::OnEraseBkgnd(CDC* pDC) 
{
  return TRUE;
}

void ZoomControl::setColors(const u8 *c)
{
  memcpy(colors, c, 3*64);
  selected = -1;
  Invalidate();
}
