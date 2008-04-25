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

#include "screenarea-cairo.h"

#include <string.h>

namespace VBA
{

template<typename T> T min( T x, T y ) { return x < y ? x : y; }
template<typename T> T max( T x, T y ) { return x > y ? x : y; }

ScreenAreaCairo::ScreenAreaCairo(int _iWidth, int _iHeight, int _iScale) :
  ScreenArea(_iWidth, _iHeight, _iScale),
  m_puiPixels(NULL),
  m_puiDelta(NULL),
  m_iScaledWidth(_iWidth),
  m_iScaledHeight(_iHeight)
{
  vUpdateSize();
}

ScreenAreaCairo::~ScreenAreaCairo()
{
}

void ScreenAreaCairo::vDrawPixels(u8 * _puiData)
{
  const int iSrcPitch = (m_iWidth + 1) * sizeof(u32);
  const int iScaledPitch = (m_iScaledWidth + 1) * sizeof(u32);

  if (m_vFilterIB != NULL)
  {
    m_vFilterIB(_puiData + iSrcPitch,
                iSrcPitch,
                m_iWidth,
                m_iHeight);
  }

  if (m_vFilter2x != NULL)
  {
    m_vFilter2x(_puiData + iSrcPitch,
                iSrcPitch,
                m_puiDelta,
                (u8 *)m_puiPixels,
                iScaledPitch,
                m_iWidth,
                m_iHeight);
  }
  else
  {
    // TODO : Find a way to use systemXXXShift cleanly to avoid this loop
    u32 * puiPixel = m_puiPixels;
    u8 * puiSrc = _puiData + iSrcPitch;
    for (int i = 0; i < (m_iWidth + 1) * m_iHeight; i++)
    {
      u8 iR = *puiSrc++;
      u8 iG = *puiSrc++;
      u8 iB = *puiSrc++;
      puiSrc++;
      *puiPixel++ = (iR << 16) + (iG << 8) + iB;
    }
  }

  queue_draw();
}

bool ScreenAreaCairo::on_expose_event(GdkEventExpose * _pstEvent)
{ 
  DrawingArea::on_expose_event(_pstEvent);
  Cairo::RefPtr< Cairo::ImageSurface >   poImage;
  Cairo::RefPtr< Cairo::SurfacePattern > poPattern;
  Cairo::RefPtr< Cairo::Context >        poContext;
  Cairo::Matrix oMatrix;
  const int iScaledPitch = (m_iScaledWidth + 1) * sizeof(u32);

  poContext = get_window()->create_cairo_context();

  poContext->set_identity_matrix();
  poContext->scale(m_dScaleFactor, m_dScaleFactor);    
  
  poImage = Cairo::ImageSurface::create((u8 *)m_puiPixels, Cairo::FORMAT_RGB24,
                                    m_iScaledWidth, m_iScaledHeight, iScaledPitch);

  cairo_matrix_init_translate(&oMatrix, -m_iAreaLeft, -m_iAreaTop);
  poPattern = Cairo::SurfacePattern::create(poImage);
  poPattern->set_filter(Cairo::FILTER_NEAREST);
  poPattern->set_matrix (oMatrix);
  poContext->set_source_rgb(0.0, 0.0, 0.0);
  poContext->paint();
  
  poContext->set_source(poPattern);
  poContext->paint(); 
    
  return true;
}

void ScreenAreaCairo::vDrawBlackScreen()
{
  if (m_puiPixels && is_realized())
  {
    memset(m_puiPixels, 0, m_iHeight * (m_iWidth + 1) * sizeof(u32));
    queue_draw_area(0, 0, get_width(), get_height());
  }
}

void ScreenAreaCairo::vUpdateSize()
{
  if (m_puiPixels)
  {
    delete[] m_puiPixels;
  }

  if (m_puiDelta)
  {
    delete[] m_puiDelta;
  }
  
  m_iScaledWidth = m_iFilterScale * m_iWidth;
  m_iScaledHeight = m_iFilterScale * m_iHeight;
  
  vOnWidgetResize();

  m_puiPixels = new u32[(m_iScaledWidth + 1) * m_iScaledHeight];
  m_puiDelta = new u8[(m_iWidth + 2) * (m_iHeight + 2) * sizeof(u32)];
  memset(m_puiDelta, 255, (m_iWidth + 2) * (m_iHeight + 2) * sizeof(u32));

  set_size_request(m_iScale * m_iWidth, m_iScale * m_iHeight);
}

void ScreenAreaCairo::vOnWidgetResize()
{
  m_dScaleFactor = min<double>(get_height() / (double)m_iHeight, get_width() / (double)m_iWidth);
  m_dScaleFactor /= m_iFilterScale;

  m_iAreaTop = (get_height() / m_dScaleFactor - m_iHeight * m_iFilterScale) / 2;
  m_iAreaLeft = (get_width() / m_dScaleFactor - m_iWidth * m_iFilterScale) / 2;
}

bool ScreenAreaCairo::on_configure_event(GdkEventConfigure * event)
{
  vOnWidgetResize();
  
  return true;
}

} // namespace VBA
