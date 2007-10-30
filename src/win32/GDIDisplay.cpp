// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team

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
#include <stdio.h>

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"
#include "..\gb\gbGlobals.h"
#include "../Text.h"
#include "../Util.h"
#include "UniVideoModeDlg.h"

#include "VBA.h"
#include "MainWnd.h"
#include "Reg.h"
#include "..\..\res\resource.h"

#include "../gbafilter.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern void winlog(const char *,...);
extern int Init_2xSaI(u32);
extern int systemSpeed;
extern int winVideoModeSelect(CWnd *, GUID **);


class GDIDisplay : public IDisplay
{
private:
	u8 *filterData;
	u8 info[sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD)];
	int SelectedFreq, SelectedAdapter;
public:
	GDIDisplay();
	virtual ~GDIDisplay();

	virtual bool changeRenderSize(int w, int h);
	virtual bool initialize();
	virtual void cleanup();
	virtual void render();
	virtual void checkFullScreen();
	virtual void renderMenu();
	virtual void clear();
	virtual DISPLAY_TYPE getType() { return GDI; };
	virtual void setOption(const char *, int) {}
	virtual int selectFullScreenMode(GUID **);
	virtual int selectFullScreenMode2();
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
	filterData = NULL;
}

GDIDisplay::~GDIDisplay()
{
  cleanup();
}

void GDIDisplay::cleanup()
{
	if(filterData)
	{
		delete [] filterData;
		filterData = NULL;
	}
}

bool GDIDisplay::initialize()
{
	switch(theApp.cartridgeType)
	{
	case 0:
		theApp.sizeX = 240;
		theApp.sizeY = 160;
		break;
	case 1:
		if(gbBorderOn)
		{
			theApp.sizeX = 256;
			theApp.sizeY = 224;
		}
		else
		{
			theApp.sizeX = 160;
			theApp.sizeY = 144;
		}
		break;
	}

	switch(theApp.videoOption)
	{
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
	case VIDEO_1024x768:
	case VIDEO_1280x1024:
	case VIDEO_OTHER:
			float scaleX = ((float)theApp.fsWidth / theApp.sizeX);
			float scaleY = ((float)theApp.fsHeight / theApp.sizeY);
			float min = scaleX < scaleY ? scaleX : scaleY;
			if(theApp.fsMaxScale)
				min = min > theApp.fsMaxScale ? theApp.fsMaxScale : min;
			if(theApp.fullScreenStretch)
			{
				theApp.surfaceSizeX = theApp.fsWidth;
				theApp.surfaceSizeY = theApp.fsHeight;
			}
			else
			{
				theApp.surfaceSizeX = (int)(theApp.sizeX * min);
				theApp.surfaceSizeY = (int)(theApp.sizeY * min);
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

  // Enumerate available display modes
  theApp.mode320Available = false;
  theApp.mode640Available = false;
  theApp.mode800Available = false;
  theApp.mode1024Available = false;
  theApp.mode1280Available = false;
  DISPLAY_DEVICE dev;
  dev.cb = sizeof(DISPLAY_DEVICE);
  EnumDisplayDevices(NULL, 0, &dev, 0);
  DEVMODE mode;
  for (DWORD iMode = 0;
	  TRUE == EnumDisplaySettings(dev.DeviceName, iMode, &mode);
	  iMode++)
  {
	  if ( (mode.dmBitsPerPel == 16) &&
		  (mode.dmPelsWidth==320) && (mode.dmPelsHeight==240))
		  theApp.mode320Available = true;

	  if ( (mode.dmBitsPerPel == 16) &&
		  (mode.dmPelsWidth==640) && (mode.dmPelsHeight==480))
		  theApp.mode640Available = true;

	  if ( (mode.dmBitsPerPel == 16) &&
		  (mode.dmPelsWidth==800) && (mode.dmPelsHeight==600))
		  theApp.mode800Available = true;

	  if ( (mode.dmBitsPerPel == 32) &&
		  (mode.dmPelsWidth==1024) && (mode.dmPelsHeight==768))
		  theApp.mode1024Available = true;

	  if ( (mode.dmBitsPerPel == 32) &&
		  (mode.dmPelsWidth==1280) && (mode.dmPelsHeight==1024))
		  theApp.mode1280Available = true;
  }

  // Go into fullscreen
  if(theApp.videoOption >= VIDEO_320x240)
  {
	  mode.dmBitsPerPel	= theApp.fsColorDepth;
	  mode.dmPelsWidth	= theApp.fsWidth;
	  mode.dmPelsHeight	= theApp.fsHeight;
	  mode.dmDisplayFrequency = theApp.fsFrequency;
	  mode.dmFields		= DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
	  DISPLAY_DEVICE dd;
	  dd.cb = sizeof(DISPLAY_DEVICE);
	  EnumDisplayDevices(NULL, theApp.fsAdapter, &dd, 0);
	  ChangeDisplaySettingsEx(dd.DeviceName, &mode, NULL, CDS_FULLSCREEN, NULL);
  }
  else // Reset from fullscreen
  {
	  ChangeDisplaySettings(NULL, 0);
  }
  

  // Initialize 2xSaI
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


  // Setup system color depth
  theApp.fsColorDepth = systemColorDepth;
  if(systemColorDepth == 24)
	  theApp.filterFunction = NULL;
#ifdef MMX
  if(!theApp.disableMMX)
	  cpu_mmx = theApp.detectMMX();
  else
	  cpu_mmx = 0;
#endif

  utilUpdateSystemColorMaps(theApp.filterLCD );
  theApp.updateFilter();
  theApp.updateIFB();

  pWnd->DragAcceptFiles(TRUE);

  return TRUE;  
}


void GDIDisplay::clear()
{
	CDC *dc = theApp.m_pMainWnd->GetDC();
	CBrush brush(RGB(0x00, 0x00, 0x00));
	dc->FillRect(CRect(0, 0, theApp.fsWidth, theApp.fsHeight), &brush);
	theApp.m_pMainWnd->ReleaseDC(dc);
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
	unsigned int pitch = theApp.filterWidth * (systemColorDepth / 8) + 4;

	BITMAPINFO *bi = (BITMAPINFO *)info;
	bi->bmiHeader.biWidth = theApp.filterWidth + 1;
	bi->bmiHeader.biHeight = -theApp.filterHeight;

	if(theApp.filterFunction)
	{
		bi->bmiHeader.biWidth = theApp.rect.right;
		bi->bmiHeader.biHeight = -(int)theApp.rect.bottom;
		
		(*theApp.filterFunction)(
			pix + pitch,
			pitch,
			(u8*)theApp.delta,
			(u8*)filterData,
			theApp.rect.right * (systemColorDepth / 8),
			theApp.filterWidth,
			theApp.filterHeight);
	}

	POINT p1, p2;
	p1.x = theApp.dest.left;
	p1.y = theApp.dest.top;
	p2.x = theApp.dest.right;
	p2.y = theApp.dest.bottom;
	theApp.m_pMainWnd->ScreenToClient(&p1);
	theApp.m_pMainWnd->ScreenToClient(&p2);
	
	CDC *dc = theApp.m_pMainWnd->GetDC();

	// Draw bitmap to device
	StretchDIBits(
		dc->GetSafeHdc(),
		p1.x,  p1.y,
		p2.x - p1.x,
		p2.y - p1.y,
		theApp.rect.left, theApp.rect.top,
		theApp.rect.right - theApp.rect.left,
		theApp.rect.bottom - theApp.rect.top,
		theApp.filterFunction ? filterData : pix + pitch,
		bi,
		DIB_RGB_COLORS,
		SRCCOPY);

	// Draw frame counter
	if (theApp.showSpeed && (theApp.videoOption >= VIDEO_320x240))
	{
		CString speedText;
		if (theApp.showSpeed == 1)
			speedText.AppendFormat("%3d%%", systemSpeed);
		else
			speedText.AppendFormat("%3d%%(%d, %d fps)",
				systemSpeed, systemFrameSkip, theApp.showRenderedFrames);

		dc->SetTextColor(RGB(0xFF, 0x3F, 0x3F));
		if (theApp.showSpeedTransparent)
			dc->SetBkMode(TRANSPARENT);
		else
			dc->SetBkMode(OPAQUE);
		dc->SetBkColor(RGB(0xFF, 0xFF, 0xFF));
		dc->TextOut(p1.x + 16, p1.y + 16, speedText);
	}

	// Draw screen message
	if (theApp.screenMessage)
	{
		if ( ((GetTickCount() - theApp.screenMessageTime) < 3000) && !theApp.disableStatusMessage )
		{
			dc->SetTextColor(RGB(0x3F, 0x3F, 0xFF));
			if (theApp.showSpeedTransparent)
				dc->SetBkMode(TRANSPARENT);
			else
				dc->SetBkMode(OPAQUE);
			dc->SetBkColor(RGB(0xFF, 0xFF, 0xFF));
			dc->TextOut(p1.x + 16, p2.y - 16, theApp.screenMessageBuffer);
		}
		else
			theApp.screenMessage = false;
	}

	theApp.m_pMainWnd->ReleaseDC(dc);
}

int GDIDisplay::selectFullScreenMode(GUID **pGUID)
{
	int w, h, b;
	UniVideoModeDlg dlg(0, &w, &h, &b, &SelectedFreq, &SelectedAdapter);

	if (0 == dlg.DoModal())
	{
		return (b<<24) + (w<<12) + h;
	// Bits<<24  |  Width<<12  |  Height
	}
	else
	{
		return -1;
	}
}

int GDIDisplay::selectFullScreenMode2()
{
	return (SelectedAdapter<<16) + SelectedFreq;
}

bool GDIDisplay::changeRenderSize(int w, int h)
{
	if (filterData)
	{
		delete [] filterData;
		filterData = NULL;
	}
	filterData = new u8[w*h*(systemColorDepth>>3)];
	return true;
}


IDisplay *newGDIDisplay()
{
  return new GDIDisplay();
}