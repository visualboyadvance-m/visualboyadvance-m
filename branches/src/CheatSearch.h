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

#ifndef VBA_CHEATSEARCH_H
#define VBA_CHEATSEARCH_H

#include "System.h"

struct CheatSearchBlock {
  int size;
  u32 offset;
  u8 *bits;
  u8 *data;
  u8 *saved;
};

struct CheatSearchData {
  int count;
  CheatSearchBlock *blocks;
};

enum {
  SEARCH_EQ,
  SEARCH_NE,
  SEARCH_LT,
  SEARCH_LE,
  SEARCH_GT,
  SEARCH_GE
};

enum {
  BITS_8,
  BITS_16,
  BITS_32
};

#define SET_BIT(bits,off) \
  (bits)[(off) >> 3] |= (1 << ((off) & 7))

#define CLEAR_BIT(bits, off) \
  (bits)[(off) >> 3] &= ~(1 << ((off) & 7))

#define IS_BIT_SET(bits, off) \
  (bits)[(off) >> 3] & (1 << ((off) & 7))

extern CheatSearchData cheatSearchData;
extern void cheatSearchCleanup(CheatSearchData *cs);
extern void cheatSearchStart(const CheatSearchData *cs);
extern void cheatSearch(const CheatSearchData *cs, int compare, int size, 
                        bool isSigned);
extern void cheatSearchValue(const CheatSearchData *cs, int compare, int size, 
                             bool isSigned, u32 value);
extern int cheatSearchGetCount(const CheatSearchData *cs, int size);
extern void cheatSearchUpdateValues(const CheatSearchData *cs);
extern s32 cheatSearchSignedRead(u8 *data, int off, int size);
extern u32 cheatSearchRead(u8 *data, int off, int size);
#endif
