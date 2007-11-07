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

// skinButton.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "skinButton.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern bool winAccelGetID(const char *command, WORD& id);

/////////////////////////////////////////////////////////////////////////////
// SkinButton

SkinButton::SkinButton()
{
  normalBmp = NULL;
  downBmp = NULL;
  overBmp = NULL;
  mouseOver = false;
  id = "";
  idCommand = 0;
  region = NULL;
  buttonMask = 0;
  menu = -1;
}

SkinButton::~SkinButton()
{
  DestroyWindow();
  if(normalBmp) {
    DeleteObject(normalBmp);
    normalBmp = NULL;
  }
  if(downBmp) {
    DeleteObject(downBmp);
    downBmp = NULL;
  }
  if(overBmp) {
    DeleteObject(overBmp);
    overBmp = NULL;
  }
  if(region) {
    DeleteObject(region);
    region = NULL;
  }
}


BEGIN_MESSAGE_MAP(SkinButton, CWnd)
  //{{AFX_MSG_MAP(SkinButton)
  ON_WM_ERASEBKGND()
  ON_WM_PAINT()
  ON_WM_KILLFOCUS()
  ON_WM_CAPTURECHANGED()
  ON_WM_CONTEXTMENU()
  //}}AFX_MSG_MAP
  ON_MESSAGE(WM_LBUTTONUP, OnLButtonUpMsg)
  ON_MESSAGE(WM_LBUTTONDOWN, OnLButtonDownMsg)
  ON_MESSAGE(WM_MOUSEMOVE, OnMouseMoveMsg)
  ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeaveMsg)
  END_MESSAGE_MAP()


  /////////////////////////////////////////////////////////////////////////////
// SkinButton message handlers

BOOL SkinButton::OnEraseBkgnd(CDC* pDC) 
{
  return TRUE;
}

void SkinButton::OnPaint() 
{
  PAINTSTRUCT ps;
  HDC hDC = ::BeginPaint(m_hWnd, &ps);
  HDC memDC = ::CreateCompatibleDC(hDC);
  LRESULT state = ::SendMessage(m_hWnd, BM_GETSTATE, 0, 0);
  HBITMAP oldBitmap;
  if(state & BST_PUSHED)
    oldBitmap = (HBITMAP)SelectObject(memDC, downBmp);
  else if(mouseOver && overBmp != NULL)
    oldBitmap = (HBITMAP)SelectObject(memDC, overBmp);
  else
    oldBitmap = (HBITMAP)SelectObject(memDC, normalBmp);
  SelectClipRgn(hDC, region);
  BitBlt(hDC, 0, 0, theApp.rect.right - theApp.rect.left,
         theApp.rect.bottom - theApp.rect.top, memDC, 0, 0, SRCCOPY);
  SelectClipRgn(hDC, NULL);
  SelectObject(memDC, oldBitmap);
  DeleteDC(memDC);

  ::EndPaint(m_hWnd, &ps);
}

LRESULT SkinButton::OnLButtonUpMsg(WPARAM wParam, LPARAM lParam)
{
  POINT pt;
  pt.x = LOWORD(lParam);
  pt.y = HIWORD(lParam);
  RECT r;
  GetClientRect(&r);
  BOOL inside = PtInRect(&r, pt);
  if(region != NULL)
    inside &= PtInRegion(region, pt.x, pt.y);
  if(inside) {
    ReleaseCapture();
    Invalidate();
    HWND hWnd = m_hWnd;
    if(idCommand != 0)
      GetParent()->SendMessage(WM_COMMAND, idCommand, 0);
    else if(buttonMask)
      theApp.skinButtons = 0;
    else if(menu != -1) {
      HMENU m = GetSubMenu(theApp.menu, menu);
      pt.x = r.left;
      pt.y = r.bottom;
      ClientToScreen(&pt);
      theApp.m_pMainWnd->SendMessage(WM_INITMENUPOPUP, (WPARAM)m, menu);
      TrackPopupMenu(m, 0, pt.x, pt.y, 0, *theApp.m_pMainWnd, NULL);
    }

    return ::DefWindowProc(hWnd, WM_LBUTTONUP, wParam, lParam);
  }
  return GetParent()->SendMessage(WM_LBUTTONUP, wParam, lParam);
}

LRESULT SkinButton::OnLButtonDownMsg(WPARAM wParam, LPARAM lParam)
{
  if(idCommand != 0)
    return Default();

  POINT pt;
  pt.x = LOWORD(lParam);
  pt.y = HIWORD(lParam);
  RECT r;
  GetClientRect(&r);
  BOOL inside = PtInRect(&r, pt);
  if(region != NULL)
    inside &= PtInRegion(region, pt.x, pt.y);
  if(inside) {
    if(buttonMask)
      theApp.skinButtons = buttonMask;
    return Default();
  }
  return GetParent()->SendMessage(WM_LBUTTONDOWN, wParam, lParam);
}

LRESULT SkinButton::OnMouseMoveMsg(WPARAM wParam, LPARAM lParam)
{
  if(wParam & MK_LBUTTON && !mouseOver)
    return Default();

  if(GetCapture() != this) {
    SetCapture();
  }
  POINT pt;
  pt.x = LOWORD(lParam);
  pt.y = HIWORD(lParam);
  //  ClientToScreen(getHandle(), &p);
  RECT r;
  GetClientRect(&r);
  BOOL inside = PtInRect(&r, pt);
  if(region != NULL)
    inside &= PtInRegion(region, pt.x, pt.y);

  if(!inside) {
    //  HWND h = WindowFromPoint(p);
    //  if(h != getHandle()) {
    if(mouseOver) {
      mouseOver = false;
      Invalidate();
    }
    if(!(wParam & MK_LBUTTON))
      ReleaseCapture();
  } else {
    if(!mouseOver) {
      mouseOver = true;
      Invalidate();
    }
  }
  return Default();
}

void SkinButton::OnKillFocus(CWnd* pNewWnd) 
{
  mouseOver = false;
  Invalidate();

  CWnd::OnKillFocus(pNewWnd);
}

void SkinButton::OnCaptureChanged(CWnd *pWnd) 
{
  if(mouseOver) {
    ReleaseCapture();
    Invalidate();
  }
  
  CWnd::OnCaptureChanged(pWnd);
}

LRESULT SkinButton::OnMouseLeaveMsg(WPARAM wParam, LPARAM lParam)
{
  if(mouseOver) {
    ReleaseCapture();
    mouseOver = false;
    Invalidate();
  }

  return Default();
}

void SkinButton::OnContextMenu(CWnd* pWnd, CPoint point) 
{
}

void SkinButton::SetNormalBitmap(HBITMAP bmp)
{
  normalBmp = bmp;
}

void SkinButton::SetDownBitmap(HBITMAP bmp)
{
  downBmp = bmp;
}

void SkinButton::SetOverBitmap(HBITMAP bmp)
{
  overBmp = bmp;
}

void SkinButton::SetRect(const RECT& r)
{
  rect = r;
}

void SkinButton::SetId(const char *id)
{
  this->id = id;
  if(!winAccelGetID(id, idCommand)) {
    if(!strcmp(id, "A"))
      buttonMask = 1;
    else if(!strcmp("B", id))
      buttonMask = 2;
    else if(!strcmp("SEL", id))
      buttonMask = 4;
    else if(!strcmp("START", id))
      buttonMask = 8;
    else if(!strcmp("R", id))
      buttonMask = 16;
    else if(!strcmp("L", id))
      buttonMask = 32;
    else if(!strcmp("U", id))
      buttonMask = 64;
    else if(!strcmp("D", id))
      buttonMask = 128;
    else if(!strcmp("BR", id))
      buttonMask = 256;
    else if(!strcmp("BL", id))
      buttonMask = 512;
    else if(!strcmp("SPEED", id))
      buttonMask = 1024;
    else if(!strcmp("CAPTURE", id))
      buttonMask = 2048;
    else if(!strcmp("GS", id))
      buttonMask = 4096;
    else if(!strcmp("UR", id))
      buttonMask = 64+16;
    else if(!strcmp("UL", id))
      buttonMask = 64+32;
    else if(!strcmp("DR", id))
      buttonMask = 128+16;
    else if(!strcmp("DL", id))
      buttonMask = 128+32;
    else if(!strcmp("MENUFILE", id))
      menu = 0;
    else if(!strcmp("MENUOPTIONS", id))
      menu = 1;
    else if(!strcmp("MENUCHEATS", id))
      menu = 2;
    else if(!strcmp("MENUTOOLS", id))
      menu = 3;
    else if(!strcmp("MENUHELP", id))
      menu = 4;
  }
}

void SkinButton::SetRegion(HRGN rgn)
{
  region = rgn;
}

void SkinButton::GetRect(RECT& r)
{
  r = rect;
}

BOOL SkinButton::CreateButton(const char *name, DWORD style, const RECT& r, 
                              CWnd *parent, UINT id)
{
  return CWnd::Create("BUTTON",
                      name,
                      style|WS_CHILDWINDOW,
                      r,
                      parent,
                      id);
}
