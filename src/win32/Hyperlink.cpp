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

// Hyperlink.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "Hyperlink.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Hyperlink

Hyperlink::Hyperlink()
{
  m_over = false;
}

Hyperlink::~Hyperlink()
{
  m_underlineFont.DeleteObject();
}


BEGIN_MESSAGE_MAP(Hyperlink, CStatic)
  //{{AFX_MSG_MAP(Hyperlink)
  ON_WM_CTLCOLOR_REFLECT()
  ON_WM_ERASEBKGND()
  ON_WM_MOUSEMOVE()
  //}}AFX_MSG_MAP
  ON_CONTROL_REFLECT(STN_CLICKED, OnClicked)
END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// Hyperlink message handlers

void Hyperlink::PreSubclassWindow() 
{
  DWORD dwStyle = GetStyle();
  ::SetWindowLong(GetSafeHwnd(), GWL_STYLE, dwStyle | SS_NOTIFY);

  // 32649 is the hand cursor
  m_cursor = LoadCursor(NULL, MAKEINTRESOURCE(32649));

  CFont *font = GetFont();

  LOGFONT lg;
  font->GetLogFont(&lg);

  lg.lfUnderline = TRUE;
  
  m_underlineFont.CreateFontIndirect(&lg);
  SetFont(&m_underlineFont);
	
  CStatic::PreSubclassWindow();
}

void Hyperlink::OnClicked()
{
  CString url;
  GetWindowText(url);
  ::ShellExecute(0, _T("open"), url, 
                 0, 0, SW_SHOWNORMAL);
}

HBRUSH Hyperlink::CtlColor(CDC* pDC, UINT nCtlColor) 
{
  pDC->SetTextColor(RGB(0,0,240));
	
  return (HBRUSH)GetStockObject(NULL_BRUSH);
}

BOOL Hyperlink::OnEraseBkgnd(CDC* pDC) 
{
  CRect rect;
  GetClientRect(rect);
  pDC->FillSolidRect(rect, ::GetSysColor(COLOR_3DFACE));

  return TRUE;
}

void Hyperlink::OnMouseMove(UINT nFlags, CPoint point) 
{
  if(!m_over) {
    m_over = true;
    SetCapture();
    ::SetCursor(m_cursor);
  } else {
    CRect r;
    GetClientRect(&r);

    if(!r.PtInRect(point)) {
      m_over = false;
      ReleaseCapture();
    }
  }
}
