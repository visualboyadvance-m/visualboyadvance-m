// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004-2006 Forgotten and the VBA development team

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

#ifndef GBA_CHEATS_H
#define GBA_CHEATS_H

struct CheatsData {
  int code;
  int size;
  int status;
  bool enabled;
  u32 rawaddress;
  u32 address;
  u32 value;
  u32 oldValue;
  char codestring[20];
  char desc[32];
};

extern void cheatsAdd(const char *,const char *,u32, u32,u32,int,int);
extern void cheatsAddCheatCode(const char *code, const char *desc);
extern void cheatsAddGSACode(const char *code, const char *desc, bool v3);
extern void cheatsAddCBACode(const char *code, const char *desc);
extern bool cheatsImportGSACodeFile(const char *name, int game, bool v3);
extern void cheatsDelete(int number, bool restore);
extern void cheatsDeleteAll(bool restore);
extern void cheatsEnable(int number);
extern void cheatsDisable(int number);
extern void cheatsSaveGame(gzFile file);
extern void cheatsReadGame(gzFile file, int version);
extern void cheatsSaveCheatList(const char *file);
extern bool cheatsLoadCheatList(const char *file);
extern void cheatsWriteMemory(u32, u32);
extern void cheatsWriteHalfWord(u32, u16);
extern void cheatsWriteByte(u32, u8);
extern int cheatsCheckKeys(u32,u32);
extern int cheatsNumber;
extern CheatsData cheatsList[100];
#endif // GBA_CHEATS_H
