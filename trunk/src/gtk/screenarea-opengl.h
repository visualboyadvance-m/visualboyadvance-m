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

#ifndef __VBA_SCREENAREA_OPENGL_H__
#define __VBA_SCREENAREA_OPENGL_H__

#include "screenarea.h"
#include <gtkmm/gl/widget.h>

namespace VBA
{

class ScreenAreaGl : public ScreenArea,
                     public Gtk::GL::Widget<ScreenAreaGl>
{
public:
  ScreenAreaGl(int _iWidth, int _iHeight, int _iScale = 1);
  virtual ~ScreenAreaGl();
  void vDrawPixels(u8 * _puiData);
  void vDrawBlackScreen();

protected:
  void on_realize();
  bool on_expose_event(GdkEventExpose * _pstEvent);
  bool on_configure_event(GdkEventConfigure * event);

private:
  double   m_dAreaTop;
  double   m_dAreaLeft;
  double   m_dScaleFactor;
  int      m_iScaledWidth;
  int      m_iScaledHeight;
  u32 *    m_puiPixels;
  u8 *     m_puiDelta;

  void vUpdateSize();
  void vOnWidgetResize();
};

} // namespace VBA


#endif // __VBA_SCREENAREA_OPENGL_H__
