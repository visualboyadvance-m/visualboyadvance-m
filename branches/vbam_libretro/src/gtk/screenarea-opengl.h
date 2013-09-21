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
  void vDrawPixels(u8 * _puiData);
  void vDrawBlackScreen();

protected:
  void on_realize();
  bool on_expose_event(GdkEventExpose * _pstEvent);
  void vOnWidgetResize();

private:
  GLuint m_uiScreenTexture;
  int m_iTextureSize;

  void vUpdateTexture();
  void vOnSizeUpdated();
};

} // namespace VBA


#endif // __VBA_SCREENAREA_OPENGL_H__
