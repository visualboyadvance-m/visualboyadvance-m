// -*- C++ -*-
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

#ifndef VBA_SYSTEM_H
#define VBA_SYSTEM_H

#include "unzip.h"

#ifndef NULL
#define NULL 0
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#ifdef _MSC_VER
typedef unsigned __int64 u64;
#else
typedef unsigned long long u64;
#endif

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

#ifdef _MSC_VER
typedef signed __int64 s64;
#else
typedef signed long long s64;
#endif

struct EmulatedSystem {
  // main emulation function
  void (*emuMain)(int);
  // reset emulator
  void (*emuReset)();
  // clean up memory
  void (*emuCleanUp)();
  // load battery file
  bool (*emuReadBattery)(const char *);
  // write battery file
  bool (*emuWriteBattery)(const char *);
  // load state
  bool (*emuReadState)(const char *);  
  // save state
  bool (*emuWriteState)(const char *);
  // load memory state (rewind)
  bool (*emuReadMemState)(char *, int);
  // write memory state (rewind)
  bool (*emuWriteMemState)(char *, int);
  // write PNG file
  bool (*emuWritePNG)(const char *);
  // write BMP file
  bool (*emuWriteBMP)(const char *);
  // emulator update CPSR (ARM only)
  void (*emuUpdateCPSR)();
  // emulator has debugger
  bool emuHasDebugger;
  // clock ticks to emulate
  int emuCount;
};

extern void log(const char *,...);

extern bool systemPauseOnFrame();
extern void systemGbPrint(u8 *,int,int,int,int);
extern void systemScreenCapture(int);
extern void systemDrawScreen();
// updates the joystick data
extern bool systemReadJoypads();
// return information about the given joystick, -1 for default joystick
extern u32 systemReadJoypad(int);
extern u32 systemGetClock();
extern void systemMessage(int, const char *, ...);
extern void systemSetTitle(const char *);
extern void systemWriteDataToSoundBuffer();
extern void systemSoundShutdown();
extern void systemSoundPause();
extern void systemSoundResume();
extern void systemSoundReset();
extern bool systemSoundInit();
extern void systemScreenMessage(const char *);
extern void systemUpdateMotionSensor();
extern int  systemGetSensorX();
extern int  systemGetSensorY();
extern bool systemCanChangeSoundQuality();
extern void systemShowSpeed(int);
extern void system10Frames(int);
extern void systemFrame();
extern void systemGbBorderOn();

extern void Sm60FPS_Init();
extern bool Sm60FPS_CanSkipFrame();
extern void Sm60FPS_Sleep();
extern void DbgMsg(const char *msg, ...);
extern void winlog(const char *,...);

extern bool systemSoundOn;
extern u16 systemColorMap16[0x10000];
extern u32 systemColorMap32[0x10000];
extern u16 systemGbPalette[24];
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;
extern int systemColorDepth;
extern int systemDebug;
extern int systemVerbose;
extern int systemFrameSkip;
extern int systemSaveUpdateCounter;
extern int systemSpeed;

#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0

#endif //VBA_SYSTEM_H
