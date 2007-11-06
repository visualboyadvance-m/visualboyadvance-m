// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team

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

#include "GBA.h"

#ifdef BKPT_SUPPORT
int  oldreg[17];
char oldbuffer[10];
#endif

reg_pair reg[45];
memoryMap map[256];
bool ioReadable[0x400];
bool N_FLAG = 0;
bool C_FLAG = 0;
bool Z_FLAG = 0;
bool V_FLAG = 0;
bool armState = true;
bool armIrqEnable = true;
u32 armNextPC = 0x00000000;
int armMode = 0x1f;
u32 stop = 0x08000568;
int saveType = 0;
bool useBios = false;
bool skipBios = false;
int frameSkip = 1;
bool speedup = false;
bool synchronize = true;
bool cpuDisableSfx = false;
bool cpuIsMultiBoot = false;
bool parseDebug = true;
int layerSettings = 0xff00;
int layerEnable = 0xff00;
bool speedHack = false;
int cpuSaveType = 0;
bool cheatsEnabled = true;
bool mirroringEnable = false;

u8 *bios = NULL;
u8 *rom = NULL;
u8 *internalRAM = NULL;
u8 *workRAM = NULL;
u8 *paletteRAM = NULL;
u8 *vram = NULL;
u8 *pix = NULL;
u8 *oam = NULL;
u8 *ioMem = NULL;

u16 DISPCNT  = 0x0080;
u16 DISPSTAT = 0x0000;
u16 VCOUNT   = 0x0000;
u16 BG0CNT   = 0x0000;
u16 BG1CNT   = 0x0000;
u16 BG2CNT   = 0x0000;
u16 BG3CNT   = 0x0000;
u16 BG0HOFS  = 0x0000;
u16 BG0VOFS  = 0x0000;
u16 BG1HOFS  = 0x0000;
u16 BG1VOFS  = 0x0000;
u16 BG2HOFS  = 0x0000;
u16 BG2VOFS  = 0x0000;
u16 BG3HOFS  = 0x0000;
u16 BG3VOFS  = 0x0000;
u16 BG2PA    = 0x0100;
u16 BG2PB    = 0x0000;
u16 BG2PC    = 0x0000;
u16 BG2PD    = 0x0100;
u16 BG2X_L   = 0x0000;
u16 BG2X_H   = 0x0000;
u16 BG2Y_L   = 0x0000;
u16 BG2Y_H   = 0x0000;
u16 BG3PA    = 0x0100;
u16 BG3PB    = 0x0000;
u16 BG3PC    = 0x0000;
u16 BG3PD    = 0x0100;
u16 BG3X_L   = 0x0000;
u16 BG3X_H   = 0x0000;
u16 BG3Y_L   = 0x0000;
u16 BG3Y_H   = 0x0000;
u16 WIN0H    = 0x0000;
u16 WIN1H    = 0x0000;
u16 WIN0V    = 0x0000;
u16 WIN1V    = 0x0000;
u16 WININ    = 0x0000;
u16 WINOUT   = 0x0000;
u16 MOSAIC   = 0x0000;
u16 BLDMOD   = 0x0000;
u16 COLEV    = 0x0000;
u16 COLY     = 0x0000;
u16 DM0SAD_L = 0x0000;
u16 DM0SAD_H = 0x0000;
u16 DM0DAD_L = 0x0000;
u16 DM0DAD_H = 0x0000;
u16 DM0CNT_L = 0x0000;
u16 DM0CNT_H = 0x0000;
u16 DM1SAD_L = 0x0000;
u16 DM1SAD_H = 0x0000;
u16 DM1DAD_L = 0x0000;
u16 DM1DAD_H = 0x0000;
u16 DM1CNT_L = 0x0000;
u16 DM1CNT_H = 0x0000;
u16 DM2SAD_L = 0x0000;
u16 DM2SAD_H = 0x0000;
u16 DM2DAD_L = 0x0000;
u16 DM2DAD_H = 0x0000;
u16 DM2CNT_L = 0x0000;
u16 DM2CNT_H = 0x0000;
u16 DM3SAD_L = 0x0000;
u16 DM3SAD_H = 0x0000;
u16 DM3DAD_L = 0x0000;
u16 DM3DAD_H = 0x0000;
u16 DM3CNT_L = 0x0000;
u16 DM3CNT_H = 0x0000;
u16 TM0D     = 0x0000;
u16 TM0CNT   = 0x0000;
u16 TM1D     = 0x0000;
u16 TM1CNT   = 0x0000;
u16 TM2D     = 0x0000;
u16 TM2CNT   = 0x0000;
u16 TM3D     = 0x0000;
u16 TM3CNT   = 0x0000;
u16 P1       = 0xFFFF;
u16 IE       = 0x0000;
u16 IF       = 0x0000;
u16 IME      = 0x0000;
