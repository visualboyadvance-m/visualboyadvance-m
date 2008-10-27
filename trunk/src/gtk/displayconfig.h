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
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#ifndef __VBA_DISPLAYCONFIG_H__
#define __VBA_DISPLAYCONFIG_H__

#include <gtkmm/dialog.h>
#include <gtkmm/builder.h>
#include <gtkmm/combobox.h>

#include "configfile.h"
#include "window.h"

namespace VBA
{

class DisplayConfigDialog : public Gtk::Dialog
{
public:
  DisplayConfigDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder);

  void vSetConfig(Config::Section * _poConfig, VBA::Window * _poWindow);

private:
  void vOnFilterChanged();
  void vOnFilterIBChanged();
  void vOnOutputChanged(VBA::Window::EVideoOutput _eOutput);

  VBA::Window *             m_poWindow;

  Config::Section *         m_poConfig;
  Gtk::ComboBox *           m_poFiltersComboBox;
  Gtk::ComboBox *           m_poIBFiltersComboBox;
  Gtk::RadioButton *        m_poOutputOpenGLRadioButton;
  Gtk::RadioButton *        m_poOutputCairoRadioButton;
  Gtk::RadioButton *        m_poOutputXvRadioButton;
};

} // namespace VBA


#endif // __VBA_DISPLAYCONFIG_H__
