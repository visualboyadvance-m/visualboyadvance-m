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

#ifndef __VBA_FILTERS_H__
#define __VBA_FILTERS_H__

#include "../System.h"

int Init_2xSaI(u32);

void _2xSaI        (u8 *, u32, u8 *, u8 *, u32, int, int);
void _2xSaI32      (u8 *, u32, u8 *, u8 *, u32, int, int);
void Super2xSaI    (u8 *, u32, u8 *, u8 *, u32, int, int);
void Super2xSaI32  (u8 *, u32, u8 *, u8 *, u32, int, int);
void SuperEagle    (u8 *, u32, u8 *, u8 *, u32, int, int);
void SuperEagle32  (u8 *, u32, u8 *, u8 *, u32, int, int);
void Pixelate      (u8 *, u32, u8 *, u8 *, u32, int, int);
void Pixelate32    (u8 *, u32, u8 *, u8 *, u32, int, int);
void MotionBlur    (u8 *, u32, u8 *, u8 *, u32, int, int);
void MotionBlur32  (u8 *, u32, u8 *, u8 *, u32, int, int);
void AdMame2x      (u8 *, u32, u8 *, u8 *, u32, int, int);
void AdMame2x32    (u8 *, u32, u8 *, u8 *, u32, int, int);
void Simple2x      (u8 *, u32, u8 *, u8 *, u32, int, int);
void Simple2x32    (u8 *, u32, u8 *, u8 *, u32, int, int);
void Bilinear      (u8 *, u32, u8 *, u8 *, u32, int, int);
void Bilinear32    (u8 *, u32, u8 *, u8 *, u32, int, int);
void BilinearPlus  (u8 *, u32, u8 *, u8 *, u32, int, int);
void BilinearPlus32(u8 *, u32, u8 *, u8 *, u32, int, int);
void Scanlines     (u8 *, u32, u8 *, u8 *, u32, int, int);
void Scanlines32   (u8 *, u32, u8 *, u8 *, u32, int, int);
void ScanlinesTV   (u8 *, u32, u8 *, u8 *, u32, int, int);
void ScanlinesTV32 (u8 *, u32, u8 *, u8 *, u32, int, int);
void hq2x          (u8 *, u32, u8 *, u8 *, u32, int, int);
void hq2x32        (u8 *, u32, u8 *, u8 *, u32, int, int);
void lq2x          (u8 *, u32, u8 *, u8 *, u32, int, int);
void lq2x32        (u8 *, u32, u8 *, u8 *, u32, int, int);

void SmartIB       (u8 *, u32, int, int);
void SmartIB32     (u8 *, u32, int, int);
void MotionBlurIB  (u8 *, u32, int, int);
void MotionBlurIB32(u8 *, u32, int, int);

namespace VBA
{

typedef void (*Filter2x)(u8 *, u32, u8 *, u8 *, u32, int, int);
typedef void (*FilterIB)(u8 *, u32, int, int);

enum EFilter2x
{
  FirstFilter,
  FilterNone = FirstFilter,
  Filter2xSaI,
  FilterSuper2xSaI,
  FilterSuperEagle,
  FilterPixelate,
  FilterMotionBlur,
  FilterAdMame2x,
  FilterSimple2x,
  FilterBilinear,
  FilterBilinearPlus,
  FilterScanlines,
  FilterScanlinesTV,
  FilterHq2x,
  FilterLq2x,
  LastFilter = FilterLq2x
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

Filter2x pvGetFilter2x(EFilter2x _eFilter2x, EFilterDepth _eDepth);
FilterIB pvGetFilterIB(EFilterIB _eFilterIB, EFilterDepth _eDepth);

} // namespace VBA


#endif // __VBA_FILTERS_H__
