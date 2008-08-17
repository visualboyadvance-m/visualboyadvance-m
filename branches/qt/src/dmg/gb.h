// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005-2006 Forgotten and the VBA development team

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

#ifndef VBA_GB_GB_H
#define VBA_GB_GB_H

#include "../shared/System.h"

#define C_FLAG 0x10
#define H_FLAG 0x20
#define N_FLAG 0x40
#define Z_FLAG 0x80

typedef union {
  struct {
#ifdef WORDS_BIGENDIAN
    u8 B1, B0;
#else
    u8 B0,B1;
#endif
  } B;
  u16 W;
} gbRegister;

bool gbLoadRom(const char *);
void gbEmulate(int);
void gbWriteMemory(register u16, register u8);
void gbDrawLine();
bool gbIsGameboyRom(const char *);
void gbSoundReset();
void gbSoundSetQuality(int);
void gbGetHardwareType();
void gbReset();
void gbCleanUp();
void gbCPUInit(const char *,bool);
bool gbWriteBatteryFile(const char *);
bool gbWriteBatteryFile(const char *, bool);
bool gbReadBatteryFile(const char *);
bool gbWriteSaveState(const char *);
bool gbWriteMemSaveState(char *, int);
bool gbReadSaveState(const char *);
bool gbReadMemSaveState(char *, int);
void gbSgbRenderBorder();
bool gbWritePNGFile(const char *);
bool gbWriteBMPFile(const char *);
bool gbReadGSASnapshot(const char *);

extern struct EmulatedSystem GBSystem;

#endif
