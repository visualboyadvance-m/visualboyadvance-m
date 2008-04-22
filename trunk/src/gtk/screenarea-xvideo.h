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

#ifndef __VBA_SCREENAREA_XVIDEO_H__
#define __VBA_SCREENAREA_XVIDEO_H__

#include "screenarea.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>

namespace VBA
{

class ScreenAreaXv : public ScreenArea
{
public:
  ScreenAreaXv(int _iWidth, int _iHeight, int _iScale = 1);
  virtual ~ScreenAreaXv();
  void vDrawPixels(u8 * _puiData);
  void vDrawColor(u32 _uiColor); // 0xRRGGBB

private:
  Display *m_pDisplay;
  XvImage *m_pXvImage;
  int m_iXvPortId;
  int m_iFormat;
  u16* m_paYUY;
  XShmSegmentInfo m_oShm;

  void vUpdateSize();
  void vRGB32toYUY2 (unsigned char* dest_ptr,
                 int            dest_width,
                 int            dest_height,
                 int            dest_pitch,
                 unsigned char* src_ptr,
                 int            src_width,
                 int            src_height,
                 int            src_pitch);
};

} // namespace VBA


#endif // __VBA_SCREENAREA_XVIDEO_H__
