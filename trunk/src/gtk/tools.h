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

#ifndef __VBA_TOOLS_H__
#define __VBA_TOOLS_H__

#include <string>
#include <vector>
#include <glibmm/ustring.h>

namespace VBA
{

std::string sCutSuffix(const std::string & _rsString,
                       const std::string & _rsSep = ".");

Glib::ustring sCutSuffix(const Glib::ustring & _rsString,
                         const Glib::ustring & _rsSep = ".");

bool bHasSuffix(const Glib::ustring & _rsString,
                const Glib::ustring & _rsSuffix,
                bool _bCaseSensitive = true);
                
void vTokenize(Glib::ustring source, std::vector<Glib::ustring>& tokens);

}


#endif // __VBA_TOOLS_H__
