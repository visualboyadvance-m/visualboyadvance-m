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

#include "screenarea-cairo.h"

#include <cstring>

namespace VBA
{

template<typename T> T min( T x, T y ) { return x < y ? x : y; }
template<typename T> T max( T x, T y ) { return x > y ? x : y; }

ScreenAreaCairo::ScreenAreaCairo(int _iWidth, int _iHeight, int _iScale) :
  ScreenArea(_iWidth, _iHeight, _iScale)
{
  vUpdateSize();
}

void ScreenAreaCairo::vDrawPixels(u8 * _puiData)
{
  ScreenArea::vDrawPixels(_puiData);

  queue_draw();
}

#if !GTK_CHECK_VERSION(3, 0, 0)
bool ScreenAreaCairo::on_expose_event(GdkEventExpose * _pstEvent)
{
  DrawingArea::on_expose_event(_pstEvent);
  Cairo::RefPtr< Cairo::ImageSurface >   poImage;
  Cairo::RefPtr< Cairo::SurfacePattern > poPattern;
  Cairo::RefPtr< Cairo::Context >        poContext;
  Cairo::Matrix oMatrix;
  const int iScaledPitch = (m_iScaledWidth + 1) * sizeof(u32);

  poContext = get_window()->create_cairo_context();

  //poContext->set_identity_matrix();
  poContext->scale(m_dScaleFactor, m_dScaleFactor);

  poImage = Cairo::ImageSurface::create((u8 *)m_puiPixels, Cairo::FORMAT_RGB24,
                                    m_iScaledWidth, m_iScaledHeight, iScaledPitch);

  //cairo_matrix_init_translate(&oMatrix, -m_iAreaLeft, -m_iAreaTop);
  poPattern = Cairo::SurfacePattern::create(poImage);
  poPattern->set_filter(Cairo::FILTER_NEAREST);
  //poPattern->set_matrix (oMatrix);
  poContext->set_source_rgb(0.0, 0.0, 0.0);
  poContext->paint();

  poContext->set_source(poPattern);
  poContext->paint();

  return true;
}
#else
bool ScreenAreaCairo::on_draw(const Cairo::RefPtr<Cairo::Context> &poContext)
{
  DrawingArea::on_draw(poContext);
  Cairo::RefPtr< Cairo::ImageSurface >   poImage;
  Cairo::RefPtr< Cairo::SurfacePattern > poPattern;
  Cairo::Matrix oMatrix;
  const int iScaledPitch = (m_iScaledWidth + 1) * sizeof(u32);

  //poContext->set_identity_matrix();
  poContext->scale(m_dScaleFactor, m_dScaleFactor);

  poImage = Cairo::ImageSurface::create((u8 *)m_puiPixels, Cairo::FORMAT_RGB24,
                                    m_iScaledWidth, m_iScaledHeight, iScaledPitch);

  //cairo_matrix_init_translate(&oMatrix, -m_iAreaLeft, -m_iAreaTop);
  poPattern = Cairo::SurfacePattern::create(poImage);
  poPattern->set_filter(Cairo::FILTER_NEAREST);
  //poPattern->set_matrix (oMatrix);
  poContext->set_source_rgb(0.0, 0.0, 0.0);
  poContext->paint();

  poContext->set_source(poPattern);
  poContext->paint();

  return true;
}
#endif

void ScreenAreaCairo::vDrawBlackScreen()
{
  if (m_puiPixels && get_realized())
  {
    memset(m_puiPixels, 0, m_iHeight * (m_iWidth + 1) * sizeof(u32));
    queue_draw_area(0, 0, get_width(), get_height());
  }
}

void ScreenAreaCairo::vOnWidgetResize()
{
  m_dScaleFactor = min<double>(get_height() / (double)m_iHeight, get_width() / (double)m_iWidth);
  m_dScaleFactor /= m_iFilterScale;

  m_iAreaTop = (get_height() / m_dScaleFactor - m_iHeight * m_iFilterScale) / 2;
  m_iAreaLeft = (get_width() / m_dScaleFactor - m_iWidth * m_iFilterScale) / 2;
}

} // namespace VBA
