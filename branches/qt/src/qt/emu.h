// VBA-M, A Nintendo Handheld Console Emulator
// Copyright (C) 2008 VBA-M development team
//
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


#ifndef EMU_H
#define EMU_H


#include "../shared/System.h"


// Global Variables
extern int emulating;
extern int systemFrameSkip;
extern int systemSaveUpdateCounter;
extern bool systemSoundOn;
extern u32 systemColorMap32[0x10000];
extern u16 systemColorMap16[0x10000];
extern u16 systemGbPalette[24];
extern int systemRedShift;
extern int systemBlueShift;
extern int systemGreenShift;
extern int systemColorDepth;
extern int systemDebug;
extern int systemVerbose;


// Global Functions
bool systemReadJoypads();
u32 systemReadJoypad( int which );
void systemUpdateMotionSensor();
int systemGetSensorX();
int systemGetSensorY();
u32 systemGetClock();

void systemMessage( int number, const char *defaultMsg, ... );
void winlog( const char *msg, ... );

void debuggerSignal( int sig,int number );
void debuggerOutput( const char *s, u32 addr );

bool systemPauseOnFrame();
void systemGbBorderOn();
void systemScreenCapture( int captureNumber );
void systemDrawScreen();
void systemShowSpeed( int speed );
void system10Frames( int rate );
void systemFrame();

void systemWriteDataToSoundBuffer();
void systemSoundShutdown();
void systemSoundPause();
void systemSoundResume();
void systemSoundReset();
bool systemSoundInit();
bool systemCanChangeSoundQuality();

void systemGbPrint(u8 *data,
                   int pages,
                   int feed,
                   int palette,
                   int contrast);


#endif // #ifndef EMU_H
