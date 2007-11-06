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

#include "../GBA.h"

u8 *gbMemoryMap[16];

int gbRomSizeMask = 0;
int gbRomSize = 0;
int gbRamSizeMask = 0;
int gbRamSize = 0;
int gbTAMA5ramSize = 0;

u8 *gbMemory = NULL;
u8 *gbVram = NULL;
u8 *gbRom = NULL;
u8 *gbRam = NULL;
u8 *gbWram = NULL;
u16 *gbLineBuffer = NULL;
u8 *gbTAMA5ram = NULL;

u16 gbPalette[128];
u8 gbBgp[4]  = { 0, 1, 2, 3};
u8 gbObp0[4] = { 0, 1, 2, 3};
u8 gbObp1[4] = { 0, 1, 2, 3};
int gbWindowLine = -1;

bool genericflashcardEnable = false;
int gbCgbMode = 0;

u16 gbColorFilter[32768];
int gbColorOption = 0;
int gbPaletteOption = 0;
int gbEmulatorType = 0;
int gbBorderOn = 1;
int gbBorderAutomatic = 0;
int gbBorderLineSkip = 160;
int gbBorderRowSkip = 0;
int gbBorderColumnSkip = 0;
int gbDmaTicks = 0;

u8 (*gbSerialFunction)(u8) = NULL;
