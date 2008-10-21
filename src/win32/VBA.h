// -*- C++ -*-
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

// VBA.h : main header file for the VBA application
//

#pragma once

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#include "stdafx.h"
#include "resource.h"

#include "AcceleratorManager.h"
#include "Display.h"
#include "Input.h"
#include "IUpdate.h"
#include "Sound.h"
#include "../System.h"
#include "../Util.h"

/////////////////////////////////////////////////////////////////////////////
// VBA:
// See VBA.cpp for the implementation of this class
//

enum VIDEO_SIZE{
  VIDEO_1X, VIDEO_2X, VIDEO_3X, VIDEO_4X,
  VIDEO_320x240, VIDEO_640x480, VIDEO_800x600, VIDEO_1024x768, VIDEO_1280x1024,
  VIDEO_OTHER
};

enum pixelFilterType
{
	FILTER_NONE,

	FILTER_SIMPLE2X, FILTER_PIXELATE, FILTER_TVMODE, FILTER_SCANLINES,
    FILTER_PLUGIN,
	FILTER_BILINEAR, FILTER_BILINEARPLUS, FILTER_MAMESCALE2X,
	FILTER_2XSAI, FILTER_SUPER2XSAI, FILTER_SUPEREAGLE, FILTER_LQ2X, FILTER_HQ2X,

	FILTER_SIMPLE3X, FILTER_HQ3X,

	FILTER_SIMPLE4X, FILTER_HQ4X
};

#define REWIND_SIZE 400000

#define DEFAULT_BATTERY_DIR    ".\\battery"
#define DEFAULT_SAVESTATES_DIR ".\\savestates"
#define DEFAULT_SCREENSHOTS_DIR ".\\screenshots"

class AVIWrite;
class WavWriter;

typedef DWORD (WINAPI* lpGetModuleFileNameA)(HINSTANCE, LPSTR, DWORD);
typedef VOID (WINAPI* lpExitProcess)(UINT);

class VBA : public CWinApp
{
 public:
  CMenu m_menu;
  HMENU menu;
  HMENU popup;
  unsigned int maxCpuCores; // maximum number of CPU cores VBA should use, 0 means auto-detect
  int windowPositionX;
  int windowPositionY;
  void (*filterFunction)(u8*,u32,u8*,u8*,u32,int,int);
  void (*ifbFunction)(u8*,u32,int,int);
  int ifbType;
  int filterType;
  char pluginName[MAX_PATH];
  int filterWidth;
  int filterHeight;
  int filterMagnification;
  int filterLCD;
  bool filterMT; // enable multi-threading for pixel filters
  int fsWidth;
  int fsHeight;
  int fsColorDepth;
  int fsFrequency;
  int fsAdapter;
  bool fsForceChange;
  int sizeX;
  int sizeY;
  int surfaceSizeX;
  int surfaceSizeY;
  int videoOption;
  bool fullScreenStretch;
  bool disableStatusMessage;
  int showSpeed;
  BOOL showSpeedTransparent;
  int showRenderedFrames;
  bool screenMessage;
  CString screenMessageBuffer;
  DWORD screenMessageTime;
  u8 *delta[257*244*4];
  IDisplay *display;
  IMAGE_TYPE cartridgeType;
  bool soundInitialized;
  bool useBiosFileGBA;
  bool useBiosFileGB;
  bool skipBiosFile;
  CString biosFileNameGBA;
  CString biosFileNameGB;
  bool active;
  bool paused;
  CString recentFiles[10];
  bool recentFreeze;
  bool autoSaveLoadCheatList;
  FILE *winout;
  bool removeIntros;
  bool autoPatch;
  int winGbBorderOn;
  int winFlashSize;
  bool winRtcEnable;
  int winSaveType;
  char *rewindMemory;
  int rewindPos;
  int rewindTopPos;
  int rewindCounter;
  int rewindCount;
  bool rewindSaveNeeded;
  int rewindTimer;
  int captureFormat;
  bool tripleBuffering;
  unsigned short throttle;
  u32 autoFrameSkipLastTime;
  bool autoFrameSkip;
  bool vsync;
  bool changingVideoSize;
  DISPLAY_TYPE renderMethod;
  AUDIO_API audioAPI;
#ifndef NO_OAL
  TCHAR *oalDevice;
  int oalBufferCount;
#endif
#ifndef NO_XAUDIO2
  UINT32 xa2Device;
  UINT32 xa2BufferCount;
  bool xa2Upmixing;
#endif
#ifndef NO_D3D
  int d3dFilter;
  bool d3dMotionBlur;
#endif
  bool iconic;
  int glFilter;
  bool dinputKeyFocus;
  bool speedupToggle;
  bool winGbPrinterEnabled;
  int threadPriority;
  bool disableMMX;
  int languageOption;
  CString languageName;
  HMODULE languageModule;
  int renderedFrames;
  Input *input;
  int joypadDefault;
  int autoFire;
  bool autoFireToggle;
  bool winPauseNextFrame;
  bool soundRecording;
  WavWriter *soundRecorder;
  CString soundRecordName;
  bool dsoundDisableHardwareAcceleration;
  ISound *sound;
  bool aviRecording;
  AVIWrite *aviRecorder;
  CString aviRecordName;
  bool painting;
  unsigned int skipAudioFrames;
  bool movieRecording;
  bool moviePlaying;
  int movieFrame;
  int moviePlayFrame;
  FILE *movieFile;
  u32 movieLastJoypad;
  u32 movieNextJoypad;
  int sensorX;
  int sensorY;
  int mouseCounter;
  bool wasPaused;
  int frameskipadjust;
  bool autoLoadMostRecent;
  bool winSkipSaveGameBattery;
  int maxScale;
  int romSize;
  VIDEO_SIZE lastWindowed;
  VIDEO_SIZE lastFullscreen;

  CList<IUpdateListener *, IUpdateListener*&> updateList;
  int updateCount;

  CAcceleratorManager winAccelMgr;
  HACCEL hAccel;

  RECT rect;
  RECT dest;

  struct EmulatedSystem emulator;

  CString szFile;
  CString filename;
  CString dir;

  CString wndClass;

  lpGetModuleFileNameA pGetModuleFileNameA;
  lpExitProcess pExitProcess;

  int securityCheck;
  int securityCheck2;

 public:
  VBA();
  ~VBA();

  void adjustDestRect();
  void updateIFB();
  void updateFilter();
  void updateThrottle( unsigned short throttle );
  void updateMenuBar();
  void winAddUpdateListener(IUpdateListener *l);
  void winRemoveUpdateListener(IUpdateListener *l);
  CString winLoadFilter(UINT id);

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(VBA)
 public:
  virtual BOOL InitInstance();
  virtual BOOL OnIdle(LONG lCount);
  //}}AFX_VIRTUAL

  // Implementation

 public:
  void saveSettings();
  void movieReadNext();
  bool initInput();
  HMODULE winLoadLanguage(const char *name);
  void winSetLanguageOption(int option, bool force);
#ifdef MMX
  bool detectMMX();
#endif
  void updatePriority();
  void directXMessage(const char *msg);
  void shutdownDisplay();
  bool preInitialize();
  bool updateRenderMethod0(bool force);
  bool updateRenderMethod(bool force);
  bool initDisplay();
  void updateWindowSize(int value);
  void updateVideoSize(UINT id);
  void updateFrameSkip();
  void loadSettings();
  void addRecentFile(CString file);

  bool trap(bool);

  private:
  unsigned int detectCpuCores();
  int doProtection();
};

extern VBA theApp;
extern int emulating;

#ifdef MMX
    extern "C" bool cpu_mmx;
#endif

void treatRelativePath( CString & path );
