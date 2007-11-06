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

// VBA.cpp : Defines the class behaviors for the application.
//
#include "stdafx.h"

#include "AVIWrite.h"
#include "LangSelect.h"
#include "MainWnd.h"
#include "Reg.h"
#include "resource.h"
#include "resource2.h"
#include "skin.h"
#include "WavWriter.h"
#include "WinResUtil.h"

#include "../System.h"
#include "../agbprint.h"
#include "../cheatSearch.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../RTC.h"
#include "../Sound.h"
#include "../Util.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbPrinter.h"

extern void Pixelate(u8*,u32,u8*,u8*,u32,int,int);
extern void Pixelate32(u8*,u32,u8*,u8*,u32,int,int);
extern void MotionBlur(u8*,u32,u8*,u8*,u32,int,int);
extern void MotionBlur32(u8*,u32,u8*,u8*,u32,int,int);
extern void _2xSaI(u8*,u32,u8*,u8*,u32,int,int);
extern void _2xSaI32(u8*,u32,u8*,u8*,u32,int,int);
extern void Super2xSaI(u8*,u32,u8*,u8*,u32,int,int);
extern void Super2xSaI32(u8*,u32,u8*,u8*,u32,int,int);
extern void SuperEagle(u8*,u32,u8*,u8*,u32,int,int);
extern void SuperEagle32(u8*,u32,u8*,u8*,u32,int,int);
extern void AdMame2x(u8*,u32,u8*,u8*,u32,int,int);
extern void AdMame2x32(u8*,u32,u8*,u8*,u32,int,int);
extern void Simple2x(u8*,u32,u8*,u8*,u32,int,int);
extern void Simple2x32(u8*,u32,u8*,u8*,u32,int,int);
extern void Bilinear(u8*,u32,u8*,u8*,u32,int,int);
extern void Bilinear32(u8*,u32,u8*,u8*,u32,int,int);
extern void BilinearPlus(u8*,u32,u8*,u8*,u32,int,int);
extern void BilinearPlus32(u8*,u32,u8*,u8*,u32,int,int);
extern void Scanlines(u8*,u32,u8*,u8*,u32,int,int);
extern void Scanlines32(u8*,u32,u8*,u8*,u32,int,int);
extern void ScanlinesTV(u8*,u32,u8*,u8*,u32,int,int);
extern void ScanlinesTV32(u8*,u32,u8*,u8*,u32,int,int);
extern void hq2x(u8*,u32,u8*,u8*,u32,int,int);
extern void hq2x32(u8*,u32,u8*,u8*,u32,int,int);
extern void lq2x(u8*,u32,u8*,u8*,u32,int,int);
extern void lq2x32(u8*,u32,u8*,u8*,u32,int,int);

extern void SmartIB(u8*,u32,int,int);
extern void SmartIB32(u8*,u32,int,int);
extern void MotionBlurIB(u8*,u32,int,int);
extern void InterlaceIB(u8*,u32,int,int);
extern void MotionBlurIB32(u8*,u32,int,int);

extern void toolsLog(const char *);

extern IDisplay *newGDIDisplay();
extern IDisplay *newDirectDrawDisplay();
extern IDisplay *newDirect3DDisplay();
extern IDisplay *newOpenGLDisplay();

extern Input *newDirectInput();

extern ISound *newDirectSound();

extern void remoteStubSignal(int, int);
extern void remoteOutput(char *, u32);
extern void remoteStubMain();
extern void remoteSetProtocol(int);
extern void remoteCleanUp();
extern int remoteSocket;

extern void InterframeCleanup();

void winlog(const char *msg, ...);

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int emulating = 0;
bool debugger = false;
int RGB_LOW_BITS_MASK = 0;

int systemFrameSkip = 0;
int systemSpeed = 0;
bool systemSoundOn = false;
u32 systemColorMap32[0x10000];
u16 systemColorMap16[0x10000];
u16 systemGbPalette[24];
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 16;
int systemVerbose = 0;
int systemDebug = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

void winSignal(int,int);
void winOutput(char *, u32);

void (*dbgSignal)(int,int) = winSignal;
void (*dbgOutput)(char *, u32) = winOutput;

#ifdef MMX
extern "C" bool cpu_mmx;
#endif

namespace Sm60FPS
{
  float					K_fCpuSpeed = 98.0f;
  float					K_fTargetFps = 60.0f * K_fCpuSpeed / 100;
  float					K_fDT = 1000.0f / K_fTargetFps;

  u32					dwTimeElapse;
  u32					dwTime0;
  u32					dwTime1;
  u32					nFrameCnt;
  float					fWantFPS;
  float					fCurFPS;
  bool					bLastSkip;
  int					nCurSpeed;
  int					bSaveMoreCPU;
};

void directXMessage(const char *msg)
{
  systemMessage(IDS_DIRECTX_7_REQUIRED,
                "DirectX 7.0 or greater is required to run.\nDownload at http://www.microsoft.com/directx.\n\nError found at: %s",
                msg);
}

/////////////////////////////////////////////////////////////////////////////
// VBA

BEGIN_MESSAGE_MAP(VBA, CWinApp)
  //{{AFX_MSG_MAP(VBA)
  // NOTE - the ClassWizard will add and remove mapping macros here.
  //    DO NOT EDIT what you see in these blocks of generated code!
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// VBA construction

VBA::VBA()
{
  mode320Available = false;
  mode640Available = false;
  mode800Available = false;
  windowPositionX = 0;
  windowPositionY = 0;
  filterFunction = NULL;
  ifbFunction = NULL;
  ifbType = 0;
  filterType = 0;
  filterWidth = 0;
  filterHeight = 0;
  fsAdapter = 0;
  fsWidth = 0;
  fsHeight = 0;
  fsColorDepth = 0;
  fsFrequency = 0;
  fsForceChange = false;
  surfaceSizeX = 0;
  surfaceSizeY = 0;
  sizeX = 0;
  sizeY = 0;
  videoOption = 0;
  fullScreenStretch = false;
  disableStatusMessage = false;
  showSpeed = 1;
  showSpeedTransparent = true;
  showRenderedFrames = 0;
  screenMessage = false;
  screenMessageTime = 0;
  menuToggle = true;
  display = NULL;
  menu = NULL;
  popup = NULL;
  cartridgeType = IMAGE_GBA;
  soundInitialized = false;
  useBiosFile = false;
  skipBiosFile = false;
  active = true;
  paused = false;
  recentFreeze = false;
  autoSaveLoadCheatList = false;
  winout = NULL;
  removeIntros = false;
  autoIPS = true;
  winGbBorderOn = 0;
  winFlashSize = 0x10000;
  winRtcEnable = false;
  winGenericflashcardEnable = false;
  winSaveType = 0;
  rewindMemory = NULL;
  rewindPos = 0;
  rewindTopPos = 0;
  rewindCounter = 0;
  rewindCount = 0;
  rewindSaveNeeded = false;
  rewindTimer = 0;
  captureFormat = 0;
  tripleBuffering = true;
  autoHideMenu = false;
  throttle = 0;
  throttleLastTime = 0;
  autoFrameSkipLastTime = 0;  
  autoFrameSkip = false;
  vsync = false;
  changingVideoSize = false;
  pVideoDriverGUID = NULL;
  renderMethod = DIRECT_DRAW;
  iconic = false;
  ddrawEmulationOnly = false;
  ddrawUsingEmulationOnly = false;
  ddrawDebug = false;
  ddrawUseVideoMemory = false;
  d3dFilter = 0;
  glFilter = 0;
  glType = 0;
  skin = NULL;
  skinName = "";
  skinEnabled = false;
  skinButtons = 0;
  regEnabled = false;
  pauseWhenInactive = true;
  speedupToggle = false;
  useOldSync = false;
  winGbPrinterEnabled = false;
  threadPriority = 2;
  disableMMX = false;
  languageOption = 0;
  languageModule = NULL;
  languageName = "";
  renderedFrames = 0;
  input = NULL;
  joypadDefault = 0;
  autoFire = 0;
  autoFireToggle = false;
  winPauseNextFrame = false;
  soundRecording = false;
  soundRecorder = NULL;
  dsoundDisableHardwareAcceleration = true;
  sound = NULL;
  aviRecording = false;
  aviRecorder = NULL;
  aviFrameNumber = 0;
  painting = false;
  movieRecording = false;
  moviePlaying = false;
  movieFrame = 0;
  moviePlayFrame = 0;
  movieFile = NULL;
  movieLastJoypad = 0;
  movieNextJoypad = 0;
  sensorX = 2047;
  sensorY = 2047;
  mouseCounter = 0;
  wasPaused = false;
  frameskipadjust = 0;
  autoLoadMostRecent = false;
  fsMaxScale = 0;
  romSize = 0;
  
  updateCount = 0;

  systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

  ZeroMemory(&emulator, sizeof(emulator));

  hAccel = NULL;

  for(int i = 0; i < 24;) {
    systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
    systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
    systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
    systemGbPalette[i++] = 0;
  }
}

VBA::~VBA()
{
  InterframeCleanup();

  char winBuffer[2048];

  GetModuleFileName(NULL, winBuffer, 2048);
  char *p = strrchr(winBuffer, '\\');
  if(p)
    *p = 0;
  
  regInit(winBuffer);

  saveSettings();

  if(moviePlaying) {
    if(movieFile != NULL) {
      fclose(movieFile);
      movieFile = NULL;
    }
    moviePlaying = false;
    movieLastJoypad = 0;
  }

  if(movieRecording) {
    if(movieFile != NULL) {
      // record the last joypad change so that the correct time can be
      // recorded
      fwrite(&movieFrame, 1, sizeof(int), movieFile);
      fwrite(&movieLastJoypad, 1, sizeof(u32), movieFile);
      fclose(movieFile);
      movieFile = NULL;
    }
    movieRecording = false;
    moviePlaying = false;
    movieLastJoypad = 0;
  }

  if(aviRecorder) {
    delete aviRecorder;
    aviRecording = false;
  }

  if(soundRecorder) {
    delete soundRecorder;
    soundRecorder = NULL;
  }
  soundRecording = false;
  soundPause();
  soundShutdown();

  if(gbRom != NULL || rom != NULL) {
    if(autoSaveLoadCheatList)
      ((MainWnd *)m_pMainWnd)->winSaveCheatListDefault();
    ((MainWnd *)m_pMainWnd)->writeBatteryFile();
    cheatSearchCleanup(&cheatSearchData);
    emulator.emuCleanUp();
  }

  if(input)
    delete input;

  shutdownDisplay();

  if(skin) {
    delete skin;
  }

  if(rewindMemory)
    free(rewindMemory);
}

/////////////////////////////////////////////////////////////////////////////
// The one and only VBA object

VBA theApp;
/////////////////////////////////////////////////////////////////////////////
// VBA initialization

// code from SDL_main.c for Windows
/* Parse a command line buffer into arguments */
static int parseCommandLine(char *cmdline, char **argv)
{
  char *bufp;
  int argc;
  
  argc = 0;
  for ( bufp = cmdline; *bufp; ) {
    /* Skip leading whitespace */
    while ( isspace(*bufp) ) {
      ++bufp;
    }
    /* Skip over argument */
    if ( *bufp == '"' ) {
      ++bufp;
      if ( *bufp ) {
        if ( argv ) {
          argv[argc] = bufp;
        }
        ++argc;
      }
      /* Skip over word */
      while ( *bufp && (*bufp != '"') ) {
        ++bufp;
      }
    } else {
      if ( *bufp ) {
        if ( argv ) {
          argv[argc] = bufp;
        }
        ++argc;
      }
      /* Skip over word */
      while ( *bufp && ! isspace(*bufp) ) {
        ++bufp;
      }
    }
    if ( *bufp ) {
      if ( argv ) {
        *bufp = '\0';
      }
      ++bufp;
    }
  }
  if ( argv ) {
    argv[argc] = NULL;
  }
  return(argc);
}

BOOL VBA::InitInstance()
{
#if _MSC_VER < 1400
#ifdef _AFXDLL
  Enable3dControls();      // Call this when using MFC in a shared DLL
#else
  Enable3dControlsStatic();  // Call this when linking to MFC statically
#endif
#endif

  SetRegistryKey(_T("VBA"));

  remoteSetProtocol(0);

  systemVerbose = GetPrivateProfileInt("config",
                                       "verbose",
                                       0,
                                       "VBA.ini");
  
  systemDebug = GetPrivateProfileInt("config",
                                     "debug",
                                     0,
                                     "VBA.ini");
  ddrawDebug = GetPrivateProfileInt("config",
                                    "ddrawDebug",
                                    0,
                                    "VBA.ini") ? true : false;

  wndClass = AfxRegisterWndClass(0, LoadCursor(IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH), LoadIcon(IDI_ICON));
  
  char winBuffer[2048];

  GetModuleFileName(NULL, winBuffer, 2048);
  char *p = strrchr(winBuffer, '\\');
  if(p)
    *p = 0;
  
  regInit(winBuffer);

  loadSettings();

  if(!initInput())
    return FALSE;

  if(!initDisplay()) {
    if(videoOption >= VIDEO_320x240) {
      regSetDwordValue("video", VIDEO_1X);
      if(pVideoDriverGUID)
        regSetDwordValue("defaultVideoDriver", TRUE);
    }
    return FALSE;
  }

  hAccel = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_ACCELERATOR));

  winAccelMgr.Connect((MainWnd *)m_pMainWnd);

  winAccelMgr.SetRegKey(HKEY_CURRENT_USER, "Software\\Emulators\\VisualBoyAdvance");

  extern void winAccelAddCommands(CAcceleratorManager&);

  winAccelAddCommands(winAccelMgr);

  winAccelMgr.CreateDefaultTable();

  winAccelMgr.Load();

  winAccelMgr.UpdateWndTable();
  
  winAccelMgr.UpdateMenu(menu);

  if (m_lpCmdLine[0])
    {
      int argc = parseCommandLine(m_lpCmdLine, NULL);
      char **argv = (char **)malloc((argc+1)*sizeof(char *));
      parseCommandLine(m_lpCmdLine, argv);
      if(argc > 0) {
        szFile = argv[0];
        filename = szFile;
      }
      int index = filename.ReverseFind('.');

      if(index != -1)
        filename = filename.Left(index);
      
      if(((MainWnd*)m_pMainWnd)->FileRun())
        emulating = true;
      else
        emulating = false;
      free(argv);
    }
        
  return TRUE;
}


void VBA::adjustDestRect()
{
  POINT point;
  RECT skinRect;
  if(skin)
    skinRect = skin->GetBlitRect();
  
  point.x = 0;
  point.y = 0;

  if(skin) {
    point.x = skinRect.left;
    point.y = skinRect.top;
  }

  m_pMainWnd->ClientToScreen(&point);
  dest.top = point.y;
  dest.left = point.x;

  point.x = surfaceSizeX;
  point.y = surfaceSizeY;

  if(skin) {
    point.x = skinRect.right;
    point.y = skinRect.bottom;
  }

  m_pMainWnd->ClientToScreen(&point);
  dest.bottom = point.y;
  dest.right = point.x;

  // make sure that dest rect lies in the monitor
  if(videoOption >= VIDEO_320x240) {
    dest.top -= windowPositionY;
    dest.left -= windowPositionX;
    dest.bottom-= windowPositionY;
    dest.right -= windowPositionX;
  }

  if(skin)
    return;
  
  int menuSkip = 0;
  
  if(videoOption >= VIDEO_320x240 && menuToggle) {
    int m = GetSystemMetrics(SM_CYMENU);
    menuSkip = m;
    dest.bottom -=m;
  }

  if(videoOption > VIDEO_4X) {
    int top = (fsHeight - surfaceSizeY) / 2;
    int left = (fsWidth - surfaceSizeX) / 2;
    dest.top += top;
    dest.bottom += top;
    dest.left += left;
    dest.right += left;
    if(fullScreenStretch) {
      dest.top = 0+menuSkip;
      dest.left = 0;
      dest.right = fsWidth;
      dest.bottom = fsHeight;
    }          
  }
}


void VBA::updateIFB()
{
  if(systemColorDepth == 16) {
    switch(ifbType) {
    case 0:
    default:
      ifbFunction = NULL;
      break;
    case 1:
      ifbFunction = MotionBlurIB;
      break;
    case 2:
      ifbFunction = SmartIB;
      break;
    }
  } else if(systemColorDepth == 32) {
    switch(ifbType) {
    case 0:
    default:
      ifbFunction = NULL;
      break;
    case 1:
      ifbFunction = MotionBlurIB32;
      break;
    case 2:
      ifbFunction = SmartIB32;
      break;
    }
  } else
    ifbFunction = NULL;
}

void VBA::updateFilter()
{
  filterWidth = sizeX;
  filterHeight = sizeY;
  
  if(systemColorDepth == 16 && (videoOption > VIDEO_1X &&
                                videoOption != VIDEO_320x240)) {
    switch(filterType) {
    default:
    case 0:
      filterFunction = NULL;
      break;
    case 1:
      filterFunction = ScanlinesTV;
      break;
    case 2:
      filterFunction = _2xSaI;
      break;
    case 3:
      filterFunction = Super2xSaI;
      break;
    case 4:
      filterFunction = SuperEagle;
      break;
    case 5:
      filterFunction = Pixelate;
      break;
    case 6:
      filterFunction = MotionBlur;
      break;
    case 7:
      filterFunction = AdMame2x;
      break;
    case 8:
      filterFunction = Simple2x;
      break;
    case 9:
      filterFunction = Bilinear;
      break;
    case 10:
      filterFunction = BilinearPlus;
      break;
    case 11:
      filterFunction = Scanlines;
      break;
    case 12:
      filterFunction = hq2x;
      break;
    case 13:
      filterFunction = lq2x;
      break;
    }
    
    if(filterType != 0) {
      rect.right = sizeX*2;
      rect.bottom = sizeY*2;
      memset(delta,255,sizeof(delta));
    } else {
      rect.right = sizeX;
      rect.bottom = sizeY;
    }
  } else {
    if(systemColorDepth == 32 && videoOption > VIDEO_1X &&
       videoOption != VIDEO_320x240) {
      switch(filterType) {
      default:
      case 0:
        filterFunction = NULL;
        break;
      case 1:
        filterFunction = ScanlinesTV32;
        break;
      case 2:
        filterFunction = _2xSaI32;
        break;
      case 3:
        filterFunction = Super2xSaI32;
        break;
      case 4:
        filterFunction = SuperEagle32;
        break;        
      case 5:
        filterFunction = Pixelate32;
        break;
      case 6:
        filterFunction = MotionBlur32;
        break;
      case 7:
        filterFunction = AdMame2x32;
        break;
      case 8:
        filterFunction = Simple2x32;
        break;
      case 9:
        filterFunction = Bilinear32;
        break;
      case 10:
        filterFunction = BilinearPlus32;
        break;
      case 11:
        filterFunction = Scanlines32;
        break;
      case 12:
        filterFunction = hq2x32;
        break;
      case 13:
        filterFunction = lq2x32;
        break;
      }
      if(filterType != 0) {
        rect.right = sizeX*2;
        rect.bottom = sizeY*2;
        memset(delta,255,sizeof(delta));
      } else {
        rect.right = sizeX;
        rect.bottom = sizeY;
      }
    } else
      filterFunction = NULL;
  }

  if(display)
    display->changeRenderSize(rect.right, rect.bottom);  
}

void VBA::updateMenuBar()
{
  if(menu != NULL) {
    if(m_pMainWnd)
      m_pMainWnd->SetMenu(NULL);
    m_menu.Detach();
    DestroyMenu(menu);
  }

  if(popup != NULL) {
    // force popup recreation if language changed
    DestroyMenu(popup);
    popup = NULL;
  }

  m_menu.Attach(winResLoadMenu(MAKEINTRESOURCE(IDR_MENU)));
  menu = (HMENU)m_menu;

  // don't set a menu if skin is active
  if(skin == NULL)
    if(m_pMainWnd)
      m_pMainWnd->SetMenu(&m_menu);
}

void winlog(const char *msg, ...)
{
  CString buffer;
  va_list valist;

  va_start(valist, msg);
  buffer.FormatV(msg, valist);
  
  if(theApp.winout == NULL) {
    theApp.winout = fopen("vba-trace.log","w");
  }

  fputs(buffer, theApp.winout);
  
  va_end(valist);
}

void log(const char *msg, ...)
{
  CString buffer;
  va_list valist;

  va_start(valist, msg);
  buffer.FormatV(msg, valist);
  
  toolsLog(buffer);
  
  va_end(valist);
}

bool systemReadJoypads()
{
  if(theApp.input)
    return theApp.input->readDevices();
  return false;
}

u32 systemReadJoypad(int which)
{
  if(theApp.input)
    return theApp.input->readDevice(which);
  return 0;
}

void systemDrawScreen()
{
  if(theApp.display == NULL)
    return;

  theApp.renderedFrames++;

  if(theApp.updateCount) {
    POSITION pos = theApp.updateList.GetHeadPosition();
    while(pos) {
      IUpdateListener *up = theApp.updateList.GetNext(pos);
      up->update();
    }
  }

  if (Sm60FPS_CanSkipFrame())
	  return;

  if(theApp.aviRecording && !theApp.painting) {
    int width = 240;
    int height = 160;
    switch(theApp.cartridgeType) {
    case 0:
      width = 240;
      height = 160;
      break;
    case 1:
      if(gbBorderOn) {
        width = 256;
        height = 224;
      } else {
        width = 160;
        height = 144;
      }
      break;
    }
    
    if(theApp.aviRecorder == NULL) {
      theApp.aviRecorder = new AVIWrite();
      theApp.aviFrameNumber = 0;
      
      theApp.aviRecorder->SetFPS(60);
        
      BITMAPINFOHEADER bi;
      memset(&bi, 0, sizeof(bi));      
      bi.biSize = 0x28;    
      bi.biPlanes = 1;
      bi.biBitCount = 24;
      bi.biWidth = width;
      bi.biHeight = height;
      bi.biSizeImage = 3*width*height;
      theApp.aviRecorder->SetVideoFormat(&bi);
      theApp.aviRecorder->Open(theApp.aviRecordName);
    }
    
    char *bmp = new char[width*height*3];
    
    utilWriteBMP(bmp, width, height, pix);
    theApp.aviRecorder->AddFrame(theApp.aviFrameNumber, bmp);
    
    delete bmp;
  }

  if( theApp.ifbFunction ) {
	  theApp.ifbFunction( pix + (theApp.filterWidth * (systemColorDepth>>3)) + 4,
		  (theApp.filterWidth * (systemColorDepth>>3)) + 4,
		  theApp.filterWidth, theApp.filterHeight );
  }

  theApp.display->render();

  Sm60FPS_Sleep();
}

void systemScreenCapture(int captureNumber)
{
  if(theApp.m_pMainWnd)
    ((MainWnd *)theApp.m_pMainWnd)->screenCapture(captureNumber);
}

u32 systemGetClock()
{
  return GetTickCount();
}

void systemMessage(int number, const char *defaultMsg, ...)
{
  CString buffer;
  va_list valist;
  CString msg = defaultMsg;
  if(number)
    msg = winResLoadString(number);
  
  va_start(valist, defaultMsg);
  buffer.FormatV(msg, valist);

  theApp.winCheckFullscreen();

  AfxGetApp()->m_pMainWnd->MessageBox(buffer, winResLoadString(IDS_ERROR), MB_OK|MB_ICONERROR);

  va_end(valist);
}

void systemSetTitle(const char *title)
{
  if(theApp.m_pMainWnd != NULL) {
    AfxGetApp()->m_pMainWnd->SetWindowText(title);
  }
}

void systemShowSpeed(int speed)
{
  systemSpeed = speed;
  theApp.showRenderedFrames = theApp.renderedFrames;
  theApp.renderedFrames = 0;
  if(theApp.videoOption <= VIDEO_4X && theApp.showSpeed) {
    CString buffer;
    if(theApp.showSpeed == 1)
      buffer.Format("VisualBoyAdvance-%3d%%", systemSpeed);
    else
      buffer.Format("VisualBoyAdvance-%3d%%(%d, %d fps)", systemSpeed,
                    systemFrameSkip,
                    theApp.showRenderedFrames);

    systemSetTitle(buffer);
  }
}

void systemFrame()
{
  if(theApp.aviRecording)
    theApp.aviFrameNumber++;
  if(theApp.movieRecording || theApp.moviePlaying)
    theApp.movieFrame++;
}

void system10Frames(int rate)
{
  u32 time = systemGetClock();  

  if (theApp.autoFrameSkip)
  {
    u32 diff = time - theApp.autoFrameSkipLastTime;
	Sm60FPS::nCurSpeed = 100;

    if (diff)
		Sm60FPS::nCurSpeed = (1000000/rate)/diff;
  }



  if(!theApp.wasPaused && theApp.throttle) {
    if(!speedup) {
      u32 diff = time - theApp.throttleLastTime;
      
      int target = (1000000/(rate*theApp.throttle));
      int d = (target - diff);
      
      if(d > 0) {
        Sleep(d);
      }
    }
    theApp.throttleLastTime = systemGetClock();
  }
  if(theApp.rewindMemory) {
    if(++theApp.rewindCounter >= (theApp.rewindTimer)) {
      theApp.rewindSaveNeeded = true;
      theApp.rewindCounter = 0;
    }
  }
  if(systemSaveUpdateCounter) {
    if(--systemSaveUpdateCounter <= SYSTEM_SAVE_NOT_UPDATED) {
      ((MainWnd *)theApp.m_pMainWnd)->writeBatteryFile();
      systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
    }
  }

  theApp.wasPaused = false;
  theApp.autoFrameSkipLastTime = time;
}

void systemScreenMessage(const char *msg)
{
  theApp.screenMessage = true;
  theApp.screenMessageTime = GetTickCount();
  theApp.screenMessageBuffer = msg;

  if(theApp.screenMessageBuffer.GetLength() > 40)
    theApp.screenMessageBuffer = theApp.screenMessageBuffer.Left(40);
}

void systemUpdateMotionSensor()
{
  if(theApp.input)
    theApp.input->checkMotionKeys();
}

int systemGetSensorX()
{
  return theApp.sensorX;
}

int systemGetSensorY()
{
  return theApp.sensorY;
}

bool systemSoundInit()
{
  if(theApp.sound)
    delete theApp.sound;

  theApp.sound = newDirectSound();
  return theApp.sound->init();
}


void systemSoundShutdown()
{
  if(theApp.sound)
    delete theApp.sound;
  theApp.sound = NULL;
}

void systemSoundPause()
{
  if(theApp.sound)
    theApp.sound->pause();
}

void systemSoundReset()
{
  if(theApp.sound)
    theApp.sound->reset();
}

void systemSoundResume()
{
  if(theApp.sound)
    theApp.sound->resume();
}

void systemWriteDataToSoundBuffer()
{
  if(theApp.sound)
    theApp.sound->write();
}

bool systemCanChangeSoundQuality()
{
  return true;
}

bool systemPauseOnFrame()
{
  if(theApp.winPauseNextFrame) {
    theApp.paused = true;
    theApp.winPauseNextFrame = false;
    return true;
  }
  return false;
}

void systemGbBorderOn()
{
  if(emulating && theApp.cartridgeType == IMAGE_GB && gbBorderOn) {
    theApp.updateWindowSize(theApp.videoOption);
  }
}

BOOL VBA::OnIdle(LONG lCount) 
{
  if(emulating && debugger) {
    MSG msg;    
    remoteStubMain();
    if(debugger)
      return TRUE; // continue loop
    return !::PeekMessage(&msg, NULL, NULL, NULL, PM_NOREMOVE);
  } else if(emulating && active && !paused) {
    for(int i = 0; i < 2; i++) {
      emulator.emuMain(emulator.emuCount);

      if(rewindSaveNeeded && rewindMemory && emulator.emuWriteMemState) {
        rewindCount++;
        if(rewindCount > 8)
          rewindCount = 8;
        if(emulator.emuWriteMemState(&rewindMemory[rewindPos*REWIND_SIZE], 
                                     REWIND_SIZE)) {
          rewindPos = ++rewindPos & 7;
          if(rewindCount == 8)
            rewindTopPos = ++rewindTopPos & 7;
        }
      }
    
      rewindSaveNeeded = false;
    }
    
    if(mouseCounter) {
      if(--mouseCounter == 0) {
        SetCursor(NULL);
      }
    }    
    return TRUE;
  }
  return FALSE;
  
  //  return CWinApp::OnIdle(lCount);
}

void VBA::addRecentFile(CString file)
{
  // Do not change recent list if frozen
  if(recentFreeze)
    return;
  int i = 0;
  for(i = 0; i < 10; i++) {
    if(recentFiles[i].GetLength() == 0)
      break;
    
    if(recentFiles[i].Compare(file) == 0) {
      if(i == 0)
        return;
      CString p = recentFiles[i];
      for(int j = i; j > 0; j--) {
        recentFiles[j] = recentFiles[j-1];
      }
      recentFiles[0] = p;
      return;
    }
  }
  int num = 0;
  for(i = 0; i < 10; i++) {
    if(recentFiles[i].GetLength() != 0)
      num++;
  }
  if(num == 10) {
    num--;
  }

  for(i = num; i >= 1; i--) {
    recentFiles[i] = recentFiles[i-1];
  }
  recentFiles[0] = file;
}

void VBA::loadSettings()
{
  CString buffer;

  languageOption = regQueryDwordValue("language", 1);
  if(languageOption < 0 || languageOption > 2)
    languageOption = 1;

  buffer = regQueryStringValue("languageName", "");
  if(!buffer.IsEmpty()) {
    languageName = buffer.Left(3);
  } else
    languageName = "";
  
  winSetLanguageOption(languageOption, true);
  
  frameSkip = regQueryDwordValue("frameSkip", 0);
  if(frameSkip < 0 || frameSkip > 9)
    frameSkip = 0;

  gbFrameSkip = regQueryDwordValue("gbFrameSkip", 0);
  if(gbFrameSkip < 0 || gbFrameSkip > 9)
    gbFrameSkip = 0;

  autoFrameSkip = regQueryDwordValue("autoFrameSkip", FALSE) ? TRUE : FALSE;
  
  vsync = regQueryDwordValue("vsync", false) ? true : false ;
  synchronize = regQueryDwordValue("synchronize", 1) ? true : false;
  fullScreenStretch = regQueryDwordValue("stretch", 0) ? true : false;

  videoOption = regQueryDwordValue("video", 1);
  if(videoOption < 0 || videoOption > VIDEO_OTHER)
    videoOption = 0;

  bool defaultVideoDriver = regQueryDwordValue("defaultVideoDriver", true) ?
    true : false;

  if(!regQueryBinaryValue("videoDriverGUID", (char *)&videoDriverGUID,
                          sizeof(GUID))) {
    defaultVideoDriver = TRUE;
  }

  if(defaultVideoDriver)
    pVideoDriverGUID = NULL;
  else
    pVideoDriverGUID = &videoDriverGUID;

  fsAdapter = regQueryDwordValue("fsAdapter", 0);
  fsWidth = regQueryDwordValue("fsWidth", 0);
  fsHeight = regQueryDwordValue("fsHeight", 0);
  fsColorDepth = regQueryDwordValue("fsColorDepth", 0);
  fsFrequency = regQueryDwordValue("fsFrequency", 0);

  if(videoOption == VIDEO_OTHER) {
    if(fsWidth < 0 || fsWidth > 4095 || fsHeight < 0 || fsHeight > 4095)
      videoOption = 0;
    if(fsColorDepth != 16 && fsColorDepth != 24 && fsColorDepth != 32)
      videoOption = 0;
  }

  renderMethod = (DISPLAY_TYPE)regQueryDwordValue("renderMethod", DIRECT_DRAW);
  if(renderMethod < GDI || renderMethod > OPENGL)
    renderMethod = DIRECT_DRAW;
  
  windowPositionX = regQueryDwordValue("windowX", 0);
  if(windowPositionX < 0)
    windowPositionX = 0;
  windowPositionY = regQueryDwordValue("windowY", 0);
  if(windowPositionY < 0)
    windowPositionY = 0;

  useBiosFile = regQueryDwordValue("useBios", 0) ? true: false;

  skipBiosFile = regQueryDwordValue("skipBios", 0) ? true : false;

  buffer = regQueryStringValue("biosFile", "");

  if(!buffer.IsEmpty()) {
    biosFileName = buffer;
  }
  
  int res = regQueryDwordValue("soundEnable", 0x30f);
  
  soundEnable(res);
  soundDisable(~res);

  soundOffFlag = (regQueryDwordValue("soundOff", 0)) ? true : false;

  soundQuality = regQueryDwordValue("soundQuality", 1);

  soundEcho = regQueryDwordValue("soundEcho", 0) ? true : false;

  soundLowPass = regQueryDwordValue("soundLowPass", 0) ? true : false;

  soundReverse = regQueryDwordValue("soundReverse", 0) ? true : false;

  soundVolume = regQueryDwordValue("soundVolume", 0);
  if(soundVolume < 0 || soundVolume > 5)
    soundVolume = 0;

  ddrawEmulationOnly = regQueryDwordValue("ddrawEmulationOnly", false) ? true : false;
  ddrawUseVideoMemory = regQueryDwordValue("ddrawUseVideoMemory", true) ? true : false;
  tripleBuffering = regQueryDwordValue("tripleBuffering", false) ? true : false;

  d3dFilter = regQueryDwordValue("d3dFilter", 1);
  if(d3dFilter < 0 || d3dFilter > 1)
    d3dFilter = 1;

  glFilter = regQueryDwordValue("glFilter", 1);
  if(glFilter < 0 || glFilter > 1)
    glFilter = 1;

  glType = regQueryDwordValue("glType", 0);
  if(glType < 0 || glType > 1)
    glType = 0;

  filterType = regQueryDwordValue("filter", 0);
  if(filterType < 0 || filterType > 13)
    filterType = 0;

  disableMMX = regQueryDwordValue("disableMMX", false) ? true: false;

  disableStatusMessage = regQueryDwordValue("disableStatus", 0) ? true : false;

  showSpeed = regQueryDwordValue("showSpeed", 1);
  if(showSpeed < 0 || showSpeed > 2)
    showSpeed = 1;

  showSpeedTransparent = regQueryDwordValue("showSpeedTransparent", TRUE) ?
    TRUE : FALSE;

  winGbPrinterEnabled = regQueryDwordValue("gbPrinter", false) ? true : false;

  if(winGbPrinterEnabled)
    gbSerialFunction = gbPrinterSend;
  else
    gbSerialFunction = NULL;  

  pauseWhenInactive = regQueryDwordValue("pauseWhenInactive", 1) ?
    true : false;

  useOldSync = regQueryDwordValue("useOldSync", 0) ?
    TRUE : FALSE;

  captureFormat = regQueryDwordValue("captureFormat", 0);

  removeIntros = regQueryDwordValue("removeIntros", false) ? true : false;

  recentFreeze = regQueryDwordValue("recentFreeze", false) ? true : false;

  autoIPS = regQueryDwordValue("autoIPS", true) ? true : false;

  cpuDisableSfx = regQueryDwordValue("disableSfx", 0) ? true : false;
  
  winSaveType = regQueryDwordValue("saveType", 0);
  if(winSaveType < 0 || winSaveType > 5)
    winSaveType = 0;
  
  ifbType = regQueryDwordValue("ifbType", 0);
  if(ifbType < 0 || ifbType > 2)
    ifbType = 0;

  winFlashSize = regQueryDwordValue("flashSize", 0x10000);
  if(winFlashSize != 0x10000 && winFlashSize != 0x20000)
    winFlashSize = 0x10000;
  flashSize = winFlashSize;

  agbPrintEnable(regQueryDwordValue("agbPrint", 0) ? true : false);

  winRtcEnable = regQueryDwordValue("rtcEnabled", 0) ? true : false;
  rtcEnable(winRtcEnable);

  autoHideMenu = regQueryDwordValue("autoHideMenu", 0) ? true : false;

  skinEnabled = regQueryDwordValue("skinEnabled", 0) ? true : false;

  skinName = regQueryStringValue("skinName", "");

  switch(videoOption) {
  case VIDEO_320x240:
    fsWidth = 320;
    fsHeight = 240;
    fsColorDepth = 16;
    break;
  case VIDEO_640x480:
    fsWidth = 640;
    fsHeight = 480;
    fsColorDepth = 16;
    break;
  case VIDEO_800x600:
    fsWidth = 800;
    fsHeight = 600;
    fsColorDepth = 16;
    break;
  }

  winGbBorderOn = regQueryDwordValue("borderOn", 0);
  gbBorderAutomatic = regQueryDwordValue("borderAutomatic", 0);
  gbEmulatorType = regQueryDwordValue("emulatorType", 1);
  if(gbEmulatorType < 0 || gbEmulatorType > 5)
    gbEmulatorType = 1;
  gbColorOption = regQueryDwordValue("colorOption", 0);

  threadPriority = regQueryDwordValue("priority", 2);

  if(threadPriority < 0 || threadPriority >3)
    threadPriority = 2;
  updatePriority();

  autoSaveLoadCheatList = regQueryDwordValue("autoSaveCheatList", 0) ?
    true : false;

  gbPaletteOption = regQueryDwordValue("gbPaletteOption", 0);
  if(gbPaletteOption < 0)
    gbPaletteOption = 0;
  if(gbPaletteOption > 2)
    gbPaletteOption = 2;

  regQueryBinaryValue("gbPalette", (char *)systemGbPalette,
                      24*sizeof(u16));

  rewindTimer = regQueryDwordValue("rewindTimer", 0);

  if(rewindTimer < 0 || rewindTimer > 600)
    rewindTimer = 0;

  rewindTimer *= 6; // convert to 10 frames multiple
  
  if(rewindTimer != 0)
    rewindMemory = (char *)malloc(8*REWIND_SIZE);

  for(int i = 0; i < 10; i++) {
    buffer.Format("recent%d", i);
    char *s = regQueryStringValue(buffer, NULL);
    if(s == NULL)
      break;
    recentFiles[i] = s;
  }

  joypadDefault = regQueryDwordValue("joypadDefault", 0);
  if(joypadDefault < 0 || joypadDefault > 3)
    joypadDefault = 0;

  autoLoadMostRecent = regQueryDwordValue("autoLoadMostRecent", false) ? true :
    false;
  
  cheatsEnabled = regQueryDwordValue("cheatsEnabled", true) ? true : false;

  fsMaxScale = regQueryDwordValue("fsMaxScale", 0);

  throttle = regQueryDwordValue("throttle", 0);
  if(throttle < 5 || throttle > 1000)
    throttle = 0;

  if (autoFrameSkip)
  {
	  throttle = 0;
	  frameSkip = 0;
	  systemFrameSkip = 0;
  }

  Sm60FPS::bSaveMoreCPU = regQueryDwordValue("saveMoreCPU", 0);
}

void VBA::updateFrameSkip()
{
  switch(cartridgeType) {
  case 0:
    systemFrameSkip = frameSkip;
    break;
  case 1:
    systemFrameSkip = gbFrameSkip;
    break;
  }
}

void VBA::updateVideoSize(UINT id)
{
  int value = 0;

  switch(id) {
  case ID_OPTIONS_VIDEO_X1:
    value = VIDEO_1X;
    break;
  case ID_OPTIONS_VIDEO_X2:
    value = VIDEO_2X;
    break;
  case ID_OPTIONS_VIDEO_X3:
    value = VIDEO_3X;
    break;
  case ID_OPTIONS_VIDEO_X4:
    value = VIDEO_4X;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN320X240:
    value = VIDEO_320x240;
    fsWidth = 320;
    fsHeight = 240;
    fsColorDepth = 32;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN640X480:
    value = VIDEO_640x480;
    fsWidth = 640;
    fsHeight = 480;
    fsColorDepth = 32;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN800X600:
    value = VIDEO_800x600;
    fsWidth = 800;
    fsHeight = 600;
    fsColorDepth = 32;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN:
    value = VIDEO_OTHER;
    break;
  }

  updateWindowSize(value);
}

typedef BOOL (WINAPI *GETMENUBARINFO)(HWND, LONG, LONG, PMENUBARINFO);

static void winCheckMenuBarInfo(int& winSizeX, int& winSizeY)
{
  HINSTANCE hinstDll;
  DWORD dwVersion = 0;
  
#ifdef _AFXDLL
  hinstDll = AfxLoadLibrary("user32.dll");
#else
  hinstDll = LoadLibrary( _T("user32.dll") );
#endif
  
  if(hinstDll) {
	  GETMENUBARINFO func = (GETMENUBARINFO)GetProcAddress(hinstDll, "GetMenuBarInfo");

    if(func) {
      MENUBARINFO info;
      info.cbSize = sizeof(MENUBARINFO);
      
      func(AfxGetMainWnd()->GetSafeHwnd(), OBJID_MENU, 0, &info);
      
      int menuHeight = GetSystemMetrics(SM_CYMENU);
      
      if((info.rcBar.bottom - info.rcBar.top) > menuHeight) {
        winSizeY += (info.rcBar.bottom - info.rcBar.top) - menuHeight + 1;
        theApp.m_pMainWnd->SetWindowPos(
                                        0, //HWND_TOPMOST,
                                        theApp.windowPositionX,
                                        theApp.windowPositionY,
                                        winSizeX,
                                        winSizeY,
                                        SWP_NOMOVE | SWP_SHOWWINDOW);
      }
    }
#ifdef _AFXDLL
    AfxFreeLibrary( hinstDll );
#else
    FreeLibrary( hinstDll );
#endif
  }
}

void VBA::updateWindowSize(int value)
{
  regSetDwordValue("video", value);

  if(value == VIDEO_OTHER) {
    regSetDwordValue("fsWidth", fsWidth);
    regSetDwordValue("fsHeight", fsHeight);
    regSetDwordValue("fsColorDepth", fsColorDepth);
  }
  
  if(((value >= VIDEO_320x240) &&
      (videoOption != value)) ||
     (videoOption >= VIDEO_320x240 &&
      value <= VIDEO_4X) ||
     fsForceChange) {
    fsForceChange = false;
    changingVideoSize = true;
    shutdownDisplay();
    if(input) {
      delete input;
      input = NULL;
    }
    m_pMainWnd->DragAcceptFiles(FALSE);
    CWnd *pWnd = m_pMainWnd;
    m_pMainWnd = NULL;
    pWnd->DestroyWindow();
    delete pWnd;
    videoOption = value;
    if(!initDisplay()) {
      if(videoOption == VIDEO_320x240 ||
         videoOption == VIDEO_640x480 ||
         videoOption == VIDEO_800x600 ||
         videoOption == VIDEO_OTHER) {
        regSetDwordValue("video", VIDEO_1X);
        if(pVideoDriverGUID)
          regSetDwordValue("defaultVideoDriver", TRUE);
      }
      changingVideoSize = false;
      AfxPostQuitMessage(0);
      return;
    }
    if(!initInput()) {
      changingVideoSize = false;
      AfxPostQuitMessage(0);
      return;
    }
    input->checkKeys();
    updateMenuBar();
    changingVideoSize = FALSE;
    updateWindowSize(videoOption);
    return;
  }
  
  sizeX = 240;
  sizeY = 160;

  videoOption = value;
  
  if(cartridgeType == IMAGE_GB) {
    if(gbBorderOn) {
      sizeX = 256;
      sizeY = 224;
      gbBorderLineSkip = 256;
      gbBorderColumnSkip = 48;
      gbBorderRowSkip = 40;
    } else {
      sizeX = 160;
      sizeY = 144;
      gbBorderLineSkip = 160;
      gbBorderColumnSkip = 0;
      gbBorderRowSkip = 0;
    }
  }
  
  surfaceSizeX = sizeX;
  surfaceSizeY = sizeY;

  switch(videoOption) {
  case VIDEO_1X:
    surfaceSizeX = sizeX;
    surfaceSizeY = sizeY;
    break;
  case VIDEO_2X:
    surfaceSizeX = sizeX * 2;
    surfaceSizeY = sizeY * 2;
    break;
  case VIDEO_3X:
    surfaceSizeX = sizeX * 3;
    surfaceSizeY = sizeY * 3;
    break;
  case VIDEO_4X:
    surfaceSizeX = sizeX * 4;
    surfaceSizeY = sizeY * 4;
    break;
  case VIDEO_320x240:
  case VIDEO_640x480:
  case VIDEO_800x600:
  case VIDEO_OTHER:
    {
      int scaleX = 1;
      int scaleY = 1;
      scaleX = (fsWidth / sizeX);
      scaleY = (fsHeight / sizeY);
      int min = scaleX < scaleY ? scaleX : scaleY;
      if(fsMaxScale)
        min = min > fsMaxScale ? fsMaxScale : min;
      surfaceSizeX = min * sizeX;
      surfaceSizeY = min * sizeY;
      if((fullScreenStretch && (display != NULL && 
                                (display->getType() != DIRECT_3D)))
         || (display != NULL && display->getType() >= DIRECT_3D)) {
        surfaceSizeX = fsWidth;
        surfaceSizeY = fsHeight;
      }
    }
    break;
  }

  rect.right = sizeX;
  rect.bottom = sizeY;

  int winSizeX = sizeX;
  int winSizeY = sizeY;
  
  if(videoOption <= VIDEO_4X) {
    dest.left = 0;
    dest.top = 0;
    dest.right = surfaceSizeX;
    dest.bottom = surfaceSizeY;    
    
    DWORD style = WS_POPUP | WS_VISIBLE;
    
    style |= WS_OVERLAPPEDWINDOW;
    
    menuToggle = TRUE;
    AdjustWindowRectEx(&dest, style, TRUE, 0); //WS_EX_TOPMOST);
    
    winSizeX = dest.right-dest.left;
    winSizeY = dest.bottom-dest.top;

    if(skin == NULL) {
      m_pMainWnd->SetWindowPos(0, //HWND_TOPMOST,
                               windowPositionX,
                               windowPositionY,
                               winSizeX,
                               winSizeY,
                               SWP_NOMOVE | SWP_SHOWWINDOW);

      winCheckMenuBarInfo(winSizeX, winSizeY);
    }
  }

  adjustDestRect();

  updateIFB();  
  updateFilter();
  
  if(display)
    display->resize(theApp.dest.right-theApp.dest.left, theApp.dest.bottom-theApp.dest.top);
  
  m_pMainWnd->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);  
}

bool VBA::initDisplay()
{
  return updateRenderMethod(false);
}

bool VBA::updateRenderMethod(bool force)
{
	Sm60FPS_Init();
	bool res = updateRenderMethod0(force);

	while(!res && renderMethod > 0) {
		if( fsAdapter > 0 ) {
			fsAdapter = 0;
		} else {
			if( videoOption > VIDEO_4X ) {
				videoOption = VIDEO_1X;
				force = true;
			} else {
				if(renderMethod == OPENGL) {
					renderMethod = DIRECT_3D;
				} else {
					if(renderMethod == DIRECT_3D) {
						renderMethod = DIRECT_DRAW;
					} else {
						if(renderMethod == DIRECT_DRAW) {
							renderMethod = GDI;
						}
					}
				}
			}
		}
		res = updateRenderMethod(force);
	}

	return res;
}


bool VBA::updateRenderMethod0(bool force)
{
  bool initInput = false;
  
  if(display) {
    if(display->getType() != renderMethod || force) {
      if(skin) {
        delete skin;
        skin = NULL;
      }
      initInput = true;
      changingVideoSize = true;
      shutdownDisplay();
      if(input) {
        delete input;
        input = NULL;
      }
      CWnd *pWnd = m_pMainWnd;

      m_pMainWnd = NULL;
      pWnd->DragAcceptFiles(FALSE);
      pWnd->DestroyWindow();
      delete pWnd;
      
      display = NULL;
      regSetDwordValue("renderMethod", renderMethod);      
    }
  }
  if(display == NULL) {
    switch(renderMethod) {
    case GDI:
		display = newGDIDisplay();
		break;
    case DIRECT_DRAW:
		pVideoDriverGUID = NULL;
		ZeroMemory( &videoDriverGUID, sizeof( GUID ) );
		display = newDirectDrawDisplay();
		break;
    case DIRECT_3D:
		display = newDirect3DDisplay();
		break;
	case OPENGL:
		display = newOpenGLDisplay();
		break;
    }
    
    if(display->initialize()) {
      winUpdateSkin();
      if(initInput) {
        if(!this->initInput()) {
          changingVideoSize = false;
          AfxPostQuitMessage(0);
          return false;
        }
        input->checkKeys();
        updateMenuBar();
        changingVideoSize = false;
        updateWindowSize(videoOption);

        m_pMainWnd->ShowWindow(SW_SHOW);
        m_pMainWnd->UpdateWindow();
        m_pMainWnd->SetFocus();
        
        return true;
      } else {
        changingVideoSize = false;
        return true;
      }
    }
    changingVideoSize = false;
  }
  return true;
}


void VBA::winCheckFullscreen()
{
	if(videoOption > VIDEO_4X && tripleBuffering) {
		if(display) {
			display->checkFullScreen();
		}
	}
}


void VBA::shutdownDisplay()
{
  if(display != NULL) {
    display->cleanup();
    delete display;
    display = NULL;
  }
}

void VBA::directXMessage(const char *msg)
{
  systemMessage(IDS_DIRECTX_7_REQUIRED,
                "DirectX 7.0 or greater is required to run.\nDownload at http://www.microsoft.com/directx.\n\nError found at: %s",
                msg);
}

void VBA::winUpdateSkin()
{
#ifndef NOSKINS
  skinButtons = 0;
  if(skin) {
    delete skin;
    skin = NULL;
  }
  
  if(!skinName.IsEmpty() && skinEnabled && display->isSkinSupported()) {
    skin = new CSkin();
    if(skin->Initialize(skinName)) {
      skin->Hook(m_pMainWnd);
      skin->Enable(true);
    } else {
      delete skin;
      skin = NULL;
    }
  }

  if(!skin) {
    adjustDestRect();
    updateMenuBar();
  }
#endif
}

void VBA::updatePriority()
{
  switch(threadPriority) {
  case 0:
    SetThreadPriority(THREAD_PRIORITY_HIGHEST);
    break;
  case 1:
    SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL);    
    break;
  case 3:
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);    
    break;
  default:
    SetThreadPriority(THREAD_PRIORITY_NORMAL);
  }
}

#ifdef MMX
bool VBA::detectMMX()
{
  bool support = false;
  char brand[13];

  // check for Intel chip
  __try {
    __asm {
      mov eax, 0;
      cpuid;
      mov [dword ptr brand+0], ebx;
      mov [dword ptr brand+4], edx;
      mov [dword ptr brand+8], ecx;
    }
  }
  __except(EXCEPTION_EXECUTE_HANDLER) {
    if(_exception_code() == STATUS_ILLEGAL_INSTRUCTION) {
      return false;
    }
    return false;
  }
  // Check for Intel or AMD CPUs
  if(strncmp(brand, "GenuineIntel", 12)) {
    if(strncmp(brand, "AuthenticAMD", 12)) {
      return false;
    }
  }

  __asm {
    mov eax, 1;
    cpuid;
    test edx, 00800000h;
    jz NotFound;
    mov [support], 1;
  NotFound:
  }
  return support;
}
#endif

void VBA::winSetLanguageOption(int option, bool force)
{
  if(((option == languageOption) && option != 2) && !force)
    return;
  switch(option) {
  case 0:
    {
      char lbuffer[10];

      if(GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SABBREVLANGNAME,
                       lbuffer, 10)) {
        HINSTANCE l = winLoadLanguage(lbuffer);
        if(l == NULL) {
          LCID locIdBase = MAKELCID( MAKELANGID( PRIMARYLANGID( GetSystemDefaultLangID() ), SUBLANG_NEUTRAL ), SORT_DEFAULT );
          if(GetLocaleInfo(locIdBase, LOCALE_SABBREVLANGNAME,
                           lbuffer, 10)) {
            l = winLoadLanguage(lbuffer);
            if(l == NULL) {
              systemMessage(IDS_FAILED_TO_LOAD_LIBRARY,
                            "Failed to load library %s",
                            lbuffer);
              return;
            }
          }
        }
        AfxSetResourceHandle(l);
        if(languageModule != NULL)
#ifdef _AFXDLL
          AfxFreeLibrary( languageModule );
#else
          FreeLibrary( languageModule );
#endif
        languageModule = l;
      } else {
        systemMessage(IDS_FAILED_TO_GET_LOCINFO,
                      "Failed to get locale information");
        return;
      }
    }
    break;
  case 1:
    if(languageModule != NULL)
#ifdef _AFXDLL
      AfxFreeLibrary( languageModule );
#else
      FreeLibrary( languageModule );
#endif
    languageModule = NULL;
    AfxSetResourceHandle(AfxGetInstanceHandle());
    break;
  case 2:
    {
      if(!force) {
        LangSelect dlg;
        if(dlg.DoModal()) {
          HINSTANCE l = winLoadLanguage(languageName);
          if(l == NULL) {
            systemMessage(IDS_FAILED_TO_LOAD_LIBRARY,
                          "Failed to load library %s",
                          languageName);
            return;
          }
          AfxSetResourceHandle(l);
          if(languageModule != NULL)
		  {
#ifdef _AFXDLL
            AfxFreeLibrary( languageModule );
#else
            FreeLibrary( languageModule );
#endif
		  }
          languageModule = l;
        }
      } else {
        if(languageName.IsEmpty())
          return;
        HINSTANCE l = winLoadLanguage(languageName);
        if(l == NULL) {
          systemMessage(IDS_FAILED_TO_LOAD_LIBRARY,
                        "Failed to load library %s",
                        languageName);
          return;
        }
        AfxSetResourceHandle(l);
        if(languageModule != NULL)
          FreeLibrary(languageModule);
        languageModule = l;
      }
    }
    break;
  }
  languageOption = option;
  updateMenuBar();
}

HINSTANCE VBA::winLoadLanguage(const char *name)
{
  CString buffer;
  
  buffer.Format( _T("vba_%s.dll"), name);

#ifdef _AFXDLL
  HINSTANCE l = AfxLoadLibrary( buffer );
#else
  HMODULE l = LoadLibrary( buffer );
#endif
  
  if(l == NULL) {
    if(strlen(name) == 3) {
      char buffer2[3];
      buffer2[0] = name[0];
      buffer2[1] = name[1];
      buffer2[2] = 0;
      buffer.Format("vba_%s.dll", buffer2);

#ifdef _AFXDLL
	  return AfxLoadLibrary( buffer );
#else
	  return LoadLibrary( buffer );
#endif
    }
  }
  return l;
}


bool VBA::initInput()
{
  if(input)
    delete input;
  input = newDirectInput();
  if(input->initialize()) {
    input->loadSettings();
    input->checkKeys();
    return true;
  }
  delete input;
  return false;
}

void VBA::winAddUpdateListener(IUpdateListener *l)
{
  updateList.AddTail(l);
  updateCount++;
}

void VBA::winRemoveUpdateListener(IUpdateListener *l)
{
  POSITION pos = updateList.Find(l);
  if(pos) {
    updateList.RemoveAt(pos);
    updateCount--;
    if(updateCount < 0)
      updateCount = 0;
  }
}

CString VBA::winLoadFilter(UINT id)
{
  CString res = winResLoadString(id);
  res.Replace('_','|');
  
  return res;
}

void VBA::movieReadNext()
{
  if(movieFile) {
    bool movieEnd = false;

    if(fread(&moviePlayFrame, 1, sizeof(int), movieFile) == sizeof(int)) {
      if(fread(&movieNextJoypad, 1, sizeof(u32), movieFile) == sizeof(int)) {
        // make sure we don't have spurious entries on the movie that can
        // cause us to play it forever
        if(moviePlayFrame <= movieFrame)
          movieEnd = true;
      } else
        movieEnd = true;
    } else
      movieEnd = true;
    if(movieEnd) {
      CString string = winResLoadString(IDS_END_OF_MOVIE);
      systemScreenMessage(string);
      moviePlaying = false;
      fclose(movieFile);
      movieFile = NULL;
      return;
    }
  } else 
    moviePlaying = false;
}

void VBA::saveSettings()
{
  regSetDwordValue("language", languageOption);

  regSetStringValue("languageName", languageName);
  
  regSetDwordValue("frameSkip", frameSkip);

  regSetDwordValue("gbFrameSkip", gbFrameSkip);

  regSetDwordValue("autoFrameSkip", autoFrameSkip);
  
  regSetDwordValue("vsync", vsync);
  regSetDwordValue("synchronize", synchronize);
  regSetDwordValue("stretch", fullScreenStretch);

  regSetDwordValue("video", videoOption);

  regSetDwordValue("defaultVideoDriver", pVideoDriverGUID == NULL);

  if(pVideoDriverGUID) {
    regSetBinaryValue("videoDriverGUID", (char *)&videoDriverGUID,
                      sizeof(GUID));
  }


  regSetDwordValue("fsAdapter", fsAdapter);
  regSetDwordValue("fsWidth", fsWidth);
  regSetDwordValue("fsHeight", fsHeight);
  regSetDwordValue("fsColorDepth", fsColorDepth);
  regSetDwordValue("fsFrequency", fsFrequency);

  regSetDwordValue("renderMethod", renderMethod);

  regSetDwordValue("windowX", windowPositionX);
  regSetDwordValue("windowY", windowPositionY);

  regSetDwordValue("useBios", useBiosFile);

  regSetDwordValue("skipBios", skipBiosFile);

  if(!biosFileName.IsEmpty())
    regSetStringValue("biosFile", biosFileName);
  
  regSetDwordValue("soundEnable", soundGetEnable() & 0x30f);

  regSetDwordValue("soundOff", soundOffFlag);

  regSetDwordValue("soundQuality", soundQuality);

  regSetDwordValue("soundEcho", soundEcho);

  regSetDwordValue("soundLowPass", soundLowPass);

  regSetDwordValue("soundReverse", soundReverse);

  regSetDwordValue("soundVolume", soundVolume);

  regSetDwordValue("ddrawEmulationOnly", ddrawEmulationOnly);
  regSetDwordValue("ddrawUseVideoMemory", ddrawUseVideoMemory);
  regSetDwordValue("tripleBuffering", tripleBuffering);

  regSetDwordValue("d3dFilter", d3dFilter);
  regSetDwordValue("glFilter", glFilter);
  regSetDwordValue("glType", glType);

  regSetDwordValue("filter", filterType);

  regSetDwordValue("disableMMX", disableMMX);

  regSetDwordValue("disableStatus", disableStatusMessage);

  regSetDwordValue("showSpeed", showSpeed);

  regSetDwordValue("showSpeedTransparent", showSpeedTransparent);

  regSetDwordValue("gbPrinter", winGbPrinterEnabled);

  regSetDwordValue("pauseWhenInactive", pauseWhenInactive);

  regSetDwordValue("useOldSync", useOldSync);

  regSetDwordValue("captureFormat", captureFormat);

  regSetDwordValue("removeIntros", removeIntros);

  regSetDwordValue("recentFreeze", recentFreeze);

  regSetDwordValue("autoIPS", autoIPS);

  regSetDwordValue("disableSfx", cpuDisableSfx);
  
  regSetDwordValue("saveType", winSaveType);
  
  regSetDwordValue("ifbType", ifbType);

  regSetDwordValue("flashSize", winFlashSize);

  regSetDwordValue("agbPrint", agbPrintIsEnabled());

  regSetDwordValue("rtcEnabled", winRtcEnable);

  regSetDwordValue("autoHideMenu", autoHideMenu);

  regSetDwordValue("skinEnabled", skinEnabled);

  regSetStringValue("skinName", skinName);

  regSetDwordValue("borderOn", winGbBorderOn);
  regSetDwordValue("borderAutomatic", gbBorderAutomatic);
  regSetDwordValue("emulatorType", gbEmulatorType);
  regSetDwordValue("colorOption", gbColorOption);

  regSetDwordValue("priority", threadPriority);

  regSetDwordValue("autoSaveCheatList", autoSaveLoadCheatList);

  regSetDwordValue("gbPaletteOption", gbPaletteOption);

  regSetBinaryValue("gbPalette", (char *)systemGbPalette,
                    24*sizeof(u16));

  regSetDwordValue("rewindTimer", rewindTimer/6);

  CString buffer;
  for(int i = 0; i < 10; i++) {
    buffer.Format("recent%d", i);
    regSetStringValue(buffer, recentFiles[i]);
  }

  regSetDwordValue("joypadDefault", joypadDefault);
  regSetDwordValue("autoLoadMostRecent", autoLoadMostRecent);
  regSetDwordValue("cheatsEnabled", cheatsEnabled);
  regSetDwordValue("fsMaxScale", fsMaxScale);
  regSetDwordValue("throttle", throttle);

  regSetDwordValue("saveMoreCPU", Sm60FPS::bSaveMoreCPU);
}

void winSignal(int, int)
{
}

#define CPUReadByteQuick(addr) \
  map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]

void winOutput(char *s, u32 addr)
{
  if(s) {
    toolsLog(s);
  } else {
    CString str;
    char c;

    c = CPUReadByteQuick(addr);
    addr++;
    while(c) {
      str += c;
      c = CPUReadByteQuick(addr);
      addr++;
    }
    toolsLog(str);
  }  
}


void Sm60FPS_Init()
{
	Sm60FPS::dwTimeElapse = 0;
	Sm60FPS::fWantFPS = 60.f;
	Sm60FPS::fCurFPS = 0.f;
	Sm60FPS::nFrameCnt = 0;
	Sm60FPS::bLastSkip = false;
	Sm60FPS::nCurSpeed = 100;
}


bool Sm60FPS_CanSkipFrame()
{
  if( theApp.autoFrameSkip ) {
	  if( Sm60FPS::nFrameCnt == 0 ) {
		  Sm60FPS::nFrameCnt = 0;
		  Sm60FPS::dwTimeElapse = 0;
		  Sm60FPS::dwTime0 = timeGetTime();
	  } else {
		  if( Sm60FPS::nFrameCnt >= 10 ) {
			  Sm60FPS::nFrameCnt = 0;
			  Sm60FPS::dwTimeElapse = 0;

			  if( Sm60FPS::nCurSpeed > Sm60FPS::K_fCpuSpeed ) {
				  Sm60FPS::fWantFPS += 1;
				  if( Sm60FPS::fWantFPS > Sm60FPS::K_fTargetFps ){
					  Sm60FPS::fWantFPS = Sm60FPS::K_fTargetFps;
				  }
			  } else {
				  if( Sm60FPS::nCurSpeed < (Sm60FPS::K_fCpuSpeed - 5) ) {
					  Sm60FPS::fWantFPS -= 1;
					  if( Sm60FPS::fWantFPS < 30.f ) {
						  Sm60FPS::fWantFPS = 30.f;
					  }
				  }
			  }
		  } else { // between frame 1-10
			  Sm60FPS::dwTime1 = timeGetTime();
			  Sm60FPS::dwTimeElapse += (Sm60FPS::dwTime1 - Sm60FPS::dwTime0);
			  Sm60FPS::dwTime0 = Sm60FPS::dwTime1;
			  if( !Sm60FPS::bLastSkip &&
				  ( (Sm60FPS::fWantFPS < Sm60FPS::K_fTargetFps) || Sm60FPS::bSaveMoreCPU) ) {
					  Sm60FPS::fCurFPS = (float)Sm60FPS::nFrameCnt * 1000 / Sm60FPS::dwTimeElapse;
					  if( (Sm60FPS::fCurFPS < Sm60FPS::K_fTargetFps) || Sm60FPS::bSaveMoreCPU ) {
						  Sm60FPS::bLastSkip = true;
						  Sm60FPS::nFrameCnt++;
						  return true;
					  }
			  }
		  }
	  }
	  Sm60FPS::bLastSkip = false;
	  Sm60FPS::nFrameCnt++;
  }	
  return false;
}


void Sm60FPS_Sleep()
{
	if( theApp.autoFrameSkip ) {
		u32 dwTimePass = Sm60FPS::dwTimeElapse + (timeGetTime() - Sm60FPS::dwTime0);
		u32 dwTimeShould = (u32)(Sm60FPS::nFrameCnt * Sm60FPS::K_fDT);
		if( dwTimeShould > dwTimePass ) {
			Sleep(dwTimeShould - dwTimePass);
		}
	}
}
