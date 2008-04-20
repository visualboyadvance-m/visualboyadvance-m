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

#include "input.h"

#include <new>

namespace VBA
{

Keymap::Keymap()
{
  m_pstTable = g_hash_table_new(g_direct_hash, g_direct_equal);
  if (m_pstTable == NULL)
  {
    throw std::bad_alloc();
  }
}

Keymap::~Keymap()
{
  g_hash_table_destroy(m_pstTable);
}

void Keymap::vRegister(guint _uiVal, EKey _eKey)
{
  g_hash_table_insert(m_pstTable,
                      GUINT_TO_POINTER(_uiVal),
                      GUINT_TO_POINTER(_eKey));
}

void Keymap::vClear()
{
  g_hash_table_destroy(m_pstTable);
  m_pstTable = g_hash_table_new(g_direct_hash, g_direct_equal);
}

} // namespace VBA
