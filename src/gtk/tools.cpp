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

void vTokenize(Glib::ustring source, std::vector<Glib::ustring>& tokens)
{
  Glib::ustring delimiters = " \t\n\r";

  // Skip delimiters at beginning.
  Glib::ustring::size_type lastPos = source.find_first_not_of(delimiters, 0);
  // Find first "non-delimiter".
  Glib::ustring::size_type pos = source.find_first_of(delimiters, lastPos);

  while (Glib::ustring::npos != pos || std:: string::npos != lastPos)
  {
    // Found a token, add it to the vector.
    tokens.push_back(source.substr(lastPos, pos - lastPos));
    // Skip delimiters.  Note the "not_of"
    lastPos = source.find_first_not_of(delimiters, pos);
    // Find next "non-delimiter"
    pos = source.find_first_of(delimiters, lastPos);
  }
}

} // namespace VBA
