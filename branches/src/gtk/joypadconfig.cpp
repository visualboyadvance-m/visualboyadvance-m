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

#include "joypadconfig.h"

#include <string.h>

#include "intl.h"

namespace VBA
{

guint * JoypadConfig::puiAt(int _iIndex)
{
  guint * puiMember;

  switch (_iIndex)
  {
  case 0:
    puiMember = &m_uiUp;
    break;
  case 1:
    puiMember = &m_uiDown;
    break;
  case 2:
    puiMember = &m_uiLeft;
    break;
  case 3:
    puiMember = &m_uiRight;
    break;
  case 4:
    puiMember = &m_uiA;
    break;
  case 5:
    puiMember = &m_uiB;
    break;
  case 6:
    puiMember = &m_uiL;
    break;
  case 7:
    puiMember = &m_uiR;
    break;
  case 8:
    puiMember = &m_uiSelect;
    break;
  case 9:
    puiMember = &m_uiStart;
    break;
  case 10:
    puiMember = &m_uiSpeed;
    break;
  case 11:
    puiMember = &m_uiCapture;
    break;
  default:
    puiMember = NULL;
  }

  return puiMember;
}

int JoypadConfig::iFind(guint _uiKeycode)
{
  for (guint i = 0; i < 12; i++)
  {
    if (*puiAt(i) == _uiKeycode)
    {
      return i;
    }
  }

  return -1;
}

void JoypadConfig::vSetDefault()
{
  guint auiKeyval[] =
  {
    GDK_Up, GDK_Down, GDK_Left, GDK_Right,
    GDK_z, GDK_x, GDK_a, GDK_s,
    GDK_BackSpace, GDK_Return,
    GDK_space, GDK_F12
  };

  for (guint i = 0; i < G_N_ELEMENTS(auiKeyval); i++)
  {
    GdkKeymapKey * pstKeys;
    int iKeys;

    if (gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(),
                                          auiKeyval[i],
                                          &pstKeys,
                                          &iKeys))
    {
      *puiAt(i) = pstKeys[0].keycode;
      g_free(pstKeys);
    }
    else
    {
      *puiAt(i) = 0;
    }
  }
}

Keymap * JoypadConfig::poCreateKeymap() const
{
  Keymap * poKeymap = new Keymap();

  poKeymap->vRegister(m_uiUp,      KeyUp      );
  poKeymap->vRegister(m_uiDown,    KeyDown    );
  poKeymap->vRegister(m_uiLeft,    KeyLeft    );
  poKeymap->vRegister(m_uiRight,   KeyRight   );
  poKeymap->vRegister(m_uiA,       KeyA       );
  poKeymap->vRegister(m_uiB,       KeyB       );
  poKeymap->vRegister(m_uiL,       KeyL       );
  poKeymap->vRegister(m_uiR,       KeyR       );
  poKeymap->vRegister(m_uiSelect,  KeySelect  );
  poKeymap->vRegister(m_uiStart,   KeyStart   );
  poKeymap->vRegister(m_uiSpeed,   KeySpeed   );
  poKeymap->vRegister(m_uiCapture, KeyCapture );

  return poKeymap;
}

JoypadConfigDialog::JoypadConfigDialog(GtkDialog * _pstDialog,
                                       const Glib::RefPtr<Gnome::Glade::Xml> & _poXml) :
  Gtk::Dialog(_pstDialog)
{
  m_puiCurrentKeyCode = NULL;

  memset(&m_oConfig, 0, sizeof(m_oConfig));

  m_poOkButton = dynamic_cast<Gtk::Button *>(_poXml->get_widget("JoypadOkButton"));

  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadUpEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadDownEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadLeftEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadRightEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadAEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadBEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadLEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadREntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadSelectEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadStartEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadSpeedEntry")));
  m_oEntries.push_back(dynamic_cast<Gtk::Entry *>(_poXml->get_widget("JoypadCaptureEntry")));

  for (guint i = 0; i < m_oEntries.size(); i++)
  {
    Gtk::Entry * poEntry = m_oEntries[i];

    poEntry->signal_focus_in_event().connect(SigC::bind<guint>(
                                               SigC::slot(*this, &JoypadConfigDialog::bOnEntryFocusIn),
                                               i));
    poEntry->signal_focus_out_event().connect(SigC::slot(*this, &JoypadConfigDialog::bOnEntryFocusOut));
  }

  vUpdateEntries();
}

JoypadConfigDialog::~JoypadConfigDialog()
{
}

void JoypadConfigDialog::vSetConfig(const JoypadConfig & _roConfig)
{
  m_oConfig = _roConfig;
  vUpdateEntries();
}

void JoypadConfigDialog::vUpdateEntries()
{
  for (guint i = 0; i < m_oEntries.size(); i++)
  {
    guint uiKeyval = 0;
    gdk_keymap_translate_keyboard_state(gdk_keymap_get_default(),
                                        *m_oConfig.puiAt(i),
                                        (GdkModifierType)0,
                                        0,
                                        &uiKeyval,
                                        NULL,
                                        NULL,
                                        NULL);
    const char * csName = gdk_keyval_name(uiKeyval);
    if (csName == NULL)
    {
      m_oEntries[i]->set_text(_("<Undefined>"));
    }
    else
    {
      m_oEntries[i]->set_text(csName);
    }
  }
}

bool JoypadConfigDialog::bOnEntryFocusIn(GdkEventFocus * _pstEvent,
                                         guint           _uiEntry)
{
  m_uiCurrentEntry    = _uiEntry;
  m_puiCurrentKeyCode = m_oConfig.puiAt(_uiEntry);

  return false;
}

bool JoypadConfigDialog::bOnEntryFocusOut(GdkEventFocus * _pstEvent)
{
  m_puiCurrentKeyCode = NULL;

  return false;
}

bool JoypadConfigDialog::on_key_press_event(GdkEventKey * _pstEvent)
{
  if (m_puiCurrentKeyCode == NULL)
  {
    return Gtk::Dialog::on_key_press_event(_pstEvent);
  }

  *m_puiCurrentKeyCode = 0;
  int iFound = m_oConfig.iFind(_pstEvent->hardware_keycode);
  if (iFound >= 0)
  {
    *m_oConfig.puiAt(iFound) = 0;
    m_oEntries[iFound]->set_text(_("<Undefined>"));
  }

  *m_puiCurrentKeyCode = _pstEvent->hardware_keycode;

  guint uiKeyval = 0;
  gdk_keymap_translate_keyboard_state(gdk_keymap_get_default(),
                                      _pstEvent->hardware_keycode,
                                      (GdkModifierType)0,
                                      0,
                                      &uiKeyval,
                                      NULL,
                                      NULL,
                                      NULL);

  const char * csName = gdk_keyval_name(uiKeyval);
  if (csName == NULL)
  {
    m_oEntries[m_uiCurrentEntry]->set_text(_("<Undefined>"));
  }
  else
  {
    m_oEntries[m_uiCurrentEntry]->set_text(csName);
  }

  if (m_uiCurrentEntry + 1 < m_oEntries.size())
  {
    m_oEntries[m_uiCurrentEntry + 1]->grab_focus();
  }
  else
  {
    m_poOkButton->grab_focus();
  }

  return true;
}

} // namespace VBA
