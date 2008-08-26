// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2004-2006 VBA development team
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

#ifndef VBA_SOUND_H
#define VBA_SOUND_H

#include "System.h"

#define SGCNT0_H 0x82
#define FIFOA_L 0xa0
#define FIFOA_H 0xa2
#define FIFOB_L 0xa4
#define FIFOB_H 0xa6

extern void (*psoundTickfn)();
extern void soundShutdown();
extern bool soundInit();
extern void soundPause();
extern void soundResume();
extern void soundEnable(int);
extern void soundDisable(int);
extern int  soundGetEnable();
extern void soundReset();
extern void soundSaveGame(gzFile);
extern void soundReadGame(gzFile, int);
extern void soundEvent(u32, u8);
extern void soundEvent(u32, u16);
extern void soundTimerOverflow(int);
extern void soundSetQuality(int);
extern void setsystemSoundOn(bool value);
extern void setsoundPaused(bool value);
extern void interp_rate();

extern int SOUND_CLOCK_TICKS;   // Number of 16.8 MHz clocks between calls to soundTick()
extern int soundTicks;          // Number of 16.8 MHz clocks until soundTick() will be called
extern int soundQuality;        // sample rate = 44100 / soundQuality
extern int soundBufferLen;      // size of sound buffer in BYTES
extern u16 soundFinalWave[1470];// 16-bit SIGNED stereo sample buffer
extern int soundVolume;         // emulator volume code (not linear)

extern int soundInterpolation;  // 1 if PCM should have low-pass filtering
extern float soundFiltering;    // 0.0 = none, 1.0 = max (only if soundInterpolation!=0)

// Not used anymore
extern bool soundLowPass;
extern bool soundReverse;

// Unknown purpose
extern int soundBufferTotalLen;
extern u32 soundNextPosition;
extern bool soundPaused;
extern bool soundOffFlag;

#endif // VBA_SOUND_H
