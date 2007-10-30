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

// ColorControl.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "ColorControl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool ColorControl::isRegistered = false;

/////////////////////////////////////////////////////////////////////////////
// ColorControl

ColorControl::ColorControl()
{
  color = 0;
  registerClass();
}

ColorControl::~ColorControl()
{
}


BEGIN_MESSAGE_MAP(ColorControl, CWnd)
  //{{AFX_MSG_MAP(ColorControl)
  ON_WM_PAINT()
  ON_WM_ERASEBKGND()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()


  /////////////////////////////////////////////////////////////////////////////
// ColorControl message handlers

void ColorControl::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
}

BOOL ColorControl::OnEraseBkgnd(CDC* pDC) 
{
  int r = (color & 0x1f) << 3;
  int g = (color & 0x3e0) >> 2;
  int b = (color & 0x7c00) >> 7;

  CBrush br;
  br.CreateSolidBrush(RGB(r,g,b));

  RECT rect;
  GetClientRect(&rect);
  pDC->FillRect(&rect,&br);
  pDC->DrawEdge(&rect, EDGE_SUNKEN, BF_RECT);
  br.DeleteObject();
  return TRUE;
}

void ColorControl::setColor(u16 c)
{
  color = c;
  Invalidate();
}

void ColorControl::registerClass()
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
    wc.lpszClassName = "VbaColorControl";
    AfxRegisterClass(&wc);
    isRegistered = true;
  }
}
