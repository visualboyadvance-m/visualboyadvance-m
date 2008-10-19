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

#include "screenarea-opengl.h"

#include <cstring>

namespace VBA
{

template<typename T> T min( T x, T y ) { return x < y ? x : y; }
template<typename T> T max( T x, T y ) { return x > y ? x : y; }

ScreenAreaGl::ScreenAreaGl(int _iWidth, int _iHeight, int _iScale) :
  ScreenArea(_iWidth, _iHeight, _iScale)
{
  Glib::RefPtr<Gdk::GL::Config> glconfig;

  glconfig = Gdk::GL::Config::create(Gdk::GL::MODE_RGB    |
                                     Gdk::GL::MODE_DOUBLE);
  if (!glconfig)
    {
      fprintf(stderr, "*** OpenGL : Cannot open display.\n");
      throw std::exception();
    }

  set_gl_capability(glconfig);

  vUpdateSize();
}

void ScreenAreaGl::on_realize()
{
  Gtk::DrawingArea::on_realize();

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glOrtho(0.0, 1.0, 1.0, 0.0, 0.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.0, 0.0, 0.0, 1.0);
}

void ScreenAreaGl::vDrawPixels(u8 * _puiData)
{
  ScreenArea::vDrawPixels(_puiData);

  queue_draw_area(0, 0, get_width(), get_height());
}

void ScreenAreaGl::vDrawBlackScreen()
{
  if (m_puiPixels && is_realized())
  {
    memset(m_puiPixels, 0, m_iHeight * (m_iWidth + 1) * sizeof(u32));
    queue_draw_area(0, 0, get_width(), get_height());
  }
}

void ScreenAreaGl::vOnWidgetResize()
{
  m_dScaleFactor = min<double>(get_height() / (double)m_iScaledHeight, get_width() / (double)m_iScaledWidth);
  glViewport(0, 0, get_width(), get_height());

  m_dAreaTop = 1 - m_dScaleFactor * m_iScaledHeight / (double)get_height();
  m_dAreaLeft = 1 - m_dScaleFactor * m_iScaledWidth / (double)get_width();
}

bool ScreenAreaGl::on_expose_event(GdkEventExpose * _pstEvent)
{
  Glib::RefPtr<Gdk::GL::Window> glwindow = get_gl_window();

  glRasterPos2f(-1.0f + m_dAreaLeft, 1.0f - m_dAreaTop);
  glPixelZoom(m_dScaleFactor, -m_dScaleFactor);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, m_iScaledWidth + 1);

  if (!glwindow->gl_begin(get_gl_context()))
    return false;

    glClear( GL_COLOR_BUFFER_BIT );

    glDrawPixels(m_iScaledWidth, m_iScaledHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_puiPixels);

    glwindow->swap_buffers();

  glwindow->gl_end();

  return true;
}

} // namespace VBA
