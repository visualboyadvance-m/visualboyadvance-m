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

#ifndef __VBA_JOYPADCONFIG_H__
#define __VBA_JOYPADCONFIG_H__

#include <vector>

#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/separator.h>
#include <gtkmm/checkbutton.h>

#include "../sdl/inputSDL.h"
#include "configfile.h"

namespace VBA
{

class JoypadConfigDialog : public Gtk::Dialog
{
public:
  JoypadConfigDialog(Config::Section * _poConfig);
  virtual ~JoypadConfigDialog();

protected:
  bool bOnEntryFocusIn(GdkEventFocus * _pstEvent, guint _uiEntry);
  bool bOnEntryFocusOut(GdkEventFocus * _pstEvent);

  void vOnInputEvent(const SDL_Event &event);
  bool on_key_press_event(GdkEventKey * _pstEvent);
  void on_response(int response_id);

private:
  struct SJoypadKey
  {
	const EKey   m_eKeyFlag;
	const char * m_csKeyName;
  };

  Gtk::HBox                 m_oTitleHBox;
  Gtk::Label                m_oTitleLabel;
  Gtk::ComboBoxText         m_oTitleCombo;
  Gtk::HSeparator           m_oSeparator;
  Gtk::CheckButton          m_oDefaultJoypad;
  Gtk::Table                m_oTable;
  Gtk::Button *             m_poOkButton;
  std::vector<Gtk::Entry *> m_oEntries;
  gint                      m_iCurrentEntry;
  bool                      m_bUpdating;
  static const SJoypadKey   m_astKeys[];
  sigc::connection          m_oConfigSig;
  SDL_Event                 m_oPreviousEvent;
  EPad                      m_ePad;
  Config::Section *         m_poConfig;

  bool bOnConfigIdle();
  void vOnJoypadSelect();
  void vOnDefaultJoypadSelect();
  void vUpdateEntries();
  void vEmptyEventQueue();
};

} // namespace VBA


#endif // __VBA_JOYPADCONFIG_H__
