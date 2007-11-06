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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../AutoBuild.h"

#include "../GBA.h"
#include "debugger.h"
#include "../Sound.h"
#include "../unzip.h"
#include "../Util.h"
#include "../gb/GB.h"
#include "../gb/gbGlobals.h"
#include "../getopt.h"

#ifndef _WIN32
# include <unistd.h>
# define GETCWD getcwd
#else // WIN32
# include <direct.h>
# define GETCWD _getcwd
#endif // WIN32

#ifdef MMX
extern "C" bool cpu_mmx;
#endif
extern bool soundEcho;
extern bool soundLowPass;
extern bool soundReverse;

extern void remoteInit();
extern void remoteCleanUp();
extern void remoteStubMain();
extern void remoteStubSignal(int,int);
extern void remoteOutput(char *, u32);
extern void remoteSetProtocol(int);
extern void remoteSetPort(int);
extern void debuggerOutput(char *, u32);

struct EmulatedSystem emulator;

int systemRedShift = 0;
int systemBlueShift = 16;
int systemGreenShift = 8;
int systemColorDepth = 32;
int systemDebug = 0;
int systemVerbose = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

int cartridgeType = 3;
int captureFormat = 0;

int emulating = 0;
int RGB_LOW_BITS_MASK=0x821;
int systemFrameSkip = 0;
u32 systemColorMap32[0x10000];
u16 systemColorMap16[0x10000];
u16 systemGbPalette[24];
char filename[2048];
char biosFileName[2048];
char captureDir[2048];
char saveDir[2048];
char batteryDir[2048];

bool paused = false;
bool debugger = true;
bool debuggerStub = false;
bool systemSoundOn = false;
bool removeIntros = false;

extern void debuggerSignal(int,int);

void (*dbgMain)() = debuggerMain;
void (*dbgSignal)(int,int) = debuggerSignal;
void (*dbgOutput)(char *, u32) = debuggerOutput;

char *sdlGetFilename(char *name)
{
  static char filebuffer[2048];

  int len = strlen(name);
  
  char *p = name + len - 1;
  
  while(true) {
    if(*p == '/' ||
       *p == '\\') {
      p++;
      break;
    }
    len--;
    p--;
    if(len == 0)
      break;
  }
  
  if(len == 0)
    strcpy(filebuffer, name);
  else
    strcpy(filebuffer, p);
  return filebuffer;
}

void usage(char *cmd)
{
  printf("%s file-name\n",cmd);
}

int main(int argc, char **argv)
{
  fprintf(stderr,"VisualBoyAdvance-Test version %s\n", VERSION);

  captureDir[0] = 0;
  saveDir[0] = 0;
  batteryDir[0] = 0;
  
  char buffer[1024];

  systemFrameSkip = frameSkip = 2;
  gbBorderOn = 0;

  parseDebug = true;

  if(!debuggerStub) {
    if(argc <= 1) {
      systemMessage(0,"Missing image name");
      usage(argv[0]);
      exit(-1);
    }
  }

  for(int i = 0; i < 24;) {
    systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
    systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
    systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
    systemGbPalette[i++] = 0;
  }

  if(argc == 2) {
    char *szFile = argv[optind];
    bool failed = false;
    if(CPUIsZipFile(szFile)) {
      unzFile unz = unzOpen(szFile);
      
      if(unz == NULL) {
        systemMessage(0, "Cannot open file %s", szFile);
        exit(-1);
      }
      int r = unzGoToFirstFile(unz);
      
      if(r != UNZ_OK) {
        unzClose(unz);
        systemMessage(0, "Bad ZIP file %s", szFile);
        exit(-1);
      }
      
      bool found = false;
      
      unz_file_info info;
      
      while(true) {
        r = unzGetCurrentFileInfo(unz,
                                  &info,
                                  buffer,
                                  sizeof(buffer),
                                  NULL,
                                  0,
                                  NULL,
                                  0);
        
        if(r != UNZ_OK) {
          unzClose(unz);
          systemMessage(0,"Bad ZIP file %s", szFile);
          exit(-1);
        }
        
        if(utilIsGBImage(buffer)) {
          found = true;
          cartridgeType = 1;
          break;
        }
        if(utilIsGBAImage(buffer)) {
          found = true;
          cartridgeType = 0;
          break;
        }
        
        r = unzGoToNextFile(unz);
        
        if(r != UNZ_OK)
          break;
      }
      
      if(!found) {
        unzClose(unz);
        systemMessage(0, "No image found on ZIP file %s", szFile);
        exit(-1);
      }
      
      unzClose(unz);
    }
    
    if(utilIsGBImage(szFile) || cartridgeType == 1) {
      failed = !gbLoadRom(szFile);
      cartridgeType = 1;
      emulator = GBSystem;
    } else if(utilIsGBAImage(szFile) || cartridgeType == 0) {
      failed = !CPULoadRom(szFile);
      cartridgeType = 0;
      emulator = GBASystem;

      CPUInit(biosFileName, useBios);
      CPUReset();
    } else {
      systemMessage(0, "Unknown file type %s", szFile);
      exit(-1);
    }
    
    if(failed) {
      systemMessage(0, "Failed to load file %s", szFile);
      exit(-1);
    }
    strcpy(filename, szFile);
    char *p = strrchr(filename, '.');
    
    if(p)
      *p = 0;
  } else {
    cartridgeType = 0;
    strcpy(filename, "gnu_stub");
    rom = (u8 *)malloc(0x2000000);
    workRAM = (u8 *)calloc(1, 0x40000);
    bios = (u8 *)calloc(1,0x4000);
    internalRAM = (u8 *)calloc(1,0x8000);
    paletteRAM = (u8 *)calloc(1,0x400);
    vram = (u8 *)calloc(1, 0x20000);
    oam = (u8 *)calloc(1, 0x400);
    pix = (u8 *)calloc(1, 4 * 241 * 162);
    ioMem = (u8 *)calloc(1, 0x400);

    emulator = GBASystem;
    
    CPUInit(biosFileName, useBios);
    CPUReset();    
  }
  
  if(debuggerStub) 
    remoteInit();
  
  if(cartridgeType == 0) {
  } else if (cartridgeType == 1) {
    if(gbBorderOn) {
      gbBorderLineSkip = 256;
      gbBorderColumnSkip = 48;
      gbBorderRowSkip = 40;
    } else {      
      gbBorderLineSkip = 160;
      gbBorderColumnSkip = 0;
      gbBorderRowSkip = 0;
    }      
  } else {
  }

  for(int i = 0; i < 0x10000; i++) {
    systemColorMap32[i] = ((i & 0x1f) << systemRedShift) |
      (((i & 0x3e0) >> 5) << systemGreenShift) |
      (((i & 0x7c00) >> 10) << systemBlueShift);  
  }

  emulating = 1;
  soundInit();
  
  while(emulating) {
    if(!paused) {
      if(debugger && emulator.emuHasDebugger)
        dbgMain();
      else
        emulator.emuMain(emulator.emuCount);
    }
  }
  emulating = 0;
  fprintf(stderr,"Shutting down\n");
  remoteCleanUp();
  soundShutdown();

  if(gbRom != NULL || rom != NULL) {
    emulator.emuCleanUp();
  }

  return 0;
}

void systemMessage(int num, const char *msg, ...)
{
  char buffer[2048];
  va_list valist;
  
  va_start(valist, msg);
  vsprintf(buffer, msg, valist);
  
  fprintf(stderr, "%s\n", buffer);
  va_end(valist);
}

void systemDrawScreen()
{
}

bool systemReadJoypads()
{
  return true;
}

u32 systemReadJoypad(int)
{
  return 0;
}

void systemShowSpeed(int speed)
{
}

void system10Frames(int rate)
{
}

void systemFrame()
{
}

void systemSetTitle(const char *title)
{
}

void sdlWriteState(int num)
{
  char stateName[2048];

  if(saveDir[0])
    sprintf(stateName, "%s/%s%d.sgm", saveDir, sdlGetFilename(filename),
            num+1);
  else
    sprintf(stateName,"%s%d.sgm", filename, num+1);
  if(emulator.emuWriteState)
    emulator.emuWriteState(stateName);
  sprintf(stateName, "Wrote state %d", num+1);
  systemScreenMessage(stateName);
}

void sdlReadState(int num)
{
  char stateName[2048];

  if(saveDir[0])
    sprintf(stateName, "%s/%s%d.sgm", saveDir, sdlGetFilename(filename),
            num+1);
  else
    sprintf(stateName,"%s%d.sgm", filename, num+1);

  if(emulator.emuReadState)
    emulator.emuReadState(stateName);

  sprintf(stateName, "Loaded state %d", num+1);
  systemScreenMessage(stateName);
}

void systemScreenCapture(int a)
{
  char buffer[2048];

  if(captureFormat) {
    if(captureDir[0])
      sprintf(buffer, "%s/%s%02d.bmp", captureDir, sdlGetFilename(filename), a);
    else
      sprintf(buffer, "%s%02d.bmp", filename, a);

    emulator.emuWriteBMP(buffer);
  } else {
    if(captureDir[0])
      sprintf(buffer, "%s/%s%02d.png", captureDir, sdlGetFilename(filename), a);
    else
      sprintf(buffer, "%s%02d.png", filename, a);
    emulator.emuWritePNG(buffer);
  }

  systemScreenMessage("Screen capture");
}

u32 systemReadJoypadExtended()
{
  return 0;
}

void systemWriteDataToSoundBuffer()
{
}

bool systemSoundInit()
{
  return true;
}

void systemSoundShutdown()
{
}

void systemSoundPause()
{
}

void systemSoundResume()
{
}

void systemSoundReset()
{
}

static int ticks = 0;

u32 systemGetClock()
{
  return ticks++;
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

void systemGbPrint(u8 *data,int pages,int feed,int palette, int contrast)
{
}

void systemScreenMessage(const char *msg)
{
}

bool systemCanChangeSoundQuality()
{
  return false;
}

bool systemPauseOnFrame()
{
  return false;
}

void systemGbBorderOn()
{
}
