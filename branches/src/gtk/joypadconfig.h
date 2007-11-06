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

#ifndef __VBA_JOYPADCONFIG_H__
#define __VBA_JOYPADCONFIG_H__

#include <vector>

#include <libglademm.h>
#include <gtkmm.h>

#ifndef GTKMM20
# include "sigccompat.h"
#endif // ! GTKMM20

#include "input.h"

namespace VBA
{

class JoypadConfig
{
public:
  guint m_uiUp;
  guint m_uiDown;
  guint m_uiLeft;
  guint m_uiRight;
  guint m_uiA;
  guint m_uiB;
  guint m_uiL;
  guint m_uiR;
  guint m_uiSelect;
  guint m_uiStart;
  guint m_uiSpeed;
  guint m_uiCapture;

  guint *  puiAt(int _iIndex);
  int      iFind(guint _uiKeycode);
  void     vSetDefault();
  Keymap * poCreateKeymap() const;
};

class JoypadConfigDialog : public Gtk::Dialog
{
public:
  JoypadConfigDialog(GtkDialog * _pstDialog,
                     const Glib::RefPtr<Gnome::Glade::Xml> & _poXml);
  virtual ~JoypadConfigDialog();

  void vSetConfig(const JoypadConfig & _roConfig);
  inline JoypadConfig stGetConfig() const { return m_oConfig; }

protected:
  bool bOnEntryFocusIn(GdkEventFocus * _pstEvent, guint _uiEntry);
  bool bOnEntryFocusOut(GdkEventFocus * _pstEvent);

  bool on_key_press_event(GdkEventKey * _pstEvent);

private:
  JoypadConfig              m_oConfig;
  Gtk::Button *             m_poOkButton;
  std::vector<Gtk::Entry *> m_oEntries;
  guint *                   m_puiCurrentKeyCode;
  guint                     m_uiCurrentEntry;

  void vUpdateEntries();
};

} // namespace VBA


#endif // __VBA_JOYPADCONFIG_H__
