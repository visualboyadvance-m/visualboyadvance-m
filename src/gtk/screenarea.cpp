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

#include "screenarea.h"

#include <string.h>

namespace VBA
{

ScreenArea::ScreenArea(int _iWidth, int _iHeight, int _iScale) :
  m_vFilter2x(NULL),
  m_vFilterIB(NULL),
  m_bShowCursor(true),
  m_iFilterScale(1)
{
  g_assert(_iWidth >= 1 && _iHeight >= 1 && _iScale >= 1);

  m_iWidth  = _iWidth;
  m_iHeight = _iHeight;
  m_iScale  = _iScale;

  set_events(Gdk::EXPOSURE_MASK
             | Gdk::POINTER_MOTION_MASK
             | Gdk::ENTER_NOTIFY_MASK
             | Gdk::LEAVE_NOTIFY_MASK);

  char aiEmptyData[8];
  memset(aiEmptyData, 0, sizeof(aiEmptyData));
  Glib::RefPtr<Gdk::Bitmap> poSource = Gdk::Bitmap::create(aiEmptyData, 8, 8);
  Glib::RefPtr<Gdk::Bitmap> poMask = Gdk::Bitmap::create(aiEmptyData, 8, 8);
  Gdk::Color oFg;
  Gdk::Color oBg;
  oFg.set_rgb(0, 0, 0);
  oBg.set_rgb(0, 0, 0);

  m_poEmptyCursor = new Gdk::Cursor(poSource, poMask, oFg, oBg, 0, 0);
}

ScreenArea::~ScreenArea()
{
  if (m_poEmptyCursor != NULL)
  {
    delete m_poEmptyCursor;
  }
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

  if (_iScale != m_iScale)
  {
    m_iScale = _iScale;
    vUpdateSize();
  }
}

void ScreenArea::vSetFilter2x(EFilter2x _eFilter2x)
{
  m_vFilter2x = pvGetFilter2x(_eFilter2x, FilterDepth32);
  
  m_iFilterScale = 1;  
  if (m_iScale == 2 && m_vFilter2x != NULL)
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
  get_window()->set_cursor(*m_poEmptyCursor);
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

} // namespace VBA
