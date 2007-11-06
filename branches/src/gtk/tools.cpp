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

#include "tools.h"

namespace VBA
{

std::string sCutSuffix(const std::string & _rsString,
                       const std::string & _rsSep)
{
  return _rsString.substr(0, _rsString.find_last_of(_rsSep));
}

Glib::ustring sCutSuffix(const Glib::ustring & _rsString,
                         const Glib::ustring & _rsSep)
{
  return _rsString.substr(0, _rsString.find_last_of(_rsSep));
}

bool bHasSuffix(const Glib::ustring & _rsString,
                const Glib::ustring & _rsSuffix,
                bool _bCaseSensitive)
{
  if (_rsSuffix.size() > _rsString.size())
  {
    return false;
  }

  Glib::ustring sEnd = _rsString.substr(_rsString.size() - _rsSuffix.size());

  if (_bCaseSensitive)
  {
    if (_rsSuffix == sEnd)
    {
      return true;
    }
  }
  else
  {
    if (_rsSuffix.lowercase() == sEnd.lowercase())
    {
      return true;
    }
  }

  return false;
}

} // namespace VBA
