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

#include "directoriesconfig.h"

#include <gtkmm/stock.h>

#include "intl.h"

namespace VBA
{

const DirectoriesConfigDialog::SDirEntry DirectoriesConfigDialog::m_astDirs[] =
{
    { "gba_roms",  N_("GBA roms :"),  "GBARomsDirEntry"   },
    { "gb_roms",   N_("GB roms :"),   "GBRomsDirEntry"    },
    { "batteries", N_("Batteries :"), "BatteriesDirEntry" },
    { "cheats",    N_("Cheats :"),    "CheatsDirEntry"    },
    { "saves",     N_("Saves :"),     "SavesDirEntry"     },
    { "captures",  N_("Captures :"),  "CapturesDirEntry"  }
};

DirectoriesConfigDialog::DirectoriesConfigDialog(Config::Section * _poConfig) :
  Gtk::Dialog(_("Directories config"), true),
  m_poConfig(_poConfig)
{
  Gtk::Table * poTable = Gtk::manage( new Gtk::Table(G_N_ELEMENTS(m_astDirs), 2, false));
  poTable->set_border_width(5);
  poTable->set_spacings(5);

  for (guint i = 0; i < G_N_ELEMENTS(m_astDirs); i++)
  {
    Gtk::Label * poLabel = Gtk::manage( new Gtk::Label(gettext(m_astDirs[i].m_csLabel), Gtk::ALIGN_END) );
    m_poButtons[i] = Gtk::manage( new Gtk::FileChooserButton(Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER) );
    m_poButtons[i]->set_current_folder(m_poConfig->sGetKey(m_astDirs[i].m_csKey));

    poTable->attach(* poLabel, 0, 1, i, i + 1);
    poTable->attach(* m_poButtons[i], 1, 2, i, i + 1);
  }

  add_button(Gtk::Stock::CLOSE, Gtk::RESPONSE_CLOSE);
  get_vbox()->pack_start(* poTable);
  show_all_children();
}

void DirectoriesConfigDialog::on_response(int response_id)
{
  for (guint i = 0; i < G_N_ELEMENTS(m_astDirs); i++)
  {
    m_poConfig->vSetKey(m_astDirs[i].m_csKey, m_poButtons[i]->get_current_folder());
  }
}

} // namespace VBA
