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

#ifndef __VBA_MENUITEM_H__
#define __VBA_MENUITEM_H__

#include <gtkmm/menuitem.h>
#include <gtkmm/imagemenuitem.h>

#ifdef GTKMM20
namespace Gtk { typedef Gtk::Menu_Helpers::AccelKey AccelKey; }
#endif // GTKMM20

namespace VBA
{

class MenuItem : public Gtk::MenuItem
{
public:
  MenuItem()
    {}

  MenuItem(Gtk::Widget & _roWidget) :
    Gtk::MenuItem(_roWidget)
    {}

  MenuItem(const Glib::ustring & _rsLabel, bool _bMnemonic = false) :
    Gtk::MenuItem(_rsLabel, _bMnemonic)
    {}

  inline void set_accel_key(const Gtk::AccelKey & _roAccelKey)
    {
      Gtk::MenuItem::set_accel_key(_roAccelKey);
    }
};

class ImageMenuItem : public Gtk::ImageMenuItem
{
public:
  ImageMenuItem()
    {}

  ImageMenuItem(Widget & _roImage, const Glib::ustring & _rsLabel, bool _bMnemonic = false) :
    Gtk::ImageMenuItem(_roImage, _rsLabel, _bMnemonic)
    {}

  ImageMenuItem(const Glib::ustring & _rsLabel, bool _bMnemonic = false) :
    Gtk::ImageMenuItem(_rsLabel, _bMnemonic)
    {}

  ImageMenuItem(const Gtk::StockID & _roId) :
    Gtk::ImageMenuItem(_roId)
    {}

  inline void set_accel_key(const Gtk::AccelKey & _roAccelKey)
    {
      Gtk::MenuItem::set_accel_key(_roAccelKey);
    }
};

} // namespace VBA


#endif // __VBA_MENUITEM_H__
