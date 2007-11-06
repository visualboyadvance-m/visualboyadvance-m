// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2005-2006 VBA development team

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

#include "stdafx.h"

#include "Display.h"

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../Text.h"
#include "../Util.h"

#include "VBA.h"
#include "MainWnd.h"
#include "Reg.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern void winlog(const char *,...);
extern int Init_2xSaI(u32);
extern int systemSpeed;

class GDIDisplay : public IDisplay {
private:
  u8 *filterData;
  u8 info[sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD)];
  
public:
  GDIDisplay();
  virtual ~GDIDisplay();

  virtual bool initialize();
  virtual void cleanup();
  virtual void render();
  virtual void checkFullScreen();
  virtual void renderMenu();
  virtual void clear();
  virtual DISPLAY_TYPE getType() { return GDI; };
  virtual void setOption(const char *, int) {}
  virtual bool isSkinSupported() { return true; }
  virtual int selectFullScreenMode(GUID **);
};

static int calculateShift(u32 mask)
{
  int m = 0;
  
  while(mask) {
    m++;
    mask >>= 1;
  }

  return m-5;
}

GDIDisplay::GDIDisplay()
{
  filterData = (u8 *)malloc(4*4*256*240);
}

GDIDisplay::~GDIDisplay()
{
  cleanup();
}

void GDIDisplay::cleanup()
{
  if(filterData) {
    free(filterData);
    filterData = NULL;
  }
}

bool GDIDisplay::initialize()
{
  theApp.sizeX = 240;
  theApp.sizeY = 160;
  switch(theApp.videoOption) {
  case VIDEO_1X:
    theApp.surfaceSizeX = theApp.sizeX;
    theApp.surfaceSizeY = theApp.sizeY;
    break;
  case VIDEO_2X:
    theApp.surfaceSizeX = theApp.sizeX * 2;
    theApp.surfaceSizeY = theApp.sizeY * 2;
    break;
  case VIDEO_3X:
    theApp.surfaceSizeX = theApp.sizeX * 3;
    theApp.surfaceSizeY = theApp.sizeY * 3;
    break;
  case VIDEO_4X:
    theApp.surfaceSizeX = theApp.sizeX * 4;
    theApp.surfaceSizeY = theApp.sizeY * 4;
    break;
  case VIDEO_320x240:
  case VIDEO_640x480:
  case VIDEO_800x600:
  case VIDEO_OTHER:
    {
      int scaleX = (theApp.fsWidth / theApp.sizeX);
      int scaleY = (theApp.fsHeight / theApp.sizeY);
      int min = scaleX < scaleY ? scaleX : scaleY;
      if(theApp.fsMaxScale)
        min = min > theApp.fsMaxScale ? theApp.fsMaxScale : min;
      theApp.surfaceSizeX = theApp.sizeX * min;
      theApp.surfaceSizeY = theApp.sizeY * min;
      if(theApp.fullScreenStretch) {
        theApp.surfaceSizeX = theApp.fsWidth;
        theApp.surfaceSizeY = theApp.fsHeight;
      }
    }
    break;
  }
  
  theApp.rect.left = 0;
  theApp.rect.top = 0;
  theApp.rect.right = theApp.sizeX;
  theApp.rect.bottom = theApp.sizeY;

  theApp.dest.left = 0;
  theApp.dest.top = 0;
  theApp.dest.right = theApp.surfaceSizeX;
  theApp.dest.bottom = theApp.surfaceSizeY;

  DWORD style = WS_POPUP | WS_VISIBLE;
  DWORD styleEx = 0;
  
  if(theApp.videoOption <= VIDEO_4X)
    style |= WS_OVERLAPPEDWINDOW;
  else
    styleEx = 0;

  if(theApp.videoOption <= VIDEO_4X)
    AdjustWindowRectEx(&theApp.dest, style, TRUE, styleEx);
  else
    AdjustWindowRectEx(&theApp.dest, style, FALSE, styleEx);    

  int winSizeX = theApp.dest.right-theApp.dest.left;
  int winSizeY = theApp.dest.bottom-theApp.dest.top;

  if(theApp.videoOption > VIDEO_4X) {
    winSizeX = theApp.fsWidth;
    winSizeY = theApp.fsHeight;
  }
  
  int x = 0;
  int y = 0;

  if(theApp.videoOption <= VIDEO_4X) {
    x = theApp.windowPositionX;
    y = theApp.windowPositionY;
  }
  
  // Create a window
  MainWnd *pWnd = new MainWnd;
  theApp.m_pMainWnd = pWnd;

  pWnd->CreateEx(styleEx,
                 theApp.wndClass,
                 "VisualBoyAdvance",
                 style,
                 x,y,winSizeX,winSizeY,
                 NULL,
                 0);
  
  if (!(HWND)*pWnd) {
    winlog("Error creating Window %08x\n", GetLastError());
    return FALSE;
  }
  
  theApp.updateMenuBar();
  
  theApp.adjustDestRect();
  
  theApp.mode320Available = false;
  theApp.mode640Available = false;
  theApp.mode800Available = false;
  
  HDC dc = GetDC(NULL);
  HBITMAP hbm = CreateCompatibleBitmap(dc, 1, 1);
  BITMAPINFO *bi = (BITMAPINFO *)info;
  ZeroMemory(bi, sizeof(info));
  bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  GetDIBits(dc, hbm, 0, 1, NULL, (LPBITMAPINFO)info, DIB_RGB_COLORS);  
  GetDIBits(dc, hbm, 0, 1, NULL, (LPBITMAPINFO)info, DIB_RGB_COLORS);
  DeleteObject(hbm);
  ReleaseDC(NULL, dc);

  if(bi->bmiHeader.biCompression == BI_BITFIELDS) {
    systemColorDepth = bi->bmiHeader.biBitCount;
    if(systemColorDepth == 15)
      systemColorDepth = 16;
    systemRedShift = calculateShift(*((DWORD *)&bi->bmiColors[0]));
    systemGreenShift = calculateShift(*((DWORD *)&bi->bmiColors[1]));
    systemBlueShift = calculateShift(*((DWORD *)&bi->bmiColors[2]));
    if(systemColorDepth == 16) {
      if(systemGreenShift == 6) {
        Init_2xSaI(565);
      } else {
        Init_2xSaI(555);
      }
    } else if(systemColorDepth == 32)
      Init_2xSaI(32);
  } else {
    systemColorDepth = 32;
    systemRedShift = 19;
    systemGreenShift = 11;
    systemBlueShift = 3;

    Init_2xSaI(32);    
  }
  theApp.fsColorDepth = systemColorDepth;
  if(systemColorDepth == 24)
    theApp.filterFunction = NULL;
#ifdef MMX
  if(!theApp.disableMMX)
    cpu_mmx = theApp.detectMMX();
  else
    cpu_mmx = 0;
#endif
  
  utilUpdateSystemColorMaps();
  theApp.updateFilter();
  theApp.updateIFB();
  
  pWnd->DragAcceptFiles(TRUE);
  
  return TRUE;  
}

void GDIDisplay::clear()
{
}

void GDIDisplay::renderMenu()
{
  checkFullScreen();
  theApp.m_pMainWnd->DrawMenuBar();
}

void GDIDisplay::checkFullScreen()
{
}

void GDIDisplay::render()
{ 
  BITMAPINFO *bi = (BITMAPINFO *)info;
  bi->bmiHeader.biWidth = theApp.filterWidth+1;
  bi->bmiHeader.biHeight = -theApp.filterHeight;

  int pitch = theApp.filterWidth * 2 + 4;
  if(systemColorDepth == 24)
    pitch = theApp.filterWidth * 3;
  else if(systemColorDepth == 32)
    pitch = theApp.filterWidth * 4 + 4;
  
  if(theApp.filterFunction) {
    bi->bmiHeader.biWidth = theApp.filterWidth * 2;
    bi->bmiHeader.biHeight = -theApp.filterHeight * 2;
    
    if(systemColorDepth == 16)
      (*theApp.filterFunction)(pix+pitch,
                               pitch,
                               (u8*)theApp.delta,
                               (u8*)filterData,
                               theApp.filterWidth*2*2,
                               theApp.filterWidth,
                               theApp.filterHeight);
    else
      (*theApp.filterFunction)(pix+pitch,
                               pitch,
                               (u8*)theApp.delta,
                               (u8*)filterData,
                               theApp.filterWidth*4*2,
                               theApp.filterWidth,
                               theApp.filterHeight);
  }

  if(theApp.showSpeed && (theApp.videoOption > VIDEO_4X || theApp.skin != NULL)) {
    char buffer[30];
    if(theApp.showSpeed == 1)
      sprintf(buffer, "%3d%%", systemSpeed);
    else
      sprintf(buffer, "%3d%%(%d, %d fps)", systemSpeed,
              systemFrameSkip,
              theApp.showRenderedFrames);

    if(theApp.filterFunction) {
      int p = theApp.filterWidth * 4;
      if(systemColorDepth == 24)
        p = theApp.filterWidth * 6;
      else if(systemColorDepth == 32)
        p = theApp.filterWidth * 8;
      if(theApp.showSpeedTransparent)
        drawTextTransp((u8*)filterData,
                       p,
                       10,
                       theApp.filterHeight*2-10,
                       buffer);
      else
        drawText((u8*)filterData,
                 p,
                 10,
                 theApp.filterHeight*2-10,
                 buffer);      
    } else {
      if(theApp.showSpeedTransparent)
        drawTextTransp((u8*)pix,
                       pitch,
                       10,
                       theApp.filterHeight-10,
                       buffer);
      else
        drawText((u8*)pix,
                 pitch,
                 10,
                 theApp.filterHeight-10,
                 buffer);
    }
  }

  POINT p;
  p.x = theApp.dest.left;
  p.y = theApp.dest.top;
  CWnd *pWnd = theApp.m_pMainWnd;
  pWnd->ScreenToClient(&p);
  POINT p2;
  p2.x = theApp.dest.right;
  p2.y = theApp.dest.bottom;
  pWnd->ScreenToClient(&p2);
  
  CDC *dc = pWnd->GetDC();

  StretchDIBits((HDC)*dc,
                p.x,
                p.y,
                p2.x - p.x,
                p2.y - p.y,
                0,
                0,
                theApp.rect.right,
                theApp.rect.bottom,
                theApp.filterFunction ? filterData : pix+pitch,
                bi,
                DIB_RGB_COLORS,
                SRCCOPY);

  if(theApp.screenMessage) {
    if(((GetTickCount() - theApp.screenMessageTime) < 3000) &&
       !theApp.disableStatusMessage) {
      dc->SetTextColor(RGB(255,0,0));
      dc->SetBkMode(TRANSPARENT);
      dc->TextOut(p.x+10, p2.y - 20, theApp.screenMessageBuffer);
    } else {
      theApp.screenMessage = false;
    }
  }
  
  pWnd->ReleaseDC(dc);
}

int GDIDisplay::selectFullScreenMode(GUID **)
{
  HWND wnd = GetDesktopWindow();
  RECT r;
  GetWindowRect(wnd, &r);
  int w = (r.right - r.left) & 4095;
  int h = (r.bottom - r.top) & 4095;
  HDC dc = GetDC(wnd);
  int c = GetDeviceCaps(dc, BITSPIXEL);
  ReleaseDC(wnd, dc);

  return (c << 24) | (w << 12) | h;
}

IDisplay *newGDIDisplay()
{
  return new GDIDisplay();
}

