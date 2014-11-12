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
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "../sdl/inputSDL.h"
#include "../gba/Sound.h"
#include "../common/SoundSDL.h"

#include "window.h"
#include "intl.h"

// Required vars, used by the emulator core
//
int  systemRedShift;
int  systemGreenShift;
int  systemBlueShift;
int  systemColorDepth;
int  systemVerbose;
int  systemSaveUpdateCounter;
int  systemFrameSkip;
u32  systemColorMap32[0x10000];
u16  systemColorMap16[0x10000];
u16  systemGbPalette[24];

int  emulating;
int  RGB_LOW_BITS_MASK;

inline VBA::Window * GUI()
{
  return VBA::Window::poGetInstance();
}

void systemMessage(int _iId, const char * _csFormat, ...)
{
  va_list args;
  va_start(args, _csFormat);

  GUI()->vPopupErrorV(_(_csFormat), args);

  va_end(args);
}

void systemDrawScreen()
{
  GUI()->vDrawScreen();
}

bool systemReadJoypads()
{
  return true;
}

u32 systemReadJoypad(int joy)
{
  return inputReadJoypad(joy);
}

void systemShowSpeed(int _iSpeed)
{
  GUI()->vShowSpeed(_iSpeed);
}

void system10Frames(int _iRate)
{
  GUI()->vComputeFrameskip(_iRate);
}

void systemFrame()
{
}

void systemSetTitle(const char * _csTitle)
{
  GUI()->set_title(_csTitle);
}

void systemScreenCapture(int _iNum)
{
  GUI()->vCaptureScreen(_iNum);
}

void systemSaveOldest()
{
  GUI()->vOnSaveGameOldest();
}

void systemLoadRecent()
{
  GUI()->vOnLoadGameMostRecent();
}

u32 systemGetClock()
{
    Glib::TimeVal time;
    time.assign_current_time();
    return time.as_double() * 1000;
}

void systemUpdateMotionSensor()
{
}

int systemGetSensorX()
{
  return 0;
}

int systemGetSensorY()
{
  return 0;
}

void systemGbPrint(u8 * _puiData,
		   int  _iLen,
                   int  _iPages,
                   int  _iFeed,
                   int  _iPalette,
                   int  _iContrast)
{
}

void systemScreenMessage(const char * _csMsg)
{
}

bool systemCanChangeSoundQuality()
{
  return true;
}

bool systemPauseOnFrame()
{
  return false;
}

void systemGbBorderOn()
{
}

SoundDriver * systemSoundInit()
{
	soundShutdown();

	return new SoundSDL();
}

void systemOnSoundShutdown()
{
}

void systemOnWriteDataToSoundBuffer(const u16 * finalWave, int length)
{
}

void debuggerMain()
{
}

void debuggerSignal(int, int)
{
}

void debuggerOutput(const char *, u32)
{
}

void debuggerBreakOnWrite(u32 address, u32 oldvalue, u32 value, int size, int t)
{
}

void (*dbgMain)() = debuggerMain;
void (*dbgSignal)(int, int) = debuggerSignal;
void (*dbgOutput)(const char *, u32) = debuggerOutput;

void log(const char *defaultMsg, ...)
{
  static FILE *out = NULL;

  if(out == NULL) {
    out = fopen("trace.log","w");
  }

  va_list valist;

  va_start(valist, defaultMsg);
  vfprintf(out, defaultMsg, valist);
  va_end(valist);
}
