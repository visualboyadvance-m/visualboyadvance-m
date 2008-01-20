// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team
// Copyright (C) 2007-2008 VBA-M development team

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

#ifdef NO_D3D
#ifdef NO_OGL
#error NO_D3D and NO_OGL must not be defined at the same time.
#endif
#endif

#include "stdafx.h"

#include "AVIWrite.h"
#include "LangSelect.h"
#include "MainWnd.h"
#include "Reg.h"
#include "resource.h"
#include "WavWriter.h"
#include "WinResUtil.h"
#include "Logging.h"
#include "rpi.h"

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

/* Link
---------------------*/
#include "../Link.h"
/* ---------------- */

#include "../gbafilter.h"

#ifdef SDL
#pragma comment( lib, "SDL" )
#pragma comment( lib, "SDLmain" )
#endif

extern void Pixelate(u8*,u32,u8*,u8*,u32,int,int);
extern void Pixelate32(u8*,u32,u8*,u8*,u32,int,int);
extern void _2xSaI(u8*,u32,u8*,u8*,u32,int,int);
extern void _2xSaI32(u8*,u32,u8*,u8*,u32,int,int);
extern void Super2xSaI(u8*,u32,u8*,u8*,u32,int,int);
extern void Super2xSaI32(u8*,u32,u8*,u8*,u32,int,int);
extern void SuperEagle(u8*,u32,u8*,u8*,u32,int,int);
extern void SuperEagle32(u8*,u32,u8*,u8*,u32,int,int);
extern void AdMame2x(u8*,u32,u8*,u8*,u32,int,int);
extern void AdMame2x32(u8*,u32,u8*,u8*,u32,int,int);
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
extern void Simple2x16(u8*,u32,u8*,u8*,u32,int,int);
extern void Simple2x32(u8*,u32,u8*,u8*,u32,int,int);
extern void Simple3x16(u8*,u32,u8*,u8*,u32,int,int);
extern void Simple3x32(u8*,u32,u8*,u8*,u32,int,int);
extern void Simple4x16(u8*,u32,u8*,u8*,u32,int,int);
extern void Simple4x32(u8*,u32,u8*,u8*,u32,int,int);
extern void hq3x16(u8*,u32,u8*,u8*,u32,int,int);
extern void hq4x16(u8*,u32,u8*,u8*,u32,int,int);
extern void hq3x32(u8*,u32,u8*,u8*,u32,int,int);
extern void hq4x32(u8*,u32,u8*,u8*,u32,int,int);

extern void SmartIB(u8*,u32,int,int);
extern void SmartIB32(u8*,u32,int,int);
extern void MotionBlurIB(u8*,u32,int,int);
extern void MotionBlurIB32(u8*,u32,int,int);

extern IDisplay *newGDIDisplay();
extern IDisplay *newDirectDrawDisplay();
#ifndef NO_OGL
extern IDisplay *newOpenGLDisplay();
#endif
#ifndef NO_D3D
extern IDisplay *newDirect3DDisplay();
#endif

extern Input *newDirectInput();

extern ISound *newDirectSound();
#ifndef NO_OAL
extern ISound *newOpenAL();
#endif

extern void remoteStubSignal(int, int);
extern void remoteOutput(char *, u32);
extern void remoteStubMain();
extern void remoteSetProtocol(int);
extern void remoteCleanUp();
extern int remoteSocket;

extern void InterframeCleanup();

void winlog(const char *msg, ...);

/* Link
---------------------*/
extern int InitLink(void);
extern void CloseLink(void);
//extern int linkid;
extern char inifile[];
extern FILE *linklogfile;
/* ------------------- */
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int emulating = 0;
bool debugger = false;
int RGB_LOW_BITS_MASK = 0;
bool b16to32Video = false;
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
int realsystemRedShift = 0;
int realsystemBlueShift = 0;
int realsystemGreenShift = 0;
int realsystemColorDepth = 16;
int systemVerbose = 0;
int systemDebug = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
bool soundBufferLow = 0;
void winSignal(int,int);
void winOutput(const char *, u32);

void (*dbgSignal)(int,int) = winSignal;
void (*dbgOutput)(const char *, u32) = winOutput;

#ifdef MMX
extern "C" bool cpu_mmx;
#endif

namespace Sm60FPS
{
  float					K_fCpuSpeed = 100.0f; // was 98.0f before, but why?
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
  mode1024Available = false;
  mode1280Available = false;
  windowPositionX = 0;
  windowPositionY = 0;
  filterFunction = NULL;
  ifbFunction = NULL;
  ifbType = 0;
  filterType = FILTER_NONE;
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
  showSpeed = 0;
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
  useBiosFileGBA = false;
  useBiosFileGB = false;
  skipBiosFile = false;
  biosFileNameGBA = _T("");
  biosFileNameGB = _T("");
  active = true;
  paused = false;
  recentFreeze = false;
  autoSaveLoadCheatList = false;
  winout = NULL;
  removeIntros = false;
  autoIPS = true;
  winGbBorderOn = 0;
  winFlashSize = 0x20000;
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
  throttle = 0;
  autoFrameSkipLastTime = 0;
  autoFrameSkip = false;
  vsync = false;
  changingVideoSize = false;
  renderMethod = DIRECT_3D;
  audioAPI = DIRECTSOUND;
#ifndef NO_OAL
  oalDevice = NULL;
  oalBufferCount = 5;
#endif
  iconic = false;
#ifndef NO_D3D
  d3dFilter = 0;
  d3dMotionBlur = false;
#endif
  glFilter = 0;
  GLSLShaders = 0;
  regEnabled = false;
  pauseWhenInactive = true;
  speedupToggle = false;
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
  painting = false;
  skipAudioFrames = 0;
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
  lastWindowed = VIDEO_3X;
  lastFullscreen = VIDEO_1024x768;

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
  rpiCleanup();
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

  if(rewindMemory)
    free(rewindMemory);

#ifndef NO_OAL
  if( oalDevice ) {
	  free( oalDevice );
  }
#endif
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
                                       MakeInstanceFilename("VBA.ini"));

  systemDebug = GetPrivateProfileInt("config",
                                     "debug",
                                     0,
                                     MakeInstanceFilename("VBA.ini"));

  wndClass = AfxRegisterWndClass(0, LoadCursor(IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH), LoadIcon(IDI_MAINICON));

  char winBuffer[2048];

  GetModuleFileName(NULL, winBuffer, 2048);
  char *p = strrchr(winBuffer, '\\');
  if(p)
    *p = 0;

  if(!InitLink())
	return FALSE;;

  regInit(winBuffer);

  loadSettings();

  if(!openLinkLog())
    return FALSE;

  if(!initInput())
    return FALSE;

  if(!initDisplay()) {
    if(videoOption >= VIDEO_320x240) {
      regSetDwordValue("video", VIDEO_1X);
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

  point.x = 0;
  point.y = 0;

  m_pMainWnd->ClientToScreen(&point);
  dest.top = point.y;
  dest.left = point.x;

  point.x = surfaceSizeX;
  point.y = surfaceSizeY;

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
	// BEGIN hacky ugly code

	// HQ3X asm wants 16 bit input.  When we switch
	// away from 16 bits we need to restore the driver values

	// This hack is also necessary for Kega Fusion filter plugins

	if ( b16to32Video )
	{
		b16to32Video = false;
		systemColorDepth = realsystemColorDepth;
		systemRedShift = realsystemRedShift;
		systemGreenShift = realsystemGreenShift;
		systemBlueShift = realsystemBlueShift;
		utilUpdateSystemColorMaps();
	}
    // END hacky ugly code

    filterWidth = sizeX;
	filterHeight = sizeY;
	filterMagnification = 1;


	if ( videoOption == VIDEO_1X || videoOption == VIDEO_320x240 )
	{
		filterFunction = NULL;
		filterMagnification = 1;
	}
	else
	{
		if ( systemColorDepth == 16 )
		{
		switch(filterType)
		{
		default:
		case FILTER_NONE:
			filterFunction = NULL;
			filterMagnification = 1;
			break;
        case FILTER_PLUGIN:
			if( rpiInit( pluginName ) ) {
				filterFunction = rpiFilter;
				filterMagnification = rpiScaleFactor();
			} else {
				filterType = FILTER_NONE;
				updateFilter();
				return;
			}
		    break;
		case FILTER_TVMODE:
			filterFunction = ScanlinesTV;
			filterMagnification = 2;
			break;
		case FILTER_2XSAI:
			filterFunction = _2xSaI;
			filterMagnification = 2;
			break;
		case FILTER_SUPER2XSAI:
			filterFunction = Super2xSaI;
			filterMagnification = 2;
			break;
		case FILTER_SUPEREAGLE:
			filterFunction = SuperEagle;
			filterMagnification = 2;
			break;
		case FILTER_PIXELATE:
			filterFunction = Pixelate;
			filterMagnification = 2;
			break;
		case FILTER_MAMESCALE2X:
			filterFunction = AdMame2x;
			filterMagnification = 2;
			break;
		case FILTER_SIMPLE2X:
			filterFunction = Simple2x16;
			filterMagnification = 2;
			break;
		case FILTER_BILINEAR:
			filterFunction = Bilinear;
			filterMagnification = 2;
			break;
		case FILTER_BILINEARPLUS:
			filterFunction = BilinearPlus;
			filterMagnification = 2;
			break;
		case FILTER_SCANLINES:
			filterFunction = Scanlines;
			filterMagnification = 2;
			break;
		case FILTER_HQ2X:
			filterFunction = hq2x;
			filterMagnification = 2;
			break;
		case FILTER_LQ2X:
			filterFunction = lq2x;
			filterMagnification = 2;
			break;
		case FILTER_SIMPLE3X:
			filterFunction = Simple3x16;
			filterMagnification = 3;
			break;
		case FILTER_SIMPLE4X:
			filterFunction = Simple4x16;
			filterMagnification = 4;
			break;
		case FILTER_HQ3X:
			filterFunction = hq3x16;
			filterMagnification = 3;
			break;
		case FILTER_HQ4X:
			filterFunction = hq4x16;
			filterMagnification = 4;
			break;
		}
		}

		if ( systemColorDepth == 32 )
		{
			switch(filterType)
			{
			default:
			case FILTER_NONE:
				filterFunction = NULL;
				filterMagnification = 1;
				break;
            case FILTER_PLUGIN:
				if( rpiInit( pluginName ) ) {
					filterFunction = rpiFilter;
					filterMagnification = rpiScaleFactor();
					b16to32Video=true;
				} else {
					filterType = FILTER_NONE;
					updateFilter();
					return;
				}
				break;
			case FILTER_TVMODE:
				filterFunction = ScanlinesTV32;
				filterMagnification = 2;
				break;
			case FILTER_2XSAI:
				filterFunction = _2xSaI32;
				filterMagnification = 2;
				break;
			case FILTER_SUPER2XSAI:
				filterFunction = Super2xSaI32;
				filterMagnification = 2;
				break;
			case FILTER_SUPEREAGLE:
				filterFunction = SuperEagle32;
				filterMagnification = 2;
				break;
			case FILTER_PIXELATE:
				filterFunction = Pixelate32;
				filterMagnification = 2;
				break;
			case FILTER_MAMESCALE2X:
				filterFunction = AdMame2x32;
				filterMagnification = 2;
				break;
			case FILTER_SIMPLE2X:
				filterFunction = Simple2x32;
				filterMagnification = 2;
				break;
			case FILTER_BILINEAR:
				filterFunction = Bilinear32;
				filterMagnification = 2;
				break;
			case FILTER_BILINEARPLUS:
				filterFunction = BilinearPlus32;
				filterMagnification = 2;
				break;
			case FILTER_SCANLINES:
				filterFunction = Scanlines32;
				filterMagnification = 2;
				break;
			case FILTER_HQ2X:
				filterFunction = hq2x32;
				filterMagnification = 2;
				break;
			case FILTER_LQ2X:
				filterFunction = lq2x32;
				filterMagnification = 2;
				break;
			case FILTER_SIMPLE3X:
				filterFunction = Simple3x32;
				filterMagnification = 3;
				break;
			case FILTER_SIMPLE4X:
				filterFunction = Simple4x32;
				filterMagnification = 4;
				break;
			case FILTER_HQ3X:
				filterFunction = hq3x32;
				filterMagnification = 3;
#ifndef NO_ASM
				b16to32Video=true;
#endif
				break;
			case FILTER_HQ4X:
				filterFunction = hq4x32;
				filterMagnification = 4;
#ifndef NO_ASM
				b16to32Video=true;
#endif
				break;
			}
		}
	}

	rect.right = sizeX * filterMagnification;
	rect.bottom = sizeY * filterMagnification;


	if( filterType != FILTER_NONE )
		memset(delta, 0xFF, sizeof(delta));

	if( display )
		display->changeRenderSize(rect.right, rect.bottom);

	if (b16to32Video && systemColorDepth!=16)
	{
		realsystemColorDepth = systemColorDepth;
		systemColorDepth = 16;
		realsystemRedShift = systemRedShift;
		systemRedShift = 11;
		realsystemGreenShift = systemGreenShift;
		systemGreenShift = 6;
		realsystemBlueShift = systemBlueShift;
		systemBlueShift = 0;
		utilUpdateSystemColorMaps();
	}
}


void VBA::updateThrottle( unsigned short throttle )
{
	this->throttle = throttle;

	if( throttle == 0 ) {
		autoFrameSkip = false;
		return;
	} else {
		Sm60FPS::K_fCpuSpeed = (float)throttle;
		Sm60FPS::K_fTargetFps = 60.0f * Sm60FPS::K_fCpuSpeed / 100;
		Sm60FPS::K_fDT = 1000.0f / Sm60FPS::K_fTargetFps;
		autoFrameSkip = true;
		frameSkip = 0;
		systemFrameSkip = 0;
		return;
	}
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

  if( ( videoOption >= VIDEO_320x240 ) ) {
	  return;
  }

  m_menu.Attach(winResLoadMenu(MAKEINTRESOURCE(IDR_MENU)));
  menu = (HMENU)m_menu;

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

  if( theApp.aviRecording ) {
	  if( theApp.painting ) {
		  theApp.skipAudioFrames++;
	  } else {
		  unsigned char *bmp;
		  unsigned short srcPitch = theApp.sizeX * ( systemColorDepth >> 3 ) + 4;
		  switch( systemColorDepth )
		  {
		  case 16:
			  bmp = new unsigned char[ theApp.sizeX * theApp.sizeY * 2 ];
			  cpyImg16bmp( bmp, pix + srcPitch, srcPitch, theApp.sizeX, theApp.sizeY );
			  break;
		  case 32:
			  // use 24 bit colors to reduce video size
			  bmp = new unsigned char[ theApp.sizeX * theApp.sizeY * 3 ];
			  cpyImg32bmp( bmp, pix + srcPitch, srcPitch, theApp.sizeX, theApp.sizeY );
			  break;
		  }
		  if( false == theApp.aviRecorder->AddVideoFrame( bmp ) ) {
			  systemMessage( IDS_AVI_CANNOT_WRITE_VIDEO, "Cannot write video frame to AVI file." );
			  delete theApp.aviRecorder;
			  theApp.aviRecorder = NULL;
			  theApp.aviRecording = false;
		  }
		  delete bmp;
	  }
  }

  if( theApp.ifbFunction ) {
	  theApp.ifbFunction( pix + (theApp.filterWidth * (systemColorDepth>>3)) + 4,
		  (theApp.filterWidth * (systemColorDepth>>3)) + 4,
		  theApp.filterWidth, theApp.filterHeight );
  }

  if(!soundBufferLow)
  {
	  theApp.display->render();
      Sm60FPS_Sleep();
  }
  else
	  soundBufferLow = false;

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
	if( theApp.movieRecording || theApp.moviePlaying ) {
		theApp.movieFrame++;
	}
}


void system10Frames(int rate)
{
	if( theApp.autoFrameSkip )
	{
		u32 time = systemGetClock();
		u32 diff = time - theApp.autoFrameSkipLastTime;
		theApp.autoFrameSkipLastTime = time;
		if( diff ) {
			// countermeasure against div/0 when debugging
			Sm60FPS::nCurSpeed = (1000000/rate)/diff;
		} else {
			Sm60FPS::nCurSpeed = 100;
		}
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
	systemSoundShutdown();
	
	switch( theApp.audioAPI )
	{
	case DIRECTSOUND:
		theApp.sound = newDirectSound();
		break;
#ifndef NO_OAL
	case OPENAL_SOUND:
		theApp.sound = newOpenAL();
		break;
#endif
	}

	return theApp.sound->init();
}


void systemSoundShutdown()
{
	if( theApp.aviRecorder ) {
		delete theApp.aviRecorder;
		theApp.aviRecorder = NULL;
	}
	theApp.aviRecording = false;


	if( theApp.soundRecorder ) {
		delete theApp.soundRecorder;
		theApp.soundRecorder = NULL;
	}
	theApp.soundRecording = false;

	
	if( theApp.sound ) {
		delete theApp.sound;
		theApp.sound = NULL;
	}
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
	if( theApp.soundRecording ) {
		if( theApp.soundRecorder ) {
			theApp.soundRecorder->AddSound( (const u8 *)soundFinalWave, soundBufferLen );
		} else {
			WAVEFORMATEX format;
			format.cbSize = 0;
			format.wFormatTag = WAVE_FORMAT_PCM;
			format.nChannels = 2;
			format.nSamplesPerSec = 44100 / soundQuality;
			format.wBitsPerSample = 16;
			format.nBlockAlign = format.nChannels * ( format.wBitsPerSample >> 3 );
			format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
			theApp.soundRecorder = new WavWriter;
			if( theApp.soundRecorder->Open( theApp.soundRecordName ) ) {
				theApp.soundRecorder->SetFormat( &format );
			}
		}
	}

	if( theApp.aviRecording && theApp.aviRecorder && !soundOffFlag ) {
		if( theApp.skipAudioFrames ) {
			theApp.skipAudioFrames--;
		} else {
			if( false == theApp.aviRecorder->AddAudioFrame( soundFinalWave ) ) {
				systemMessage( IDS_AVI_CANNOT_WRITE_AUDIO, "Cannot write audio frame to AVI file." );
				delete theApp.aviRecorder;
				theApp.aviRecorder = NULL;
				theApp.aviRecording = false;
			}
		}
	}

	if( theApp.sound ) {
		theApp.sound->write();
	}	
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
      if(lanlink.connected&&linkid&&lc.numtransfers==0) lc.CheckConn();

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

  lastFullscreen = (VIDEO_SIZE)regQueryDwordValue("lastFullscreen", VIDEO_1024x768);

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

  vsync = regQueryDwordValue("vsync", false) ? true : false ;
  synchronize = regQueryDwordValue("synchronize", 1) ? true : false;
  fullScreenStretch = regQueryDwordValue("stretch", 0) ? true : false;

  videoOption = regQueryDwordValue("video", VIDEO_3X);

  strcpy(pluginName, regQueryStringValue("pluginName", "Scale2x.rpi"));

  if(videoOption < VIDEO_1X || videoOption > VIDEO_OTHER)
    videoOption = VIDEO_3X;

  fsAdapter = regQueryDwordValue("fsAdapter", 0);
  fsWidth = regQueryDwordValue("fsWidth", 800);
  fsHeight = regQueryDwordValue("fsHeight", 600);
  fsColorDepth = regQueryDwordValue("fsColorDepth", 32);
  fsFrequency = regQueryDwordValue("fsFrequency", 60);

  if(videoOption == VIDEO_OTHER) {
    if(fsWidth < 0 || fsWidth > 4095 || fsHeight < 0 || fsHeight > 4095)
      videoOption = 0;
    if(fsColorDepth != 16 && fsColorDepth != 24 && fsColorDepth != 32)
      videoOption = 0;
  }

  renderMethod = (DISPLAY_TYPE)regQueryDwordValue("renderMethod", DIRECT_3D);
#ifdef NO_OGL
  if( renderMethod == OPENGL ) {
	  renderMethod = DIRECT_3D;
  }
#endif
#ifdef NO_D3D
  if( renderMethod == DIRECT_3D ) {
	  renderMethod = OPENGL;
  }
#endif

  audioAPI = (AUDIO_API)regQueryDwordValue( "audioAPI", DIRECTSOUND );
  if( ( audioAPI != DIRECTSOUND )
#ifndef NO_OAL
	  && ( audioAPI != OPENAL_SOUND )
#endif
	  ) {
		  audioAPI = DIRECTSOUND;
  }

  windowPositionX = regQueryDwordValue("windowX", 0);
  if(windowPositionX < 0)
    windowPositionX = 0;
  windowPositionY = regQueryDwordValue("windowY", 0);
  if(windowPositionY < 0)
    windowPositionY = 0;

  useBiosFileGBA = ( regQueryDwordValue("useBiosGBA", 0) == 1 ) ? true : false;

  useBiosFileGB = ( regQueryDwordValue("useBiosGB", 0) == 1 ) ? true : false;

  skipBiosFile = regQueryDwordValue("skipBios", 0) ? true : false;

  buffer = regQueryStringValue("biosFileGBA", "");

  if(!buffer.IsEmpty()) {
    biosFileNameGBA = buffer;
  }

  buffer = regQueryStringValue("biosFileGB", "");

  if(!buffer.IsEmpty()) {
    biosFileNameGB = buffer;
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

  soundInterpolation = regQueryDwordValue("soundInterpolation", 0);
  if(soundInterpolation < 0 || soundInterpolation > 1)
    soundInterpolation = 0;

  tripleBuffering = regQueryDwordValue("tripleBuffering", false) ? true : false;

#ifndef NO_D3D
  d3dFilter = regQueryDwordValue("d3dFilter", 1);
  if(d3dFilter < 0 || d3dFilter > 1)
    d3dFilter = 1;

  d3dMotionBlur = ( regQueryDwordValue("d3dMotionBlur", 0) == 1 ) ? true : false;
#endif

  glFilter = regQueryDwordValue("glFilter", 1);
  if(glFilter < 0 || glFilter > 1)
    glFilter = 1;

  GLSLShaders = regQueryDwordValue("GLSLShaders", 0);
  if(GLSLShaders < 0 || GLSLShaders > 1)
    GLSLShaders = 0;


  filterType = regQueryDwordValue("filter", 0);
  if(filterType < 0 || filterType > 17)
    filterType = 0;

  disableMMX = regQueryDwordValue("disableMMX", false) ? true: false;

  disableStatusMessage = regQueryDwordValue("disableStatus", 0) ? true : false;

  showSpeed = regQueryDwordValue("showSpeed", 0);
  if(showSpeed < 0 || showSpeed > 2)
    showSpeed = 0;

  showSpeedTransparent = regQueryDwordValue("showSpeedTransparent", TRUE) ?
    TRUE : FALSE;

  winGbPrinterEnabled = regQueryDwordValue("gbPrinter", false) ? true : false;

  if(winGbPrinterEnabled)
    gbSerialFunction = gbPrinterSend;
  else
    gbSerialFunction = NULL;

  pauseWhenInactive = regQueryDwordValue("pauseWhenInactive", 1) ?
    true : false;

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

  switch(videoOption) {
  case VIDEO_320x240:
    fsWidth = 320;
    fsHeight = 240;
    fsColorDepth = 16;
	fsFrequency = 60;
    break;
  case VIDEO_640x480:
    fsWidth = 640;
    fsHeight = 480;
    fsColorDepth = 16;
	fsFrequency = 60;
	break;
  case VIDEO_800x600:
    fsWidth = 800;
    fsHeight = 600;
    fsColorDepth = 16;
	fsFrequency = 60;
	break;
  case VIDEO_1024x768:
    fsWidth = 1024;
    fsHeight = 768;
    fsColorDepth = 16;
	fsFrequency = 60;
	break;
  case VIDEO_1280x1024:
    fsWidth = 1280;
    fsHeight = 1024;
    fsColorDepth = 16;
	fsFrequency = 60;
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

  cheatsEnabled = regQueryDwordValue("cheatsEnabled", false) ? true : false;

  fsMaxScale = regQueryDwordValue("fsMaxScale", 0);

  updateThrottle( (unsigned short)regQueryDwordValue( "throttle", 0 ) );

   linktimeout = regQueryDwordValue("LinkTimeout", 1000);

  linklog = regQueryDwordValue("Linklog", false) ? true : false;
  if(linklog)
	  openLinkLog();

  adapter = regQueryDwordValue("RFU", false) ? true : false;
  linkenable = regQueryDwordValue("linkEnabled", false) ? true : false;

  lanlink.active = regQueryDwordValue("LAN", 0) ? true : false;

  Sm60FPS::bSaveMoreCPU = regQueryDwordValue("saveMoreCPU", 0);

#ifndef NO_OAL
  buffer = regQueryStringValue( "oalDevice", "Generic Software" );
  if( oalDevice ) {
	  free( oalDevice );
  }
  oalDevice = (TCHAR*)malloc( ( buffer.GetLength() + 1 ) * sizeof( TCHAR ) );
  _tcscpy( oalDevice, buffer.GetBuffer() );

  oalBufferCount = regQueryDwordValue( "oalBufferCount", 5 );
#endif
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
    fsColorDepth = 16;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN640X480:
    value = VIDEO_640x480;
    fsWidth = 640;
    fsHeight = 480;
    fsColorDepth = 16;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN800X600:
    value = VIDEO_800x600;
    fsWidth = 800;
    fsHeight = 600;
    fsColorDepth = 16;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN1024X768:
    value = VIDEO_1024x768;
    fsWidth = 1024;
    fsHeight = 768;
    fsColorDepth = 32;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN1280X1024:
    value = VIDEO_1280x1024;
    fsWidth = 1280;
    fsHeight = 1024;
    fsColorDepth = 32;
    break;
  case ID_OPTIONS_VIDEO_FULLSCREEN:
    value = VIDEO_OTHER;
    break;
  }

  updateWindowSize(value);
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
	if( videoOption <= VIDEO_4X ) {
		lastWindowed = (VIDEO_SIZE)videoOption; // save for when leaving full screen
	} else {
		lastFullscreen = (VIDEO_SIZE)videoOption; // save for when using quick switch to fullscreen
	}
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
         videoOption == VIDEO_1024x768 ||
         videoOption == VIDEO_1280x1024 ||
         videoOption == VIDEO_OTHER) {
        regSetDwordValue("video", VIDEO_1X);
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
  case VIDEO_1024x768:
  case VIDEO_1280x1024:
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

      m_pMainWnd->SetWindowPos(0, //HWND_TOPMOST,
                               windowPositionX,
                               windowPositionY,
                               winSizeX,
                               winSizeY,
                               SWP_NOMOVE | SWP_SHOWWINDOW);

	  // content of old seperate 'winCheckMenuBarInfo' function:
      MENUBARINFO info;
      info.cbSize = sizeof(MENUBARINFO);
	  theApp.m_pMainWnd->GetMenuBarInfo(OBJID_MENU, 0, &info);
      int menuHeight = GetSystemMetrics(SM_CYMENU); // includes white line
      if((info.rcBar.bottom - info.rcBar.top) > menuHeight)
	  {
        winSizeY += (info.rcBar.bottom - info.rcBar.top) - menuHeight + 1;
        m_pMainWnd->SetWindowPos(
                                        0, //HWND_TOPMOST,
                                        theApp.windowPositionX,
                                        theApp.windowPositionY,
                                        winSizeX,
                                        winSizeY,
                                        SWP_NOMOVE | SWP_SHOWWINDOW);
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


bool VBA::preInitialize()
{
	switch( cartridgeType )
	{
	case IMAGE_GBA:
		sizeX = 240;
		sizeY = 160;
		break;
	case IMAGE_GB:
		if( gbBorderOn ) {
			sizeX = 256;
			sizeY = 224;
		} else {
			sizeX = 160;
			sizeY = 144;
		}
		break;
	}

	switch( videoOption )
	{
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
	case VIDEO_1024x768:
	case VIDEO_1280x1024:
	case VIDEO_OTHER:
		float scaleX = (float)fsWidth / sizeX;
		float scaleY = (float)fsHeight / sizeY;
		float min = ( scaleX < scaleY ) ? scaleX : scaleY;
		if( fullScreenStretch ) {
			surfaceSizeX = fsWidth;
			surfaceSizeY = fsHeight;
		} else {
			surfaceSizeX = (int)( sizeX * min );
			surfaceSizeY = (int)( sizeY * min );
		}
		break;
	}

	rect.left = 0;
	rect.top = 0;
	rect.right = sizeX;
	rect.bottom = sizeY;

	dest.left = 0;
	dest.top = 0;
	dest.right = surfaceSizeX;
	dest.bottom = surfaceSizeY;


	DWORD style = WS_POPUP | WS_VISIBLE;
	DWORD styleEx = 0;

	if( videoOption <= VIDEO_4X ) {
		style |= WS_OVERLAPPEDWINDOW;
	} else {
		styleEx = 0;
	}

	if( videoOption <= VIDEO_4X ) {
		AdjustWindowRectEx( &dest, style, TRUE, styleEx );
	} else {
		AdjustWindowRectEx( &dest, style, FALSE, styleEx );
	}

	int winSizeX = dest.right-dest.left;
	int winSizeY = dest.bottom-dest.top;

	if( videoOption > VIDEO_4X ) {
		winSizeX = fsWidth;
		winSizeY = fsHeight;
	}

	int x = 0, y = 0;

	if( videoOption <= VIDEO_4X ) {
		x = windowPositionX;
		y = windowPositionY;
	}


	// Create a window
	MainWnd *pWnd = new MainWnd;
	m_pMainWnd = pWnd;

	pWnd->CreateEx(
		styleEx,
		wndClass,
		_T("VisualBoyAdvance"),
		style,
		x, y,
		winSizeX, winSizeY,
		NULL,
		0
		);

	if( !((HWND)*pWnd) ) {
		winlog( "Error creating Window %08x\n", GetLastError() );
		return false;
	}
	pWnd->DragAcceptFiles( TRUE );
	updateMenuBar();
	adjustDestRect();

	return true;
}


bool VBA::updateRenderMethod(bool force)
{
	bool ret = true;

	Sm60FPS_Init();

	if( !updateRenderMethod0( force ) ) {
		// fall back to safe configuration
		renderMethod = DIRECT_3D;
		fsAdapter = 0;
		videoOption = VIDEO_1X;
		ret = updateRenderMethod( true );
	}

	return ret;
}


bool VBA::updateRenderMethod0(bool force)
{
  bool initInput = false;
  b16to32Video = false;

  if(display) {
    if(display->getType() != renderMethod || force) {
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
#ifndef NO_OGL
	case OPENGL:
		display = newOpenGLDisplay();
		break;
#endif
#ifndef NO_D3D
    case DIRECT_3D:
		display = newDirect3DDisplay();
		break;
#endif
    }

	if( preInitialize() ) {
		if( display->initialize() ) {
			if( initInput ) {
				if( !this->initInput() ) {
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
	}
	changingVideoSize = false;
  }
  return true;
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

  regSetDwordValue("vsync", vsync);
  regSetDwordValue("synchronize", synchronize);
  regSetDwordValue("stretch", fullScreenStretch);

  regSetDwordValue("video", videoOption);

  regSetDwordValue("fsAdapter", fsAdapter);
  regSetDwordValue("fsWidth", fsWidth);
  regSetDwordValue("fsHeight", fsHeight);
  regSetDwordValue("fsColorDepth", fsColorDepth);
  regSetDwordValue("fsFrequency", fsFrequency);

  regSetDwordValue("renderMethod", renderMethod);
  regSetDwordValue( "audioAPI", audioAPI );

  regSetDwordValue("windowX", windowPositionX);
  regSetDwordValue("windowY", windowPositionY);

  regSetDwordValue("useBiosGBA", useBiosFileGBA);

  regSetDwordValue("useBiosGB", useBiosFileGB);

  regSetDwordValue("skipBios", skipBiosFile);

  if(!biosFileNameGBA.IsEmpty())
    regSetStringValue("biosFileGBA", biosFileNameGBA);

  if(!biosFileNameGB.IsEmpty())
    regSetStringValue("biosFileGB", biosFileNameGB);

  regSetDwordValue("soundEnable", soundGetEnable() & 0x30f);

  regSetDwordValue("soundOff", soundOffFlag);

  regSetDwordValue("soundQuality", soundQuality);

  regSetDwordValue("soundEcho", soundEcho);

  regSetDwordValue("soundLowPass", soundLowPass);

  regSetDwordValue("soundReverse", soundReverse);

  regSetDwordValue("soundVolume", soundVolume);

  regSetDwordValue("soundInterpolation", soundInterpolation);
  regSetDwordValue("tripleBuffering", tripleBuffering);

#ifndef NO_D3D
  regSetDwordValue("d3dFilter", d3dFilter);
  regSetDwordValue("d3dMotionBlur", d3dMotionBlur ? 1 : 0);
#endif

  regSetDwordValue("glFilter", glFilter);
  regSetDwordValue("GLSLShaders", GLSLShaders);

  regSetDwordValue("filter", filterType);

  regSetDwordValue("LCDFilter", filterLCD);

  regSetDwordValue("disableMMX", disableMMX);

  regSetDwordValue("disableStatus", disableStatusMessage);

  regSetDwordValue("showSpeed", showSpeed);

  regSetDwordValue("showSpeedTransparent", showSpeedTransparent);

  regSetDwordValue("gbPrinter", winGbPrinterEnabled);

  regSetDwordValue("pauseWhenInactive", pauseWhenInactive);

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
  regSetStringValue("pluginName", pluginName);
  regSetDwordValue("saveMoreCPU", Sm60FPS::bSaveMoreCPU);
  regSetDwordValue("LinkTimeout", linktimeout);
  regSetDwordValue("Linklog", linklog);
  regSetDwordValue("RFU", adapter);
  regSetDwordValue("linkEnabled", linkenable);
  regSetDwordValue("lastFullscreen", lastFullscreen);

#ifndef NO_OAL
  regSetStringValue( "oalDevice", oalDevice );
  regSetDwordValue( "oalBufferCount", oalBufferCount );
#endif
}

void winSignal(int, int)
{
}

#define CPUReadByteQuick(addr) \
  map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]

void winOutput(const char *s, u32 addr)
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
