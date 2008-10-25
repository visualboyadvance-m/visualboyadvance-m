// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team
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

void cheatsAdd(const char *codeStr, const char *desc, u32 rawaddress, u32 address, u32 value, int code, int size);
void cheatsAddCheatCode(const char *code, const char *desc);
void cheatsAddGSACode(const char *code, const char *desc, bool v3);
void cheatsAddCBACode(const char *code, const char *desc);
bool cheatsImportGSACodeFile(const char *name, int game, bool v3);
void cheatsDelete(int number, bool restore);
void cheatsDeleteAll(bool restore);
void cheatsEnable(int number);
void cheatsDisable(int number);
void cheatsSaveGame(gzFile file);
void cheatsReadGame(gzFile file, int version);
void cheatsSaveCheatList(const char *file);
bool cheatsLoadCheatList(const char *file);
void cheatsWriteMemory(u32 address, u32 value);
void cheatsWriteHalfWord(u32 address, u16 value);
void cheatsWriteByte(u32 address, u8 value);
int cheatsCheckKeys(u32 keys, u32 extended);

extern int cheatsNumber;
extern CheatsData cheatsList[100];

#endif // GBA_CHEATS_H
