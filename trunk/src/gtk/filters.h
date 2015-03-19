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
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef __VBA_FILTERS_H__
#define __VBA_FILTERS_H__

#include "../System.h"

int Init_2xSaI(u32);

namespace VBA
{

typedef void (*Filter)(u8 *, u32, u8 *, u8 *, u32, int, int);
typedef void (*FilterIB)(u8 *, u32, int, int);

enum EFilter
{
  FirstFilter,
  FilterNone = FirstFilter,
  Filter2xSaI,
  FilterSuper2xSaI,
  FilterSuperEagle,
  FilterPixelate,
  FilterAdMame2x,
  FilterBilinear,
  FilterBilinearPlus,
  FilterScanlines,
  FilterScanlinesTV,
  FilterHq2x,
  FilterLq2x,
  FilterxBRZ2x,
  LastFilter = FilterxBRZ2x
};

enum EFilterIB
{
  FirstFilterIB,
  FilterIBNone = FirstFilterIB,
  FilterIBSmart,
  FilterIBMotionBlur,
  LastFilterIB = FilterIBMotionBlur
};

enum EFilterDepth
{
  FilterDepth16,
  FilterDepth32
};

Filter pvGetFilter(EFilter _eFilter, EFilterDepth _eDepth);
const char* pcsGetFilterName(const EFilter _eFilter);

FilterIB pvGetFilterIB(EFilterIB _eFilterIB, EFilterDepth _eDepth);
const char* pcsGetFilterIBName(const EFilterIB _eFilterIB);

} // namespace VBA


#endif // __VBA_FILTERS_H__
