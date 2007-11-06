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
//#include <afxtempl.h>

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

enum {
	VIDEO_1X,
	VIDEO_2X,
	VIDEO_3X,
	VIDEO_4X,
	VIDEO_320x240,
	VIDEO_640x480,
	VIDEO_800x600,
	VIDEO_OTHER
};

#define REWIND_SIZE 400000

class CSkin;
class AVIWrite;
class WavWriter;

class VBA : public CWinApp
{
 public:
  CMenu m_menu;
  HMENU menu;
  HMENU popup;
  bool mode320Available;
  bool mode640Available;
  bool mode800Available;
  int windowPositionX;
  int windowPositionY;
  void (*filterFunction)(u8*,u32,u8*,u8*,u32,int,int);
  void (*ifbFunction)(u8*,u32,int,int);
  int ifbType;
  int filterType;
  int filterWidth;
  int filterHeight;
  int fsAdapter;
  int fsWidth;
  int fsHeight;
  int fsColorDepth;
  int fsFrequency;
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
  bool menuToggle;
  IDisplay *display;
  IMAGE_TYPE cartridgeType;
  bool soundInitialized;
  bool useBiosFile;
  bool skipBiosFile;
  CString biosFileName;
  bool active;
  bool paused;
  CString recentFiles[10];
  bool recentFreeze;
  bool autoSaveLoadCheatList;
  FILE *winout;
  bool removeIntros;
  bool autoIPS;
  int winGbBorderOn;
  int winFlashSize;
  bool winRtcEnable;
  bool winGenericflashcardEnable;
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
  bool autoHideMenu;
  int throttle;
  u32 throttleLastTime;
  u32 autoFrameSkipLastTime;
  bool autoFrameSkip;
  bool vsync;
  bool changingVideoSize;
  GUID videoDriverGUID;
  GUID *pVideoDriverGUID;
  DISPLAY_TYPE renderMethod;
  bool iconic;
  bool ddrawEmulationOnly;
  bool ddrawUsingEmulationOnly;
  bool ddrawDebug;
  bool ddrawUseVideoMemory;
  int d3dFilter;
  int glFilter;
  int glType;
  CSkin *skin;
  CString skinName;
  bool skinEnabled;
  int skinButtons;
  bool pauseWhenInactive;
  bool speedupToggle;
  bool useOldSync;
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
  int aviFrameNumber;
  bool painting;
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
  int fsMaxScale;
  int romSize;
 
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

 public:
  VBA();
  ~VBA();

  void adjustDestRect();
  void updateIFB();
  void updateFilter();
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
  void winUpdateSkin();
  void directXMessage(const char *msg);
  void shutdownDisplay();
  void winCheckFullscreen();
  bool updateRenderMethod0(bool force);
  bool updateRenderMethod(bool force);
  bool initDisplay();
  void updateWindowSize(int value);
  void updateVideoSize(UINT id);
  void updateFrameSkip();
  void loadSettings();
  void addRecentFile(CString file);
  //{{AFX_MSG(VBA)
  afx_msg void OnAppAbout();
  // NOTE - the ClassWizard will add and remove member functions here.
  //    DO NOT EDIT what you see in these blocks of generated code !
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    extern VBA theApp;
	extern int emulating;

#ifdef MMX
    extern "C" bool cpu_mmx;
#endif