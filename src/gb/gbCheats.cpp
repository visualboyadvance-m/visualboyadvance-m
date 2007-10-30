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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "../System.h"
#include "../NLS.h"
#include "../Util.h"

#include "gbCheats.h"
#include "gbGlobals.h"

gbCheat gbCheatList[100];
int gbCheatNumber = 0;
bool gbCheatMap[0x10000];

extern bool cheatsEnabled;

#define GBCHEAT_IS_HEX(a) ( ((a)>='A' && (a) <='F') || ((a) >='0' && (a) <= '9'))
#define GBCHEAT_HEX_VALUE(a) ( (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

void gbCheatUpdateMap()
{
  memset(gbCheatMap, 0, 0x10000);

  for(int i = 0; i < gbCheatNumber; i++) {
    if(gbCheatList[i].enabled)
      gbCheatMap[gbCheatList[i].address] = true;
  }
}

void gbCheatsSaveGame(gzFile gzFile)
{
  utilWriteInt(gzFile, gbCheatNumber);
  if(gbCheatNumber)
    utilGzWrite(gzFile, &gbCheatList[0], sizeof(gbCheat)*gbCheatNumber);
}

void gbCheatsReadGame(gzFile gzFile, int version)
{
  if(version <= 8) {
    int gbGgOn = utilReadInt(gzFile);

    if(gbGgOn) {
      int n = utilReadInt(gzFile);
      gbXxCheat tmpCheat;
      for(int i = 0; i < n; i++) {
        utilGzRead(gzFile,&tmpCheat, sizeof(gbXxCheat));
        gbAddGgCheat(tmpCheat.cheatCode, tmpCheat.cheatDesc);
      }
    }
  
    int gbGsOn = utilReadInt(gzFile);
    
    if(gbGsOn) {
      int n = utilReadInt(gzFile);
      gbXxCheat tmpCheat;
      for(int i = 0; i < n; i++) {
        utilGzRead(gzFile,&tmpCheat, sizeof(gbXxCheat));
        gbAddGsCheat(tmpCheat.cheatCode, tmpCheat.cheatDesc);
      }
    }
  } else {
    gbCheatNumber = utilReadInt(gzFile);

    if(gbCheatNumber) {
      utilGzRead(gzFile, &gbCheatList[0], sizeof(gbCheat)*gbCheatNumber);
    }
  }

  gbCheatUpdateMap();
}

void gbCheatsSaveCheatList(const char *file)
{
  if(gbCheatNumber == 0)
    return;
  FILE *f = fopen(file, "wb");
  if(f == NULL)
    return;
  int version = 1;
  fwrite(&version, 1, sizeof(version), f);
  int type = 1;
  fwrite(&type, 1, sizeof(type), f);
  fwrite(&gbCheatNumber, 1, sizeof(gbCheatNumber), f);
  fwrite(gbCheatList, 1, sizeof(gbCheatList), f);
  fclose(f);
}

bool gbCheatsLoadCheatList(const char *file)
{
  gbCheatNumber = 0;

  gbCheatUpdateMap();
  
  int count = 0;

  FILE *f = fopen(file, "rb");

  if(f == NULL)
    return false;

  int version = 0;

  if(fread(&version, 1, sizeof(version), f) != sizeof(version)) {
    fclose(f);
    return false;
  }
     
  if(version != 1) {
    systemMessage(MSG_UNSUPPORTED_CHEAT_LIST_VERSION,
                  N_("Unsupported cheat list version %d"), version);
    fclose(f);
    return false;
  }

  int type = 0;
  if(fread(&type, 1, sizeof(type), f) != sizeof(type)) {
    fclose(f);
    return false;
  }

  if(type != 1) {
    systemMessage(MSG_UNSUPPORTED_CHEAT_LIST_TYPE,
                  N_("Unsupported cheat list type %d"), type);
    fclose(f);
    return false;
  }
  
  if(fread(&count, 1, sizeof(count), f) != sizeof(count)) {
    fclose(f);
    return false;
  }
  
  if(fread(gbCheatList, 1, sizeof(gbCheatList), f) != sizeof(gbCheatList)) {
    fclose(f);
    return false;
  }

  gbCheatNumber = count;
  gbCheatUpdateMap();
  
  return true;
}

bool gbVerifyGsCode(const char *code)
{
  int len = strlen(code);

  if(len == 0)
    return true;
  
  if(len != 8)
    return false;

  for(int i = 0; i < 8; i++)
    if(!GBCHEAT_IS_HEX(code[i]))
      return false;

  int address = GBCHEAT_HEX_VALUE(code[6]) << 12 |
    GBCHEAT_HEX_VALUE(code[7]) << 8 |
    GBCHEAT_HEX_VALUE(code[4]) << 4 |
    GBCHEAT_HEX_VALUE(code[5]);

  if(address < 0xa000 ||
     address > 0xdfff)
    return false;

  return true;
}

void gbAddGsCheat(const char *code, const char *desc)
{
  if(gbCheatNumber > 99) {
    systemMessage(MSG_MAXIMUM_NUMBER_OF_CHEATS,
                  N_("Maximum number of cheats reached."));
    return;
  }

  if(!gbVerifyGsCode(code)) {
    systemMessage(MSG_INVALID_GAMESHARK_CODE,
                  N_("Invalid GameShark code: %s"), code);
    return;
  }
  
  int i = gbCheatNumber;

  strcpy(gbCheatList[i].cheatCode, code);
  strcpy(gbCheatList[i].cheatDesc, desc);
  
  gbCheatList[i].code = GBCHEAT_HEX_VALUE(code[0]) << 4 |
    GBCHEAT_HEX_VALUE(code[1]);

  gbCheatList[i].value = GBCHEAT_HEX_VALUE(code[2]) << 4 |
    GBCHEAT_HEX_VALUE(code[3]);

  gbCheatList[i].address = GBCHEAT_HEX_VALUE(code[6]) << 12 |
    GBCHEAT_HEX_VALUE(code[7]) << 8 |
    GBCHEAT_HEX_VALUE(code[4]) << 4 |
    GBCHEAT_HEX_VALUE(code[5]);

  gbCheatList[i].compare = 0;

  gbCheatList[i].enabled = true;
  
  gbCheatMap[gbCheatList[i].address] = true;
  
  gbCheatNumber++;
}

bool gbVerifyGgCode(const char *code)
{
  int len = strlen(code);

  if(len != 11 &&
     len != 7 &&
     len != 6 &&
     len != 0)
    return false;

  if(len == 0)
    return true;
  
  if(!GBCHEAT_IS_HEX(code[0]))
    return false;
  if(!GBCHEAT_IS_HEX(code[1]))
    return false;
  if(!GBCHEAT_IS_HEX(code[2]))
    return false;
  if(code[3] != '-')
    return false;
  if(!GBCHEAT_IS_HEX(code[4]))
    return false;
  if(!GBCHEAT_IS_HEX(code[5]))
    return false;
  if(!GBCHEAT_IS_HEX(code[6]))
    return false;
  if(code[7] != 0) {
    if(code[7] != '-')
      return false;
    if(code[8] != 0) {
      if(!GBCHEAT_IS_HEX(code[8]))
        return false;
      if(!GBCHEAT_IS_HEX(code[9]))
        return false;
      if(!GBCHEAT_IS_HEX(code[10]))
        return false;
    }
  }

  //  int replace = (GBCHEAT_HEX_VALUE(code[0]) << 4) +
  //    GBCHEAT_HEX_VALUE(code[1]);

  int address = (GBCHEAT_HEX_VALUE(code[2]) << 8) +
    (GBCHEAT_HEX_VALUE(code[4]) << 4) +
    (GBCHEAT_HEX_VALUE(code[5])) +
    ((GBCHEAT_HEX_VALUE(code[6]) ^ 0x0f) << 12);

  if(address >= 0x8000 && address <= 0x9fff)
    return false;

  if(address >= 0xc000)
    return false;

  if(code[7] == 0 || code[8] == '0')
    return true;

  int compare = (GBCHEAT_HEX_VALUE(code[8]) << 4) +
    (GBCHEAT_HEX_VALUE(code[10]));
  compare = compare ^ 0xff;
  compare = (compare >> 2) | ( (compare << 6) & 0xc0);
  compare ^= 0x45;

  int cloak = (GBCHEAT_HEX_VALUE(code[8])) ^ (GBCHEAT_HEX_VALUE(code[9]));
  
  if(cloak >=1 && cloak <= 7)
    return false;

  return true;
}

void gbAddGgCheat(const char *code, const char *desc)
{
  if(gbCheatNumber > 99) {
    systemMessage(MSG_MAXIMUM_NUMBER_OF_CHEATS,
                  N_("Maximum number of cheats reached."));
    return;
  }

  if(!gbVerifyGgCode(code)) {
    systemMessage(MSG_INVALID_GAMEGENIE_CODE,
                  N_("Invalid GameGenie code: %s"), code);
    return;
  }
  
  int i = gbCheatNumber;

  int len = strlen(code);
  
  strcpy(gbCheatList[i].cheatCode, code);
  strcpy(gbCheatList[i].cheatDesc, desc);

  gbCheatList[i].code = 1;
  gbCheatList[i].value = (GBCHEAT_HEX_VALUE(code[0]) << 4) +
    GBCHEAT_HEX_VALUE(code[1]);
  
  gbCheatList[i].address = (GBCHEAT_HEX_VALUE(code[2]) << 8) +
    (GBCHEAT_HEX_VALUE(code[4]) << 4) +
    (GBCHEAT_HEX_VALUE(code[5])) +
    ((GBCHEAT_HEX_VALUE(code[6]) ^ 0x0f) << 12);

  gbCheatList[i].compare = 0;
  
  if(len != 7 && len != 8) {
    
    int compare = (GBCHEAT_HEX_VALUE(code[8]) << 4) +
      (GBCHEAT_HEX_VALUE(code[10]));
    compare = compare ^ 0xff;
    compare = (compare >> 2) | ( (compare << 6) & 0xc0);
    compare ^= 0x45;

    gbCheatList[i].compare = compare;
    gbCheatList[i].code = 0;
  }

  gbCheatList[i].enabled = true;
  
  gbCheatMap[gbCheatList[i].address] = true;
  
  gbCheatNumber++;
}

void gbCheatRemove(int i)
{
  if(i < 0 || i >= gbCheatNumber) {
    systemMessage(MSG_INVALID_CHEAT_TO_REMOVE,
                  N_("Invalid cheat to remove %d"), i);
    return;
  }
  
  if((i+1) <  gbCheatNumber) {
    memcpy(&gbCheatList[i], &gbCheatList[i+1], sizeof(gbCheat)*
           (gbCheatNumber-i-1));
  }
  
  gbCheatNumber--;

  gbCheatUpdateMap();
}

void gbCheatRemoveAll()
{
  gbCheatNumber = 0;
  gbCheatUpdateMap();
}

void gbCheatEnable(int i)
{
  if(i >=0 && i < gbCheatNumber) {
    if(!gbCheatList[i].enabled) {
      gbCheatList[i].enabled = true;
      gbCheatUpdateMap();
    }
  }
}

void gbCheatDisable(int i)
{
  if(i >=0 && i < gbCheatNumber) {
    if(gbCheatList[i].enabled) {
      gbCheatList[i].enabled = false;
      gbCheatUpdateMap();
    }
  }
}

bool gbCheatReadGSCodeFile(const char *fileName)
{
  FILE *file = fopen(fileName, "rb");
    
  if(!file) {
    systemMessage(MSG_CANNOT_OPEN_FILE, N_("Cannot open file %s"), fileName);
    return false;
  }
  
  fseek(file, 0x18, SEEK_SET);
  int count = 0;
  fread(&count, 1, 2, file);
  int dummy = 0;
  gbCheatRemoveAll();
  char desc[13];
  char code[9];
  int i;
  for(i = 0; i < count; i++) {
    fread(&dummy, 1, 2, file);    
    fread(desc, 1, 12, file);
    desc[12] = 0;
    fread(code, 1, 8, file);
    code[8] = 0;
    gbAddGsCheat(code, desc);
  }

  for(i = 0; i < gbCheatNumber; i++)
    gbCheatDisable(i);

  fclose(file);
  return true;
}

u8 gbCheatRead(u16 address)
{
  if(!cheatsEnabled)
    return gbMemoryMap[address>>12][address & 0xFFF];

  for(int i = 0; i < gbCheatNumber; i++) {
    if(gbCheatList[i].enabled && gbCheatList[i].address == address) {
      switch(gbCheatList[i].code) {
      case 0x100: // GameGenie support
        if(gbMemoryMap[address>>12][address&0xFFF] == gbCheatList[i].compare)
          return gbCheatList[i].value;
        break;
      case 0x00:
      case 0x01:
      case 0x80:
        return gbCheatList[i].value;
      case 0x90:
      case 0x91:
      case 0x92:
      case 0x93:
      case 0x94:
      case 0x95:
      case 0x96:
      case 0x97:
        if(address >= 0xd000 && address < 0xe000) {
          if(((gbMemoryMap[0x0d] - gbWram)/0x1000) ==
             (gbCheatList[i].code - 0x90))
            return gbCheatList[i].value;
        } else
          return gbCheatList[i].value;
      }
    }
  }
  return gbMemoryMap[address>>12][address&0xFFF];
}
