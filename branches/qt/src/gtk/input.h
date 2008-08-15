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

#ifndef __VBA_INPUT_H__
#define __VBA_INPUT_H__

#include <glib.h>

namespace VBA
{

enum EKey
{
  KeyNone,
  // GBA keys
  KeyA,
  KeyB,
  KeySelect,
  KeyStart,
  KeyRight,
  KeyLeft,
  KeyUp,
  KeyDown,
  KeyR,
  KeyL,
  // VBA extension
  KeySpeed,
  KeyCapture
};

enum EKeyFlag
{
  // GBA keys
  KeyFlagA       = 1 << 0,
  KeyFlagB       = 1 << 1,
  KeyFlagSelect  = 1 << 2,
  KeyFlagStart   = 1 << 3,
  KeyFlagRight   = 1 << 4,
  KeyFlagLeft    = 1 << 5,
  KeyFlagUp      = 1 << 6,
  KeyFlagDown    = 1 << 7,
  KeyFlagR       = 1 << 8,
  KeyFlagL       = 1 << 9,
  // VBA extension
  KeyFlagSpeed   = 1 << 10,
  KeyFlagCapture = 1 << 11,
};

class Keymap
{
 public:
  Keymap();
  ~Keymap();

  void vRegister(guint _uiVal, EKey _eKey);
  void vClear();
  inline EKey eGetKey(guint _uiVal);

 private:
  GHashTable * m_pstTable;

  // noncopyable
  Keymap(const Keymap &);
  Keymap & operator=(const Keymap &);
};

inline EKey Keymap::eGetKey(guint _uiVal)
{
  return (EKey)GPOINTER_TO_UINT(g_hash_table_lookup(m_pstTable,
                                                    GUINT_TO_POINTER(_uiVal)));
}

} // namespace VBA


#endif // __VBA_INPUT_H__
