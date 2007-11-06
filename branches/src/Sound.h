// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2004-2006 VBA development team

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

#define NR10 0x60
#define NR11 0x62
#define NR12 0x63
#define NR13 0x64
#define NR14 0x65
#define NR21 0x68
#define NR22 0x69
#define NR23 0x6c
#define NR24 0x6d
#define NR30 0x70
#define NR31 0x72
#define NR32 0x73
#define NR33 0x74
#define NR34 0x75
#define NR41 0x78
#define NR42 0x79
#define NR43 0x7c
#define NR44 0x7d
#define NR50 0x80
#define NR51 0x81
#define NR52 0x84
#define SGCNT0_H 0x82
#define FIFOA_L 0xa0
#define FIFOA_H 0xa2
#define FIFOB_L 0xa4
#define FIFOB_H 0xa6

void soundTick();
void soundShutdown();
bool soundInit();
void soundPause();
void soundResume();
void soundEnable(int);
void soundDisable(int);
int  soundGetEnable();
void soundReset();
void soundSaveGame(gzFile);
void soundReadGame(gzFile, int);
void soundEvent(u32, u8);
void soundEvent(u32, u16);
void soundTimerOverflow(int);
void soundSetQuality(int);

extern int SOUND_CLOCK_TICKS;
extern int soundTicks;
extern int soundPaused;
extern bool soundOffFlag;
extern int soundQuality;
extern int soundBufferLen;
extern int soundBufferTotalLen;
extern u32 soundNextPosition;
extern u16 soundFinalWave[1470];
extern int soundVolume;

extern bool soundEcho;
extern bool soundLowPass;
extern bool soundReverse;

#endif // VBA_SOUND_H
