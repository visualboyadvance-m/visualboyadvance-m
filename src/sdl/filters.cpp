// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004-2008 Forgotten and the VBA development team

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

#include "filters.h"

#include "../filters/filters.hpp"

      //
      // Screen filters
      //

struct FilterDesc {
	char name[30];
	int enlargeFactor;
	FilterFunc func16;
	FilterFunc func24;
	FilterFunc func32;
};

const FilterDesc Filters[] = {
  { "Stretch 1x", 1, sdlStretch1x, sdlStretch1x, sdlStretch1x },
  { "Stretch 2x", 2, sdlStretch2x, sdlStretch2x, sdlStretch2x },
  { "2xSaI", 2, _2xSaI, 0, _2xSaI32 },
  { "Super 2xSaI", 2, Super2xSaI, 0, Super2xSaI32 },
  { "Super Eagle", 2, SuperEagle, 0, SuperEagle32 },
  { "Pixelate", 2, Pixelate, 0, Pixelate32 },
  { "AdvanceMAME Scale2x", 2, AdMame2x, 0, AdMame2x32 },
  { "Bilinear", 2, Bilinear, 0, Bilinear32 },
  { "Bilinear Plus", 2, BilinearPlus, 0, BilinearPlus32 },
  { "Scanlines", 2, Scanlines, 0, Scanlines32 },
  { "TV Mode", 2, ScanlinesTV, 0, ScanlinesTV32 },
  { "lq2x", 2, lq2x, 0, lq2x32 },
  { "hq2x", 2, hq2x, 0, hq2x32 },
  { "Stretch 3x", 3, sdlStretch3x, sdlStretch3x, sdlStretch3x },
  { "hq3x", 3, hq3x16, 0, hq3x32_32 },
  { "Stretch 4x", 4, sdlStretch4x, sdlStretch4x, sdlStretch4x },
  { "hq4x", 4, hq4x16, 0, hq4x32_32 }
};

int getFilterEnlargeFactor(const Filter f)
{
	return Filters[f].enlargeFactor;
}

char* getFilterName(const Filter f)
{
	return (char*)Filters[f].name;
}

FilterFunc initFilter(const Filter f, const int colorDepth, const int srcWidth)
{
  FilterFunc func;

  switch (colorDepth) {
    case 15:
    case 16:
      func = Filters[f].func16;
      break;
    case 24:
      func = Filters[f].func24;
      break;
    case 32:
      func = Filters[f].func32;
      break;
    default:
	  func = 0;
      break;
  }

  if (func)
    switch (f) {
      case kStretch1x:
        sdlStretchInit(colorDepth, 0, srcWidth);
        break;
      case kStretch2x:
        sdlStretchInit(colorDepth, 1, srcWidth);
        break;
      case kStretch3x:
        sdlStretchInit(colorDepth, 2, srcWidth);
        break;
      case kStretch4x:
        sdlStretchInit(colorDepth, 3, srcWidth);
        break;
      case k2xSaI:
      case kSuper2xSaI:
      case kSuperEagle:
        if (colorDepth == 15) Init_2xSaI(555);
        else if (colorDepth == 16) Init_2xSaI(565);
        else Init_2xSaI(colorDepth);
        break;
      case khq2x:
      case klq2x:
        hq2x_init(colorDepth);
        break;
      default:
        break;
    }

  return func;
}

struct IFBFilterDesc {
	char name[30];
	IFBFilterFunc func16;
	IFBFilterFunc func32;
};

const IFBFilterDesc IFBFilters[] = {
  { "No interframe blending", 0, 0 },
  { "Interframe motion blur", MotionBlurIB, MotionBlurIB32 },
  { "Smart interframe blending", SmartIB, SmartIB32 }
};

IFBFilterFunc initIFBFilter(const IFBFilter f, const int colorDepth)
{
  IFBFilterFunc func;

  switch (colorDepth) {
    case 15:
    case 16:
      func = IFBFilters[f].func16;
      break;
    case 32:
      func = IFBFilters[f].func32;
      break;
    case 24:
    default:
	  func = 0;
      break;
  }

  return func;
}

char* getIFBFilterName(const IFBFilter f)
{
	return (char*)IFBFilters[f].name;
}
