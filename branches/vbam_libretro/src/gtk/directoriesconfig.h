// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2008 VBA-M development team

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

#ifndef __VBA_DIRECTORIESCONFIG_H__
#define __VBA_DIRECTORIESCONFIG_H__

#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/filechooserbutton.h>

#include "configfile.h"

namespace VBA
{

class DirectoriesConfigDialog : public Gtk::Dialog
{
public:
  DirectoriesConfigDialog(Config::Section * _poConfig);

protected:
  void on_response(int response_id);

private:
  struct SDirEntry
  {
    const char * m_csKey;
    const char * m_csLabel;
    const char * m_csFileChooserButton;
  };

  Config::Section *         m_poConfig;
  static const SDirEntry    m_astDirs[];
  Gtk::FileChooserButton *  m_poButtons[6];
};

} // namespace VBA


#endif // __VBA_DIRECTORIESCONFIG_H__
