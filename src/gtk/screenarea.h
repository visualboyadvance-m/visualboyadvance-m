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

#ifndef __VBA_SCREENAREA_H__
#define __VBA_SCREENAREA_H__

#include <gtkmm/drawingarea.h>
#include <gdkmm/cursor.h>

#include "filters.h"

namespace VBA
{

class ScreenArea : public Gtk::DrawingArea
{
public:
  ScreenArea(int _iWidth, int _iHeight, int _iScale = 1);
  virtual ~ScreenArea();

  void vSetSize(int _iWidth, int _iHeight);
  void vSetScale(int _iScale);
  void vSetFilter(EFilter _eFilter);
  void vSetFilterIB(EFilterIB _eFilterIB);
  void vSetEnableRender(bool _bEnable);
  virtual void vDrawPixels(u8 * _puiData);
  virtual void vDrawBlackScreen() = 0;

protected:
  virtual bool on_motion_notify_event(GdkEventMotion * _pstEvent);
  virtual bool on_enter_notify_event(GdkEventCrossing * _pstEvent);
  virtual bool on_leave_notify_event(GdkEventCrossing * _pstEvent);
  virtual bool on_configure_event(GdkEventConfigure * event);
  virtual bool bOnCursorTimeout();
  virtual void vOnSizeUpdated() {}

  int      m_iWidth;
  int      m_iHeight;
  int      m_iScale;
  int      m_iFilterScale;
  int      m_iAreaWidth;
  int      m_iAreaHeight;
  Filter   m_vFilter2x;
  FilterIB m_vFilterIB;
  u32 *    m_puiPixels;
  u8 *     m_puiDelta;
  int      m_iScaledWidth;
  int      m_iScaledHeight;
  bool     m_bEnableRender;

  bool             m_bShowCursor;

#if !GTK_CHECK_VERSION(3, 0, 0)
  Gdk::Cursor *    m_poEmptyCursor;
#else
  Glib::RefPtr<Gdk::Cursor> m_poEmptyCursor;
#endif
  sigc::connection m_oCursorSig;

  void vUpdateSize();
  virtual void vOnWidgetResize() = 0;
  void vStartCursorTimeout();
  void vStopCursorTimeout();
  void vHideCursor();
  void vShowCursor();
};

} // namespace VBA


#endif // __VBA_SCREENAREA_H__
