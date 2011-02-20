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

#include "stringtokenizer.h"

namespace VBA
{

void StringTokenizer::tokenize(Glib::ustring source, std::vector<Glib::ustring>& tokens)
{
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

} // VBA namespace
