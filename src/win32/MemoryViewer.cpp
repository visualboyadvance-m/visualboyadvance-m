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

// MemoryViewer.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "MemoryViewer.h"

#include "../System.h"
extern int emulating;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// MemoryViewer

bool MemoryViewer::isRegistered = false;

MemoryViewer::MemoryViewer()
{
  address = 0;
  addressSize = 0;
  dataSize = 0;
  editAddress = 0;
  editNibble = 0;
  displayedLines = 0;
  hasCaret = false;
  maxNibble = 0;
  font = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
  fontSize.cx = fontSize.cy = 0;
  beginAscii = 0;
  beginHex = 0;
  dlg = NULL;
  registerClass();
}

MemoryViewer::~MemoryViewer()
{
}


BEGIN_MESSAGE_MAP(MemoryViewer, CWnd)
  //{{AFX_MSG_MAP(MemoryViewer)
  ON_WM_ERASEBKGND()
  ON_WM_PAINT()
  ON_WM_VSCROLL()
  ON_WM_GETDLGCODE()
  ON_WM_LBUTTONDOWN()
  ON_WM_SETFOCUS()
  ON_WM_KILLFOCUS()
  ON_WM_KEYDOWN()
  //}}AFX_MSG_MAP
  ON_MESSAGE(WM_CHAR, OnWMChar)
  END_MESSAGE_MAP()


  /////////////////////////////////////////////////////////////////////////////
// MemoryViewer message handlers

void MemoryViewer::setDialog(IMemoryViewerDlg *d)
{
  dlg = d;
}


void MemoryViewer::setAddress(u32 a)
{
  address = a;
  if(displayedLines) {
    if(addressSize) {
      u16 addr = address;
      if((u16)(addr+(displayedLines<<4)) < addr) {
        address = 0xffff - (displayedLines<<4) + 1;
      }      
    } else {
      if((address+(displayedLines<<4)) < address) {
        address = 0xffffffff - (displayedLines<<4) + 1;
      }
    }
  }
  if(addressSize)
    address &= 0xffff;
  setCaretPos();
  InvalidateRect(NULL, TRUE);
}


void MemoryViewer::setSize(int s)
{
  dataSize = s;
  if(s == 0)
    maxNibble = 1;
  else if(s == 1)
    maxNibble = 3;
  else
    maxNibble = 7;
  
  InvalidateRect(NULL, TRUE);
}

BOOL MemoryViewer::OnEraseBkgnd(CDC* pDC) 
{
  return TRUE;
}

void MemoryViewer::updateScrollInfo(int lines)
{
  int page = lines * 16;
  SCROLLINFO si;
  ZeroMemory(&si, sizeof(si));
  si.cbSize = sizeof(si);
  si.fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL | SIF_POS;
  si.nMin = 0;
  if(addressSize) {
    si.nMax = 0x10000/page;
    si.nPage = 1;
  } else {
    si.nMax = 0xa000000 / page;
    si.nPage = page;
  }

  si.nPos = address / page;
  SetScrollInfo(SB_VERT,
                &si,
                TRUE);
}

void MemoryViewer::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
  
  RECT rect;
  GetClientRect(&rect);
  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top - 6;
  
  CDC memDC;
  memDC.CreateCompatibleDC(&dc);
  CBitmap bitmap, *pOldBitmap;
  bitmap.CreateCompatibleBitmap(&dc, w, rect.bottom - rect.top);
  pOldBitmap = memDC.SelectObject(&bitmap);
  
  memDC.FillRect(&rect, CBrush::FromHandle((HBRUSH)GetStockObject(WHITE_BRUSH)));
  memDC.DrawEdge(&rect, EDGE_ETCHED, BF_RECT);
  
  CFont *oldFont = memDC.SelectObject(CFont::FromHandle(font));
  
  fontSize = memDC.GetTextExtent("0", 1);
  
  int lines = h / fontSize.cy;
  
  displayedLines = lines;
  
  updateScrollInfo(lines);
  
  u32 addr = address;
  
  memDC.SetTextColor(RGB(0,0,0));
  
  u8 data[32];
  
  RECT r;
  r.top = 3;
  r.left = 3;
  r.bottom = r.top+fontSize.cy;
  r.right = rect.right-3;
  
  int line = 0;
  
  for(int i = 0; i < lines; i++) {
    CString buffer;
    if(addressSize)
      buffer.Format("%04X", addr);
    else
      buffer.Format("%08X", addr);
    memDC.DrawText(buffer, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
    r.left += 10*fontSize.cx;
    beginHex = r.left;
    readData(addr, 16, data);
    
    int j;
    
    if(dataSize == 0) {
      for(j = 0; j < 16; j++) {
        buffer.Format("%02X", data[j]);
        memDC.DrawText(buffer, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
        r.left += 3*fontSize.cx;
      }
    }
    if(dataSize == 1) {
      for(j = 0; j < 16; j += 2) {
        buffer.Format("%04X", data[j] | data[j+1]<<8);
        memDC.DrawText(buffer, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
        r.left += 5*fontSize.cx;
      }      
    }
    if(dataSize == 2) {
      for(j = 0; j < 16; j += 4) {
        buffer.Format("%08X", data[j] | data[j+1]<<8 |
                      data[j+2] << 16 | data[j+3] << 24);
        memDC.DrawText(buffer, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
        r.left += 9*fontSize.cx;
      }            
    }
    
    line = r.left;
    
    r.left += fontSize.cx;
    beginAscii = r.left;
    buffer.Empty();
    for(j = 0; j < 16; j++) {
      char c = data[j];
      if(c >= 32 && c <= 127) {
        buffer += c;
      } else
        buffer += '.';
    }
    
    memDC.DrawText(buffer, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
    addr += 16;
    if(addressSize)
      addr &= 0xffff;
    r.top += fontSize.cy;
    r.bottom += fontSize.cy;
    r.left = 3;
  }
  CPen pen;
  pen.CreatePen(PS_SOLID, 1, RGB(0,0,0));
  CPen *old = memDC.SelectObject(&pen);
  
  memDC.MoveTo(3+fontSize.cx*9, 3);
  memDC.LineTo(3+fontSize.cx*9, 3+displayedLines*fontSize.cy);
  
  memDC.MoveTo(line, 3);
  memDC.LineTo(line, 3+displayedLines*fontSize.cy);
  
  memDC.SelectObject(old);
  pen.DeleteObject();
  
  memDC.SelectObject(oldFont);
  
  dc.BitBlt(0, 0, w, rect.bottom - rect.top, &memDC, 0, 0, SRCCOPY);
  
  memDC.SelectObject(pOldBitmap);
  memDC.DeleteDC();
  bitmap.DeleteObject();
}

void MemoryViewer::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
  int address = this->address;
  switch(nSBCode) {
  case SB_BOTTOM:
    address = 0xffffff00;
    break;
  case SB_LINEDOWN:
    address += 0x10;
    break;
  case SB_LINEUP:
    address -= 0x10;
    break;
  case SB_PAGEDOWN:
    address += (displayedLines<<4);
    break;
  case SB_PAGEUP:
    address -= (displayedLines<<4);
    break;
  case SB_TOP:
    address = 0;
    break;
  case SB_THUMBTRACK:
    {
      int page = displayedLines * 16;      
      SCROLLINFO si;
      ZeroMemory(&si, sizeof(si));
      si.cbSize = sizeof(si);
      si.fMask = SIF_TRACKPOS;
      GetScrollInfo(SB_VERT, &si);
      address = page * si.nTrackPos;
    }
    break;
  }
  setAddress(address);
}

UINT MemoryViewer::OnGetDlgCode() 
{
  return DLGC_WANTALLKEYS;
}

void MemoryViewer::createEditCaret(int w, int h)
{
  if(!hasCaret || caretWidth != w || caretHeight != h) {
    hasCaret = true;
    caretWidth = w;
    caretHeight = h;
    ::CreateCaret(m_hWnd, (HBITMAP)0, w, h);
  }
}


void MemoryViewer::destroyEditCaret()
{
  hasCaret = false;
  DestroyCaret();
}

void MemoryViewer::setCaretPos()
{
  if(GetFocus() != this) {
    destroyEditCaret();
    return;
  }

  if(dlg)
    dlg->setCurrentAddress(editAddress);
  
  if(editAddress < address || editAddress > (address -1 + (displayedLines<<4))) {
    destroyEditCaret();
    return;
  }

  int subAddress = (editAddress - address);

  int x = 3+10*fontSize.cx+editNibble*fontSize.cx;
  int y = 3+fontSize.cy*((editAddress-address)>>4);

  if(editAscii) {
    x = beginAscii + fontSize.cx*(subAddress&15);
  } else {
    switch(dataSize) {
    case 0:
      x += 3*fontSize.cx*(subAddress & 15);
      break;
    case 1:
      x += 5*fontSize.cx*((subAddress>>1) & 7);
      break;
    case 2:
      x += 9*fontSize.cx*((subAddress>>2) & 3);
      break;
    }
  }

  RECT r;
  GetClientRect(&r);
  r.right -= 3;
  if(x >= r.right) {
    destroyEditCaret();
    return;
  }
  int w = fontSize.cx;
  if((x+fontSize.cx)>=r.right)
    w = r.right - x;
  createEditCaret(w, fontSize.cy);
  ::SetCaretPos(x, y);
  ShowCaret();
}

void MemoryViewer::OnLButtonDown(UINT nFlags, CPoint point) 
{
  int x = point.x;
  int y = point.y;
  int line = (y-3)/fontSize.cy;
  int beforeAscii = beginHex;
  int inc = 1;
  int sub = 3*fontSize.cx;
  switch(dataSize) {
  case 0:
    beforeAscii += 47*fontSize.cx;
    break;
  case 1:
    beforeAscii += 39*fontSize.cx;
    inc = 2;
    sub = 5*fontSize.cx;
    break;
  case 2:
    beforeAscii += 35*fontSize.cx;
    inc = 4;
    sub = 9*fontSize.cx;
    break;
  }
  
  editAddress = address + (line<<4);
  if(x >= beginHex && x < beforeAscii) {
    x -= beginHex;
    editNibble = 0;
    while(x > 0) {
      x -= sub;
      if(x >= 0)
        editAddress += inc;
      else {
        editNibble = (x + sub)/fontSize.cx;
      }
    }
    editAscii = false;
  } else if(x >= beginAscii) {
    int afterAscii = beginAscii+16*fontSize.cx;
    if(x >= afterAscii) 
      x = afterAscii-1;
    editAddress += (x-beginAscii)/fontSize.cx;
    editNibble = 0;
    editAscii = true;
  } else {
    return;
  }

  if(editNibble > maxNibble)
    editNibble = maxNibble;
  SetFocus();
  setCaretPos();
}

void MemoryViewer::OnSetFocus(CWnd* pOldWnd) 
{
  setCaretPos();
  InvalidateRect(NULL, TRUE);
}

void MemoryViewer::OnKillFocus(CWnd* pNewWnd) 
{
  destroyEditCaret();
  InvalidateRect(NULL, TRUE);
}

void MemoryViewer::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  bool isShift = (GetKeyState(VK_SHIFT) & 0x80000000) == 0x80000000;

  switch(nChar) {
  case VK_RIGHT:
    if(editAscii)
      moveAddress(1,0);
    else if(isShift)
      moveAddress((maxNibble+1)>>1,0);
    else
      moveAddress(0, 1);
    break;
  case VK_LEFT:
    if(editAscii)
      moveAddress(-1, 0);
    else if(isShift)
      moveAddress(-((maxNibble+1)>>1),0);
    else
      moveAddress(0, -1);
    break;  
  case VK_DOWN:
    moveAddress(16, 0);
    break;
  case VK_UP:
    moveAddress(-16, 0);
    break;
  case VK_TAB:
    GetNextDlgTabItem(GetParent(), isShift)->SetFocus();
    break;
  }
}

void MemoryViewer::moveAddress(s32 offset, int nibbleOff)
{
  if(offset == 0) {
    if(nibbleOff == -1) {
      editNibble--;
      if(editNibble == -1) {
        editAddress -= (maxNibble + 1) >> 1;
        editNibble = maxNibble;
      }
      if(address == 0 && (editAddress >= (u32)(displayedLines<<4))) {
        editAddress = 0;
        editNibble = 0;
        beep();
      }
      if(editAddress < address)
        setAddress(address - 16);
    } else {
      editNibble++;
      if(editNibble > maxNibble) {
        editNibble = 0;
        editAddress += (maxNibble + 1) >> 1;
      }
      if(editAddress < address) {
        editAddress -= (maxNibble + 1) >> 1;
        editNibble = maxNibble;
        beep();
      }
      if(editAddress >= (address+(displayedLines<<4)))
        setAddress(address+16);
    }
  } else {
    editAddress += offset;
    if(offset < 0 && editAddress > (address-1+(displayedLines<<4))) {
      editAddress -= offset;
      beep();
      return;
    }
    if(offset > 0 && (editAddress < address)) {
      editAddress -= offset;
      beep();
      return;
    }
    if(editAddress < address) {
      if(offset & 15)
        setAddress((address+offset-16) & ~15);
      else
        setAddress(address+offset);
    } else if(editAddress > (address - 1 + (displayedLines<<4))) {
      if(offset & 15)
        setAddress((address+offset+16) & ~15);
      else
        setAddress(address+offset);
    }
  }

  setCaretPos();
}

LRESULT MemoryViewer::OnWMChar(WPARAM wParam, LPARAM LPARAM)
{
  if(OnEditInput(wParam))
    return 0;
  return 1;
}

bool MemoryViewer::OnEditInput(UINT c)
{
  if(c > 255 || !emulating) {
    beep();
    return false;
  }

  if(!editAscii)
    c = tolower(c);

  u32 value = 256;

  if(c >= 'a' && c <= 'f')
    value = 10 + (c - 'a');
  else if(c >= '0' && c <= '9')
    value = (c - '0');
  if(editAscii) {
    editData(editAddress, 8, 0, c);
    moveAddress(1, 0);
    InvalidateRect(NULL, TRUE);
  } else {
    if(value != 256) {
      value <<= 4*(maxNibble-editNibble);
      u32 mask = ~(15 << 4*(maxNibble - editNibble));
      switch(dataSize) {
      case 0:
        editData(editAddress, 8, mask, value);
        break;
      case 1:
        editData(editAddress, 16, mask, value);
        break;
      case 2:
        editData(editAddress, 32, mask, value);
        break;
      }
      moveAddress(0, 1);
      InvalidateRect(NULL, TRUE);
    }
  }
  return true;
}

void MemoryViewer::beep()
{
  MessageBeep((UINT)-1);
}

void MemoryViewer::registerClass()
{
  if(!isRegistered) {
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_PARENTDC | CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
    wc.lpfnWndProc = (WNDPROC)::DefWindowProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH )GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "VbaMemoryViewer";
    AfxRegisterClass(&wc);
    isRegistered = true;
  }
}

void MemoryViewer::setAddressSize(int s)
{
  addressSize = s;
}


u32 MemoryViewer::getCurrentAddress()
{
  return editAddress;
}

int MemoryViewer::getSize()
{
  return dataSize;
}
