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

#include "stdafx.h"

#define DIRECTDRAW_VERSION 0x0700
#include <ddraw.h>

#include "../System.h"
#include "../gb/gbGlobals.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../Text.h"
#include "../Util.h"

#include "VBA.h"
#include "MainWnd.h"
#include "Reg.h"
#include "resource.h"

#include "Display.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern int Init_2xSaI(u32);
extern void winlog(const char *,...);
extern int systemSpeed;
extern int winVideoModeSelect(CWnd *, GUID **);

class DirectDrawDisplay : public IDisplay {
private:
  HINSTANCE            ddrawDLL;
  LPDIRECTDRAW7        pDirectDraw;
  LPDIRECTDRAWSURFACE7 ddsPrimary;
  LPDIRECTDRAWSURFACE7 ddsOffscreen;
  LPDIRECTDRAWSURFACE7 ddsFlip;
  LPDIRECTDRAWCLIPPER  ddsClipper;
  int                  width;
  int                  height;
  bool                 failed;

  bool initializeOffscreen(int w, int h);
public:
  DirectDrawDisplay();
  virtual ~DirectDrawDisplay();

  virtual bool initialize();
  virtual void cleanup();
  virtual void render();
  virtual void checkFullScreen();
  virtual void renderMenu();
  virtual void clear();
  virtual bool changeRenderSize(int w, int h);
  virtual DISPLAY_TYPE getType() { return DIRECT_DRAW; };
  virtual void setOption(const char *, int) {}
  virtual bool isSkinSupported() { return true; }
  virtual int selectFullScreenMode(GUID **);  
};

static HRESULT WINAPI checkModesAvailable(LPDDSURFACEDESC2 surf, LPVOID lpContext)
{
  if(surf->dwWidth == 320 &&
     surf->dwHeight == 240 &&
     surf->ddpfPixelFormat.dwRGBBitCount == 16) {
    theApp.mode320Available = TRUE;
  }
  if(surf->dwWidth == 640 &&
     surf->dwHeight == 480 &&
     surf->ddpfPixelFormat.dwRGBBitCount == 16) {
    theApp.mode640Available = TRUE;
  }
  if(surf->dwWidth == 800 &&
     surf->dwHeight == 600 &&
     surf->ddpfPixelFormat.dwRGBBitCount == 16) {
    theApp.mode800Available = TRUE;
  }
  return DDENUMRET_OK;
}

static int ffs(UINT mask)
{
  int m = 0;
  if (mask) {
    while (!(mask & (1 << m)))
      m++;
    
    return (m);
  }
  
  return (0);
}

DirectDrawDisplay::DirectDrawDisplay()
{
  pDirectDraw = NULL;
  ddsPrimary = NULL;
  ddsOffscreen = NULL;
  ddsFlip = NULL;
  ddsClipper = NULL;
  ddrawDLL = NULL;
  width = 0;
  height = 0;
  failed = false;
}

DirectDrawDisplay::~DirectDrawDisplay()
{
  cleanup();
}

void DirectDrawDisplay::cleanup()
{
  if(pDirectDraw != NULL) {
    if(ddsClipper != NULL) {
      ddsClipper->Release();
      ddsClipper = NULL;
    }

    if(ddsFlip != NULL) {
      ddsFlip->Release();
      ddsFlip = NULL;
    }

    if(ddsOffscreen != NULL) {
      ddsOffscreen->Release();
      ddsOffscreen = NULL;
    }
    
    if(ddsPrimary != NULL) {
      ddsPrimary->Release();
      ddsPrimary = NULL;
    }
    
    pDirectDraw->Release();
    pDirectDraw = NULL;
  }

  if(ddrawDLL != NULL) {
#ifdef _AFXDLL
    AfxFreeLibrary( ddrawDLL );
#else
    FreeLibrary( ddrawDLL );
#endif
    ddrawDLL = NULL;
  }
  width = 0;
  height = 0;
}

bool DirectDrawDisplay::initialize()
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
    styleEx = WS_EX_TOPMOST;

  if(theApp.videoOption <= VIDEO_4X)
    AdjustWindowRectEx(&theApp.dest, style, TRUE, styleEx);
  else
    AdjustWindowRectEx(&theApp.dest, style, FALSE, styleEx);    

  int winSizeX = theApp.dest.right-theApp.dest.left;
  int winSizeY = theApp.dest.bottom-theApp.dest.top;

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
  
  GUID *guid = NULL;
  if(theApp.ddrawEmulationOnly)
    guid = (GUID *)DDCREATE_EMULATIONONLY;

  if(theApp.pVideoDriverGUID)
    guid = theApp.pVideoDriverGUID;

#ifdef _AFXDLL
  ddrawDLL = AfxLoadLibrary("ddraw.dll");
#else
  ddrawDLL = LoadLibrary( _T("ddraw.dll") );
#endif

  HRESULT (WINAPI *DDrawCreateEx)(GUID *,LPVOID *,REFIID,IUnknown *);  
  if(ddrawDLL != NULL) {    
    DDrawCreateEx = (HRESULT (WINAPI *)(GUID *,LPVOID *,REFIID,IUnknown *))
      GetProcAddress(ddrawDLL, "DirectDrawCreateEx");

    if(DDrawCreateEx == NULL) {
      theApp.directXMessage("DirectDrawCreateEx");
      return FALSE;
    }
  } else {
    theApp.directXMessage("DDRAW.DLL");
    return FALSE;
  }

  theApp.ddrawUsingEmulationOnly = theApp.ddrawEmulationOnly;
  
  HRESULT hret = DDrawCreateEx(guid,
                               (void **)&pDirectDraw,
                               IID_IDirectDraw7,
                               NULL);
    
  if(hret != DD_OK) {
    winlog("Error creating DirectDraw object %08x\n", hret);
    if(theApp.ddrawEmulationOnly) {
      // disable emulation only setting in case of failure
      regSetDwordValue("ddrawEmulationOnly", 0);
    }
    //    errorMessage(myLoadString(IDS_ERROR_DISP_DRAWCREATE), hret);
    return FALSE;
  }

  if(theApp.ddrawDebug) {
    DDCAPS driver;
    DDCAPS hel;
    ZeroMemory(&driver, sizeof(driver));
    ZeroMemory(&hel, sizeof(hel));
    driver.dwSize = sizeof(driver);
    hel.dwSize = sizeof(hel);
    pDirectDraw->GetCaps(&driver, &hel);
    int i;
    DWORD *p = (DWORD *)&driver;
    for(i = 0; i < (int)driver.dwSize; i+=4)
      winlog("Driver CAPS %2d: %08x\n", i>>2, *p++);
    p = (DWORD *)&hel;
    for(i = 0; i < (int)hel.dwSize; i+=4)
      winlog("HEL CAPS %2d: %08x\n", i>>2, *p++);
  }
  
  theApp.mode320Available = false;
  theApp.mode640Available = false;
  theApp.mode800Available = false;
  // check for available fullscreen modes
  pDirectDraw->EnumDisplayModes(DDEDM_STANDARDVGAMODES, NULL, NULL,
                                checkModesAvailable);
  
  DWORD flags = DDSCL_NORMAL;

  if(theApp.videoOption >= VIDEO_320x240)
    flags = DDSCL_ALLOWMODEX |
      DDSCL_ALLOWREBOOT |
      DDSCL_EXCLUSIVE |
      DDSCL_FULLSCREEN;
  
  hret = pDirectDraw->SetCooperativeLevel(pWnd->m_hWnd,  
                                          flags);

  if(hret != DD_OK) {
    winlog("Error SetCooperativeLevel %08x\n", hret);    
    //    errorMessage(myLoadString(IDS_ERROR_DISP_DRAWLEVEL), hret);
    return FALSE;
  }
  
  if(theApp.videoOption > VIDEO_4X) {
    hret = pDirectDraw->SetDisplayMode(theApp.fsWidth,
                                       theApp.fsHeight,
                                       theApp.fsColorDepth,
                                       0,
                                       0);
    if(hret != DD_OK) {
      winlog("Error SetDisplayMode %08x\n", hret);
      //      errorMessage(myLoadString(IDS_ERROR_DISP_DRAWSET), hret);
      return FALSE;
    }
  }
  
  DDSURFACEDESC2 ddsd;
  ZeroMemory(&ddsd,sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
  if(theApp.videoOption > VIDEO_4X) {
    if(theApp.tripleBuffering) {
      // setup triple buffering
      ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
      ddsd.ddsCaps.dwCaps |= DDSCAPS_COMPLEX | DDSCAPS_FLIP;
      ddsd.dwBackBufferCount = 2;
    }
  }
  
  hret = pDirectDraw->CreateSurface(&ddsd, &ddsPrimary, NULL);
  if(hret != DD_OK) {
    winlog("Error primary CreateSurface %08x\n", hret);    
    //    errorMessage(myLoadString(IDS_ERROR_DISP_DRAWSURFACE), hret);
    return FALSE;
  }

  if(theApp.ddrawDebug) {
    DDSCAPS2 caps;
    ZeroMemory(&caps, sizeof(caps));
    ddsPrimary->GetCaps(&caps);

    winlog("Primary CAPS 1: %08x\n", caps.dwCaps);
    winlog("Primary CAPS 2: %08x\n", caps.dwCaps2);
    winlog("Primary CAPS 3: %08x\n", caps.dwCaps3);
    winlog("Primary CAPS 4: %08x\n", caps.dwCaps4);
  }

  if(theApp.videoOption > VIDEO_4X && theApp.tripleBuffering) {
    DDSCAPS2 caps;
    ZeroMemory(&caps, sizeof(caps));
    // this gets the third surface. The front one is the primary,
    // the second is the backbuffer and the third is the flip
    // surface
    caps.dwCaps = DDSCAPS_BACKBUFFER;
    
    hret = ddsPrimary->GetAttachedSurface(&caps, &ddsFlip);
    if(hret != DD_OK) {
      winlog("Failed to get attached surface %08x", hret);
      return FALSE;
    }

    ddsFlip->AddRef();
    clear();
  }

  // create clipper in all modes to avoid paint problems
  //  if(videoOption <= VIDEO_4X) {
  hret = pDirectDraw->CreateClipper(0, &ddsClipper, NULL);
  if(hret == DD_OK) {
    ddsClipper->SetHWnd(0, pWnd->m_hWnd);
    if(theApp.videoOption > VIDEO_4X) {
      if(theApp.tripleBuffering)
        ddsFlip->SetClipper(ddsClipper);
      else
        ddsPrimary->SetClipper(ddsClipper);
    } else
      ddsPrimary->SetClipper(ddsClipper);
  }
  //  }

  DDPIXELFORMAT px;

  px.dwSize = sizeof(px);

  hret = ddsPrimary->GetPixelFormat(&px);

  switch(px.dwRGBBitCount) {
  case 15:
  case 16:
    systemColorDepth = 16;
    break;
  case 24:
    systemColorDepth = 24;
    theApp.filterFunction = NULL;
    break;
  case 32:
    systemColorDepth = 32;
    break;
  default:
    systemMessage(IDS_ERROR_DISP_COLOR, "Unsupported display setting for color depth: %d bits. \nWindows desktop must be in either 16-bit, 24-bit or 32-bit mode for this program to work in window mode.",px.dwRGBBitCount);
    return FALSE;
  }
  theApp.updateFilter();
  theApp.updateIFB();

  if(failed)
    return false;

  pWnd->DragAcceptFiles(TRUE);
  
  return true;  
}

bool DirectDrawDisplay::changeRenderSize(int w, int h)
{
  if(w != width || h != height) {
    if(ddsOffscreen) {
      ddsOffscreen->Release();
      ddsOffscreen = NULL;
    }
    if(!initializeOffscreen(w, h)) {
      failed = true;
      return false;
    }
  }
  return true;
}

bool DirectDrawDisplay::initializeOffscreen(int w, int h)
{
  DDSURFACEDESC2 ddsd;
  
  ZeroMemory(&ddsd, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
  ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
  if(theApp.ddrawUseVideoMemory)
    ddsd.ddsCaps.dwCaps |= (DDSCAPS_LOCALVIDMEM|DDSCAPS_VIDEOMEMORY);
  ddsd.dwWidth = w;
  ddsd.dwHeight = h;

  HRESULT hret = pDirectDraw->CreateSurface(&ddsd, &ddsOffscreen, NULL);

  if(hret != DD_OK) {
    winlog("Error offscreen CreateSurface %08x\n", hret);    
    if(theApp.ddrawUseVideoMemory) {
      regSetDwordValue("ddrawUseVideoMemory", 0);
    }    
    //    errorMessage(myLoadString(IDS_ERROR_DISP_DRAWSURFACE2), hret);
    return false;
  }

  if(theApp.ddrawDebug) {
    DDSCAPS2 caps;
    ZeroMemory(&caps, sizeof(caps));
    ddsOffscreen->GetCaps(&caps);

    winlog("Offscreen CAPS 1: %08x\n", caps.dwCaps);
    winlog("Offscreen CAPS 2: %08x\n", caps.dwCaps2);
    winlog("Offscreen CAPS 3: %08x\n", caps.dwCaps3);
    winlog("Offscreen CAPS 4: %08x\n", caps.dwCaps4);
  }
  
  DDPIXELFORMAT px;

  px.dwSize = sizeof(px);

  hret = ddsOffscreen->GetPixelFormat(&px);

  if(theApp.ddrawDebug) {
    DWORD *pdword = (DWORD *)&px;
    for(int ii = 0; ii < 8; ii++) {
      winlog("Pixel format %d %08x\n", ii, pdword[ii]);
    }
  }
  
  switch(px.dwRGBBitCount) {
  case 15:
  case 16:
    systemColorDepth = 16;
    break;
  case 24:
    systemColorDepth = 24;
    theApp.filterFunction = NULL;
    break;
  case 32:
    systemColorDepth = 32;
    break;
  default:
    systemMessage(IDS_ERROR_DISP_COLOR, "Unsupported display setting for color depth: %d bits. \nWindows desktop must be in either 16-bit, 24-bit or 32-bit mode for this program to work in window mode.",px.dwRGBBitCount);
    return FALSE;
  }
  if(theApp.ddrawDebug) {
    winlog("R Mask: %08x\n", px.dwRBitMask);
    winlog("G Mask: %08x\n", px.dwGBitMask);
    winlog("B Mask: %08x\n", px.dwBBitMask);
  }
  
  systemRedShift = ffs(px.dwRBitMask);
  systemGreenShift = ffs(px.dwGBitMask);
  systemBlueShift = ffs(px.dwBBitMask);

#ifdef MMX
  if(!theApp.disableMMX)
    cpu_mmx = theApp.detectMMX();
  else
    cpu_mmx = 0;
#endif
  
  if((px.dwFlags&DDPF_RGB) != 0 &&
     px.dwRBitMask == 0xF800 &&
     px.dwGBitMask == 0x07E0 &&
     px.dwBBitMask == 0x001F) {
    systemGreenShift++;
    Init_2xSaI(565);
  } else if((px.dwFlags&DDPF_RGB) != 0 &&
            px.dwRBitMask == 0x7C00 &&
            px.dwGBitMask == 0x03E0 &&
            px.dwBBitMask == 0x001F) {
    Init_2xSaI(555);
  } else if((px.dwFlags&DDPF_RGB) != 0 &&
            px.dwRBitMask == 0x001F &&
            px.dwGBitMask == 0x07E0 &&
            px.dwBBitMask == 0xF800) {
    systemGreenShift++;
    Init_2xSaI(565);
  } else if((px.dwFlags&DDPF_RGB) != 0 &&
            px.dwRBitMask == 0x001F &&
            px.dwGBitMask == 0x03E0 &&
            px.dwBBitMask == 0x7C00) {
    Init_2xSaI(555);
  } else {
    // 32-bit or 24-bit
    if(systemColorDepth == 32 || systemColorDepth == 24) {
      systemRedShift += 3;
      systemGreenShift += 3;
      systemBlueShift += 3;
      if(systemColorDepth == 32)
        Init_2xSaI(32);
    }
  }

  if(theApp.ddrawDebug) {
    winlog("R shift: %d\n", systemRedShift);
    winlog("G shift: %d\n", systemGreenShift);
    winlog("B shift: %d\n", systemBlueShift);
  }
  
  utilUpdateSystemColorMaps();
  width = w;
  height = h;
  return true;
}

void DirectDrawDisplay::clear()
{
  if(theApp.videoOption <= VIDEO_4X || !theApp.tripleBuffering || ddsFlip == NULL)
    return;

  DDBLTFX fx;
  ZeroMemory(&fx, sizeof(fx));
  fx.dwSize = sizeof(fx);
  fx.dwFillColor = 0;
  ddsFlip->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
  ddsPrimary->Flip(NULL, 0);
  ddsFlip->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
  ddsPrimary->Flip(NULL, 0);
  ddsFlip->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
  ddsPrimary->Flip(NULL, 0);    
}

void DirectDrawDisplay::renderMenu()
{
  checkFullScreen();
  theApp.m_pMainWnd->DrawMenuBar();
}

void DirectDrawDisplay::checkFullScreen()
{
  if(theApp.tripleBuffering)
    pDirectDraw->FlipToGDISurface();
}

void DirectDrawDisplay::render()
{
  HRESULT hret;
  unsigned int nBytesPerPixel = systemColorDepth>>3;

  if(pDirectDraw == NULL ||
     ddsOffscreen == NULL ||
     ddsPrimary == NULL)
    return;

  DDSURFACEDESC2 ddsDesc;
  
  ZeroMemory(&ddsDesc, sizeof(ddsDesc));

  ddsDesc.dwSize = sizeof(ddsDesc);

  hret = ddsOffscreen->Lock(NULL,
                            &ddsDesc,
                            DDLOCK_WRITEONLY|
#ifndef FINAL_VERSION
                            DDLOCK_NOSYSLOCK|
#endif
                            DDLOCK_SURFACEMEMORYPTR,
                            NULL);

  if(hret == DDERR_SURFACELOST) {
    hret = ddsPrimary->Restore();
    if(hret == DD_OK) {
      hret = ddsOffscreen->Restore();
      
      if(hret == DD_OK) {
        hret = ddsOffscreen->Lock(NULL,
                                  &ddsDesc,
                                  DDLOCK_WRITEONLY|
#ifndef FINAL_VERSION
                                  DDLOCK_NOSYSLOCK|
#endif
                                  DDLOCK_SURFACEMEMORYPTR,
                                  NULL);
        
      }
    }
  }
    
  if(hret == DD_OK) {
    if(theApp.filterFunction) {
      if(systemColorDepth == 16)
        (*theApp.filterFunction)(pix+theApp.filterWidth*2+4,
                                 theApp.filterWidth*2+4,
                                 (u8*)theApp.delta,
                                 (u8*)ddsDesc.lpSurface,
                                 ddsDesc.lPitch,
                                 theApp.filterWidth,
                                 theApp.filterHeight);
      else
        (*theApp.filterFunction)(pix+theApp.filterWidth*4+4,
                                 theApp.filterWidth*4+4,
                                 (u8*)theApp.delta,
                                 (u8*)ddsDesc.lpSurface,
                                 ddsDesc.lPitch,
                                 theApp.filterWidth,
                                 theApp.filterHeight);
        
    } else {
      int copyX = 240;
      int copyY = 160;
      
      if(theApp.cartridgeType == 1) {
        if(gbBorderOn) {
          copyX = 256;
          copyY = 224;
        } else {
          copyX = 160;
          copyY = 144;
        }
      }
	  copyImage( pix, ddsDesc.lpSurface, copyX, copyY, ddsDesc.lPitch, systemColorDepth );
    }
    if(theApp.showSpeed && (theApp.videoOption > VIDEO_4X || theApp.skin != NULL)) {
      char buffer[30];
      if(theApp.showSpeed == 1)
        sprintf(buffer, "%3d%%", systemSpeed);
      else
        sprintf(buffer, "%3d%%(%d, %d fps)", systemSpeed,
                systemFrameSkip,
                theApp.showRenderedFrames);
      if(theApp.showSpeedTransparent)
        drawTextTransp((u8*)ddsDesc.lpSurface,
                       ddsDesc.lPitch,
                       theApp.rect.left+10,
                       theApp.rect.bottom-10,
                       buffer);
      else
        drawText((u8*)ddsDesc.lpSurface,
                 ddsDesc.lPitch,
                 theApp.rect.left+10,
                 theApp.rect.bottom-10,
                 buffer);        
    }
  } else if(theApp.ddrawDebug)
    winlog("Error during lock: %08x\n", hret);

  hret = ddsOffscreen->Unlock(NULL);

  if(hret == DD_OK) {
   if(theApp.vsync && !(theApp.tripleBuffering && theApp.videoOption > VIDEO_4X) && !speedup) { // isn't the Flip() call synced unless a certain flag is passed to it?
      hret = pDirectDraw->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, 0);
    }
    ddsOffscreen->PageLock(0);
    if(theApp.tripleBuffering && theApp.videoOption > VIDEO_4X) {
      hret = ddsFlip->Blt(&theApp.dest, ddsOffscreen, NULL, DDBLT_WAIT, NULL);
      if(hret == DD_OK) {
        if(theApp.menuToggle || !theApp.active) {
          pDirectDraw->FlipToGDISurface();
          ddsPrimary->SetClipper(ddsClipper);
          hret = ddsPrimary->Blt(&theApp.dest, ddsFlip, NULL, DDBLT_ASYNC, NULL);
          // if using emulation only, then we have to redraw the menu
          // everytime. It seems like a bug in DirectDraw to me as we not
          // overwritting the menu area at all.
          if(theApp.ddrawUsingEmulationOnly)
            theApp.m_pMainWnd->DrawMenuBar();
        } else
          hret = ddsPrimary->Flip(NULL, 0);
      }
    } else {
      hret = ddsPrimary->Blt(&theApp.dest, ddsOffscreen, NULL,DDBLT_ASYNC,NULL);
      
      if(hret == DDERR_SURFACELOST) {
        hret = ddsPrimary->Restore();
        
        if(hret == DD_OK) {
          hret = ddsPrimary->Blt(&theApp.dest, ddsOffscreen, NULL, DDBLT_ASYNC, NULL);
        }
      }
    }
    ddsOffscreen->PageUnlock(0);
  } else if(theApp.ddrawDebug)
    winlog("Error during unlock: %08x\n", hret);

  if(theApp.screenMessage) {
    if(((GetTickCount() - theApp.screenMessageTime) < 3000) &&
       !theApp.disableStatusMessage) {
      ddsPrimary->SetClipper(ddsClipper);
      HDC hdc;
      ddsPrimary->GetDC(&hdc);
      SetTextColor(hdc, RGB(255,0,0));
      SetBkMode(hdc,TRANSPARENT);      
      TextOut(hdc, theApp.dest.left+10, theApp.dest.bottom - 20, theApp.screenMessageBuffer,
              (int)_tcslen(theApp.screenMessageBuffer));
      ddsPrimary->ReleaseDC(hdc);
    } else {
      theApp.screenMessage = false;
    }
  }
  
  if(hret != DD_OK) {
    if(theApp.ddrawDebug)
      winlog("Error on update screen: %08x\n", hret);
  }  
}

int DirectDrawDisplay::selectFullScreenMode(GUID **pGUID)
{
  return winVideoModeSelect(theApp.m_pMainWnd, pGUID);
}

IDisplay *newDirectDrawDisplay()
{
  return new DirectDrawDisplay();
}

