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
#include "MainWnd.h"
#include <gl/GL.h>

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../Text.h"
#include "../Util.h"

#include "Reg.h"
#include "..\..\res\resource.h"

#include "../gbafilter.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifdef MMX
extern "C" bool cpu_mmx;

extern bool detectMMX();
#endif

extern int Init_2xSaI(u32);
extern void winlog(const char *,...);
extern int systemSpeed;

class OpenGLDisplay : public IDisplay {
private:
  HDC hDC;
  HGLRC hglrc;
  GLuint texture;
  int width;
  int height;
  float size;
  u8 *filterData;
  bool failed;
  
  bool initializeTexture(int w, int h);
  void updateFiltering(int);
public:
  OpenGLDisplay();
  virtual ~OpenGLDisplay();

  virtual bool initialize();
  virtual void cleanup();
  virtual void render();
  virtual void checkFullScreen();
  virtual void renderMenu();
  virtual void clear();
  virtual bool changeRenderSize(int w, int h);
  virtual void resize(int w, int h);
  virtual DISPLAY_TYPE getType() { return OPENGL; };
  virtual void setOption(const char *, int);
  virtual int selectFullScreenMode(GUID **);  
};

OpenGLDisplay::OpenGLDisplay()
{
  hDC = NULL;
  hglrc = NULL;
  texture = 0;
  width = 0;
  height = 0;
  size = 0.0f;
  filterData = (u8 *)malloc(4*4*4*256*240);
  failed = false;
}

OpenGLDisplay::~OpenGLDisplay()
{
  cleanup();
}

void OpenGLDisplay::cleanup()
{
  if(texture != 0) {
    glDeleteTextures(1, &texture);
    texture = 0;
  }
  if(hglrc != NULL) {
    wglDeleteContext(hglrc);
    wglMakeCurrent(NULL, NULL);
    hglrc = NULL;
  }
  if(hDC != NULL) {
    ReleaseDC(*theApp.m_pMainWnd, hDC);
    hDC = NULL;
  }
  if(filterData) {
    free(filterData);
    filterData = NULL;
  }
  width = 0;
  height = 0;
  size = 0.0f;
}

bool OpenGLDisplay::initialize()
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
  case VIDEO_1024x768:
  case VIDEO_1280x1024:
  case VIDEO_OTHER:
    {
      RECT r;
      ::GetWindowRect(GetDesktopWindow(), &r);
      theApp.fsWidth = r.right - r.left;
      theApp.fsHeight = r.bottom - r.top;

      /* Need to fix this code later. For now, Fullscreen takes the whole
         screen.
         int scaleX = (fsWidth / sizeX);
         int scaleY = (fsHeight / sizeY);
         int min = scaleX < scaleY ? scaleX : scaleY;
         surfaceSizeX = sizeX * min;
         surfaceSizeY = sizeY * min;
         if(fullScreenStretch) {
      */
      theApp.surfaceSizeX = theApp.fsWidth;
      theApp.surfaceSizeY = theApp.fsHeight;
      //      }
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
  
  theApp.mode320Available = FALSE;
  theApp.mode640Available = FALSE;
  theApp.mode800Available = FALSE;

  CDC *dc = pWnd->GetDC();
  HDC hDC = dc->GetSafeHdc();
 
  PIXELFORMATDESCRIPTOR pfd = { 
    sizeof(PIXELFORMATDESCRIPTOR),  //  size of this pfd 
    1,                     // version number 
    PFD_DRAW_TO_WINDOW |   // support window 
    PFD_SUPPORT_OPENGL |   // support OpenGL 
    PFD_DOUBLEBUFFER,      // double buffered 
    PFD_TYPE_RGBA,         // RGBA type 
    16,                    // 16-bit color depth 
    0, 0, 0, 0, 0, 0,      // color bits ignored 
    0,                     // no alpha buffer 
    0,                     // shift bit ignored 
    0,                     // no accumulation buffer 
    0, 0, 0, 0,            // accum bits ignored 
    32,                    // 32-bit z-buffer     
    0,                     // no stencil buffer 
    0,                     // no auxiliary buffer 
    PFD_MAIN_PLANE,        // main layer 
    0,                     // reserved 
    0, 0, 0                // layer masks ignored 
  }; 
  int  iPixelFormat; 
  
  if(!(iPixelFormat = ChoosePixelFormat(hDC, &pfd))) {
    winlog("Failed ChoosePixelFormat\n");
    return false;
  }

  // obtain detailed information about 
  // the device context's first pixel format
  if(!(DescribePixelFormat(hDC, iPixelFormat,  
                           sizeof(PIXELFORMATDESCRIPTOR), &pfd))) {
    winlog("Failed DescribePixelFormat\n");
    return false;
  }

  if(!SetPixelFormat(hDC, iPixelFormat, &pfd)) {
    winlog("Failed SetPixelFormat\n");
    return false;
  }

  if(!(hglrc = wglCreateContext(hDC))) {
    winlog("Failed wglCreateContext\n");
    return false;
  }

  if(!wglMakeCurrent(hDC, hglrc)) {
    winlog("Failed wglMakeCurrent\n");
    return false;
  }
  pWnd->ReleaseDC(dc);
  
  // setup 2D gl environment
  glPushAttrib(GL_ENABLE_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_TEXTURE_2D);

  glViewport(0, 0, theApp.surfaceSizeX, theApp.surfaceSizeY);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glOrtho(0.0, (GLdouble)(theApp.surfaceSizeX), (GLdouble)(theApp.surfaceSizeY),
          0.0, 0.0,1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  systemRedShift = 3;
  systemGreenShift = 11;
  systemBlueShift = 19;
  systemColorDepth = 32;
  theApp.fsColorDepth = 32;
  
  Init_2xSaI(32);
#ifdef MMX
  if(!theApp.disableMMX)
    cpu_mmx = theApp.detectMMX();
  else
    cpu_mmx = 0;
#endif
  
  if(theApp.ddrawDebug) {
    winlog("R shift: %d\n", systemRedShift);
    winlog("G shift: %d\n", systemGreenShift);
    winlog("B shift: %d\n", systemBlueShift);
  }
  
  utilUpdateSystemColorMaps(theApp.filterLCD);
  theApp.updateFilter();
  theApp.updateIFB();

  if(failed)
    return false;
  
  pWnd->DragAcceptFiles(TRUE);
  
  return TRUE;  
}

void OpenGLDisplay::clear()
{
}

void OpenGLDisplay::renderMenu()
{
  checkFullScreen();
  if(theApp.m_pMainWnd)
    theApp.m_pMainWnd->DrawMenuBar();
}

void OpenGLDisplay::checkFullScreen()
{
  //  if(tripleBuffering)
  //    pOpenGL->FlipToGDISurface();
}

void OpenGLDisplay::render()
{
  int pitch = theApp.filterWidth * 4 + 4;
  u8 *data = pix + (theApp.sizeX+1)*4;
  
  if(theApp.filterFunction) {
    data = filterData;
    theApp.filterFunction(pix+pitch,
                          pitch,
                          (u8*)theApp.delta,
                          (u8*)filterData,
                          theApp.rect.right * (systemColorDepth / 8),
                          theApp.filterWidth,
                          theApp.filterHeight);
  }

  if(theApp.videoOption > VIDEO_4X && theApp.showSpeed) {
    char buffer[30];
    if(theApp.showSpeed == 1)
      sprintf(buffer, "%3d%%", systemSpeed);
    else
      sprintf(buffer, "%3d%%(%d, %d fps)", systemSpeed,
              systemFrameSkip,
              theApp.showRenderedFrames);

    if(theApp.filterFunction) {
      int p = theApp.rect.right * 2;
      if(systemColorDepth == 24)
        p = theApp.rect.right * 3;
      else if(systemColorDepth == 32)
        p = theApp.rect.right * 4;
      if(theApp.showSpeedTransparent)
        drawTextTransp((u8*)filterData,
                       p,
                       10,
                       theApp.rect.bottom-10,
                       buffer);
      else
        drawText((u8*)filterData,
                 p,
                 10,
                 theApp.rect.bottom-10,
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
  
  // Texturemap complete texture to surface so we have free scaling 
  // and antialiasing
  if(theApp.filterFunction) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, theApp.rect.right);
  } else {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, theApp.sizeX+1);
  }

  glTexSubImage2D( GL_TEXTURE_2D,0,
                   0,0, theApp.rect.right,theApp.rect.bottom,
                   GL_RGBA,GL_UNSIGNED_BYTE,data);

  if(theApp.glType == 0) {
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0, 0.0); glVertex3i(0, 0, 0);
    glTexCoord2f(theApp.rect.right/size, 0.0); glVertex3i(theApp.surfaceSizeX, 0, 0);
    glTexCoord2f(0.0, theApp.rect.bottom/size); glVertex3i(0, theApp.surfaceSizeY, 0);
    glTexCoord2f(theApp.rect.right/size, theApp.rect.bottom/size); glVertex3i(theApp.surfaceSizeX, theApp.surfaceSizeY, 0);
    glEnd();
  } else {
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0); glVertex3i(0, 0, 0);
    glTexCoord2f(theApp.rect.right/size, 0.0); glVertex3i(theApp.surfaceSizeX, 0, 0);
    glTexCoord2f(theApp.rect.right/size, theApp.rect.bottom/size); glVertex3i(theApp.surfaceSizeX, theApp.surfaceSizeY, 0);
    glTexCoord2f(0.0, theApp.rect.bottom/size); glVertex3i(0, theApp.surfaceSizeY, 0);
    glEnd();
  }

  CDC *dc = theApp.m_pMainWnd->GetDC();

  if(theApp.screenMessage) {
    if(((GetTickCount() - theApp.screenMessageTime) < 3000) &&
       !theApp.disableStatusMessage) {
      dc->SetTextColor(RGB(255,0,0));
      dc->SetBkMode(TRANSPARENT);
      dc->TextOut(10, theApp.surfaceSizeY - 20, theApp.screenMessageBuffer);
    } else {
      theApp.screenMessage = false;
    }
  }  
  
  SwapBuffers(dc->GetSafeHdc());
  
  theApp.m_pMainWnd->ReleaseDC(dc);
}

void OpenGLDisplay::resize(int w, int h)
{
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glOrtho(0.0, (GLdouble)(w), (GLdouble)(h),
          0.0, 0.0,1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void OpenGLDisplay::updateFiltering(int value)
{
  switch(value) {
  case 0:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    break;
  case 1:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    break;
  }
}

bool OpenGLDisplay::initializeTexture(int w, int h)
{
  int mySize = 256;
  size = 256.0f;
  if(w > 255 || h > 255) {
    size = 512.0f;
    mySize = 512;
  }
  if(w > 512 || h > 512) {
    size = 1024.0f;
    mySize = 1024;
  }
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  int filter = regQueryDwordValue("glFilter", 0);
  if(filter < 0 || filter > 1)
    filter = 0;
  updateFiltering(filter);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mySize, mySize, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL );
  width = w;
  height = h;

  return true;
}

bool OpenGLDisplay::changeRenderSize(int w, int h)
{
  if(width != w || height != h) {
    if(texture != 0) {
      glDeleteTextures(1, &texture);
      texture = 0;
    }
    if(!initializeTexture(w, h)) {
      failed = true;
      return false;
    }
  }
  return true;
}

void OpenGLDisplay::setOption(const char *option, int value)
{
  if(!strcmp(option, "glFilter"))
    updateFiltering(value);
}

int OpenGLDisplay::selectFullScreenMode(GUID **)
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

IDisplay *newOpenGLDisplay()
{
  return new OpenGLDisplay();
}

