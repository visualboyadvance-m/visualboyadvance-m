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

#include "screenarea.h"

#include <cstring>
#include <glibmm/main.h>

namespace VBA
{

ScreenArea::ScreenArea(int _iWidth, int _iHeight, int _iScale) :
  m_iFilterScale(1),
  m_vFilter2x(NULL),
  m_vFilterIB(NULL),
  m_puiPixels(NULL),
  m_puiDelta(NULL),
  m_iScaledWidth(_iWidth),
  m_iScaledHeight(_iHeight),
  m_bEnableRender(true),
  m_bShowCursor(true)
{
  g_assert(_iWidth >= 1 && _iHeight >= 1 && _iScale >= 1);

  m_iWidth  = _iWidth;
  m_iHeight = _iHeight;
  m_iScale  = _iScale;

  set_events(Gdk::EXPOSURE_MASK
             | Gdk::POINTER_MOTION_MASK
             | Gdk::ENTER_NOTIFY_MASK
             | Gdk::LEAVE_NOTIFY_MASK);

  Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, 8, 8);
  pixbuf->fill(0);

#if !GTK_CHECK_VERSION(3, 0, 0)
  m_poEmptyCursor = new Gdk::Cursor(get_display, pixbuf, 0, 0);
#else
  m_poEmptyCursor = Gdk::Cursor::create(get_display(), pixbuf, 0, 0);
#endif
}

ScreenArea::~ScreenArea()
{
  if (m_puiPixels)
  {
    delete[] m_puiPixels;
  }

  if (m_puiDelta)
  {
    delete[] m_puiDelta;
  }

#if !GTK_CHECK_VERSION(3, 0, 0)
  if (m_poEmptyCursor != NULL)
  {
    delete m_poEmptyCursor;
  }
#else
  m_poEmptyCursor.reset();
#endif
}

void ScreenArea::vSetSize(int _iWidth, int _iHeight)
{
  g_return_if_fail(_iWidth >= 1 && _iHeight >= 1);

  if (_iWidth != m_iWidth || _iHeight != m_iHeight)
  {
    m_iWidth  = _iWidth;
    m_iHeight = _iHeight;
    vUpdateSize();
  }
}

void ScreenArea::vSetScale(int _iScale)
{
  g_return_if_fail(_iScale >= 1);

  if (_iScale == 1)
  {
    vSetFilter(FilterNone);
  }

  m_iScale = _iScale;
  vUpdateSize();
}

void ScreenArea::vSetFilter(EFilter _eFilter)
{
  m_vFilter2x = pvGetFilter(_eFilter, FilterDepth32);

  m_iFilterScale = 1;
  if (m_vFilter2x != NULL)
  {
    m_iFilterScale = 2;
  }

  vUpdateSize();
}

void ScreenArea::vSetFilterIB(EFilterIB _eFilterIB)
{
  m_vFilterIB = pvGetFilterIB(_eFilterIB, FilterDepth32);
}

void ScreenArea::vStartCursorTimeout()
{
  m_oCursorSig.disconnect();
  m_oCursorSig = Glib::signal_timeout().connect(
    sigc::mem_fun(*this, &ScreenArea::bOnCursorTimeout),
    2000);
}

void ScreenArea::vStopCursorTimeout()
{
  m_oCursorSig.disconnect();
}

void ScreenArea::vHideCursor()
{
#if !GTK_CHECK_VERSION(3, 0, 0)
  get_window()->set_cursor(*m_poEmptyCursor);
#else
  get_window()->set_cursor(m_poEmptyCursor);
#endif
  m_bShowCursor = false;
}

void ScreenArea::vShowCursor()
{
  get_window()->set_cursor();
  m_bShowCursor = true;
}

bool ScreenArea::on_motion_notify_event(GdkEventMotion * _pstEvent)
{
  if (! m_bShowCursor)
  {
    vShowCursor();
  }
  vStartCursorTimeout();
  return false;
}

bool ScreenArea::on_enter_notify_event(GdkEventCrossing * _pstEvent)
{
  vStartCursorTimeout();
  return false;
}

bool ScreenArea::on_leave_notify_event(GdkEventCrossing * _pstEvent)
{
  vStopCursorTimeout();
  if (! m_bShowCursor)
  {
    vShowCursor();
  }
  return false;
}

bool ScreenArea::bOnCursorTimeout()
{
  vHideCursor();
  return false;
}

void ScreenArea::vDrawPixels(u8 * _puiData)
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
    memcpy(m_puiPixels, _puiData + iSrcPitch, m_iHeight * iSrcPitch);
  }
}

void ScreenArea::vUpdateSize()
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

  m_puiPixels = new u32[(m_iScaledWidth + 1) * m_iScaledHeight];
  m_puiDelta = new u8[(m_iWidth + 2) * (m_iHeight + 2) * sizeof(u32)];
  memset(m_puiPixels, 0, (m_iScaledWidth + 1) * m_iScaledHeight * sizeof(u32));
  memset(m_puiDelta, 255, (m_iWidth + 2) * (m_iHeight + 2) * sizeof(u32));

  vOnSizeUpdated();

  set_size_request(m_iScale * m_iWidth, m_iScale * m_iHeight);
}

bool ScreenArea::on_configure_event(GdkEventConfigure * event)
{
  vOnWidgetResize();

  return true;
}

void ScreenArea::vSetEnableRender(bool _bEnable)
{
  m_bEnableRender = _bEnable;
}

} // namespace VBA
