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

#ifndef __VBA_GB_GBCHEATS_H
#define __VBA_GB_GBCHEATS_H

#include "../System.h"

struct gbXxCheat {
  char cheatDesc[100];
  char cheatCode[20];
};

struct gbCheat {
  char cheatCode[20];
  char cheatDesc[32];
  u16 address;
  int code;
  u8 compare;
  u8 value;
  bool enabled;
};

void gbCheatsSaveGame(gzFile);
void gbCheatsReadGame(gzFile, int);
void gbCheatsSaveCheatList(const char *);
bool gbCheatsLoadCheatList(const char *);
bool gbCheatReadGSCodeFile(const char *);

bool gbAddGsCheat(const char *, const char*);
bool gbAddGgCheat(const char *, const char*);
void gbCheatRemove(int);
void gbCheatRemoveAll();
void gbCheatEnable(int);
void gbCheatDisable(int);
u8 gbCheatRead(u16);
void gbCheatWrite(bool);
bool gbVerifyGsCode(const char *code);
bool gbVerifyGgCode(const char *code);


extern int gbCheatNumber;
extern gbCheat gbCheatList[100];
extern bool gbCheatMap[0x10000];
#endif

