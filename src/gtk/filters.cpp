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
#include "intl.h"

void _2xSaI        (u8 *, u32, u8 *, u8 *, u32, int, int);
void _2xSaI32      (u8 *, u32, u8 *, u8 *, u32, int, int);
void Super2xSaI    (u8 *, u32, u8 *, u8 *, u32, int, int);
void Super2xSaI32  (u8 *, u32, u8 *, u8 *, u32, int, int);
void SuperEagle    (u8 *, u32, u8 *, u8 *, u32, int, int);
void SuperEagle32  (u8 *, u32, u8 *, u8 *, u32, int, int);
void Pixelate      (u8 *, u32, u8 *, u8 *, u32, int, int);
void Pixelate32    (u8 *, u32, u8 *, u8 *, u32, int, int);
void AdMame2x      (u8 *, u32, u8 *, u8 *, u32, int, int);
void AdMame2x32    (u8 *, u32, u8 *, u8 *, u32, int, int);
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

struct {
  char m_csName[30];
  int m_iEnlargeFactor;
  Filter m_apvFunc[2];
}
static const astFilters[] =
{
  { _("None"),                1, { 0,            0              } },
  { _("2xSaI"),               2, { _2xSaI,       _2xSaI32       } },
  { _("Super 2xSaI"),         2, { Super2xSaI,   Super2xSaI32   } },
  { _("Super Eagle"),         2, { SuperEagle,   SuperEagle32   } },
  { _("Pixelate"),            2, { Pixelate,     Pixelate32     } },
  { _("AdvanceMAME Scale2x"), 2, { AdMame2x,     AdMame2x32     } },
  { _("Bilinear"),            2, { Bilinear,     Bilinear32     } },
  { _("Bilinear Plus"),       2, { BilinearPlus, BilinearPlus32 } },
  { _("Scanlines"),           2, { Scanlines,    Scanlines32    } },
  { _("TV Mode"),             2, { ScanlinesTV,  ScanlinesTV32  } },
  { _("hq2x"),                2, { hq2x,         hq2x32         } },
  { _("lq2x"),                2, { lq2x,         lq2x32         } }
};

struct {
  char m_csName[30];
  FilterIB m_apvFunc[2];
}
static const astFiltersIB[] =
{
  { _("None"),                      { 0,            0              } },
  { _("Smart interframe blending"), { SmartIB,      SmartIB32      } },
  { _("Interframe motion blur"),    { MotionBlurIB, MotionBlurIB32 } }
};

Filter pvGetFilter(EFilter _eFilter, EFilterDepth _eDepth)
{
  return astFilters[_eFilter].m_apvFunc[_eDepth];
}

char* pcsGetFilterName(const EFilter _eFilter)
{
        return (char*)astFilters[_eFilter].m_csName;
}

FilterIB pvGetFilterIB(EFilterIB _eFilterIB, EFilterDepth _eDepth)
{
  return astFiltersIB[_eFilterIB].m_apvFunc[_eDepth];
}

char* pcsGetFilterIBName(const EFilterIB _eFilterIB)
{
        return (char*)astFiltersIB[_eFilterIB].m_csName;
}

} // namespace VBA
