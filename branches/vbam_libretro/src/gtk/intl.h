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

#ifndef __VBA_INTL_H__
#define __VBA_INTL_H__

#ifdef ENABLE_NLS
# include <glibmm/i18n.h>
#else
# define _(String) (String)
# define N_(String) (String)
# define textdomain(String) (String)
# define gettext(String) (String)
# define dgettext(Domain,String) (String)
# define dcgettext(Domain,String,Type) (String)
# define bindtextdomain(Domain,Directory) (Domain) 
#endif


#endif // __VBA_INTL_H__
