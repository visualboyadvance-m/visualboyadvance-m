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

extern int gbRomSizeMask;
extern int gbRomSize;
extern int gbRamSize;
extern int gbRamSizeMask;

extern u8 *gbRom;
extern u8 *gbRam;
extern u8 *gbVram;
extern u8 *gbWram;
extern u8 *gbMemory;
extern u16 *gbLineBuffer;

extern u8 *gbMemoryMap[16];

extern int gbFrameSkip;
extern u16 gbColorFilter[32768];
extern int gbColorOption;
extern int gbPaletteOption;
extern int gbEmulatorType;
extern int gbBorderOn;
extern int gbBorderAutomatic;
extern int gbCgbMode;
extern int gbSgbMode;
extern int gbWindowLine;
extern int gbSpeed;
extern u8 gbBgp[4];
extern u8 gbObp0[4];
extern u8 gbObp1[4];
extern u16 gbPalette[128];

extern u8 register_LCDC;
extern u8 register_LY;
extern u8 register_SCY;
extern u8 register_SCX;
extern u8 register_WY;
extern u8 register_WX;
extern u8 register_VBK;

extern int emulating;

extern int gbBorderLineSkip;
extern int gbBorderRowSkip;
extern int gbBorderColumnSkip;
extern int gbDmaTicks;

extern void gbRenderLine();
extern void gbDrawSprites();

extern u8 (*gbSerialFunction)(u8);
