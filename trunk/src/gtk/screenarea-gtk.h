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

#ifndef __VBA_SCREENAREA_GTK_H__
#define __VBA_SCREENAREA_GTK_H__

#include "screenarea.h"

namespace VBA
{

class ScreenAreaGtk : public ScreenArea
{
public:
  ScreenAreaGtk(int _iWidth, int _iHeight, int _iScale = 1);
  virtual ~ScreenAreaGtk();
  void vDrawPixels(u8 * _puiData);
  void vDrawColor(u32 _uiColor); // 0xRRGGBB

protected:
  bool on_expose_event(GdkEventExpose * _pstEvent);

private:
  int      m_iRowStride;
  u32 *    m_puiPixels;
  u8 *     m_puiDelta;

  void vUpdateSize();
};

} // namespace VBA


#endif // __VBA_SCREENAREA_GTK_H__
