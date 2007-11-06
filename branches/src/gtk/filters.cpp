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

#include "filters.h"

namespace VBA
{

static const Filter2x apvFilters2x[][2] =
{
  { NULL,         NULL           },
  { _2xSaI,       _2xSaI32       },
  { Super2xSaI,   Super2xSaI32   },
  { SuperEagle,   SuperEagle32   },
  { Pixelate,     Pixelate32     },
  { MotionBlur,   MotionBlur32   },
  { AdMame2x,     AdMame2x32     },
  { Simple2x,     Simple2x32     },
  { Bilinear,     Bilinear32     },
  { BilinearPlus, BilinearPlus32 },
  { Scanlines,    Scanlines32    },
  { ScanlinesTV,  ScanlinesTV32  },
  { hq2x,         hq2x32         },
  { lq2x,         lq2x32         }
};

static const FilterIB apvFiltersIB[][2] =
{
  { NULL,         NULL           },
  { SmartIB,      SmartIB32      },
  { MotionBlurIB, MotionBlurIB32 }
};

Filter2x pvGetFilter2x(EFilter2x _eFilter2x, EFilterDepth _eDepth)
{
  return apvFilters2x[_eFilter2x][_eDepth];
}

FilterIB pvGetFilterIB(EFilterIB _eFilterIB, EFilterDepth _eDepth)
{
  return apvFiltersIB[_eFilterIB][_eDepth];
}

} // namespace VBA
