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

const EKey JoypadConfigDialog::m_aeKeys[] =
  {
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_BUTTON_A,
    KEY_BUTTON_B,
    KEY_BUTTON_L,
    KEY_BUTTON_R,
    KEY_BUTTON_SELECT,
    KEY_BUTTON_START,
    KEY_BUTTON_SPEED,
    KEY_BUTTON_CAPTURE
  };

JoypadConfigDialog::JoypadConfigDialog(GtkDialog * _pstDialog,
                                       const Glib::RefPtr<Gnome::Glade::Xml> & _poXml) :
  Gtk::Dialog(_pstDialog)
{
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

    poEntry->signal_focus_in_event().connect(sigc::bind(
                                               sigc::mem_fun(*this, &JoypadConfigDialog::bOnEntryFocusIn),
                                               i));
    poEntry->signal_focus_out_event().connect(sigc::mem_fun(*this, &JoypadConfigDialog::bOnEntryFocusOut));
  }

  vUpdateEntries();

  vEmptyEventQueue();

  memset(&m_oPreviousEvent, 0, sizeof(m_oPreviousEvent));

  m_oConfigSig = Glib::signal_idle().connect(sigc::mem_fun(*this, &JoypadConfigDialog::bOnConfigIdle),
          Glib::PRIORITY_DEFAULT_IDLE);
}

JoypadConfigDialog::~JoypadConfigDialog()
{
  m_oConfigSig.disconnect();
}

void JoypadConfigDialog::vUpdateEntries()
{
  for (guint i = 0; i < m_oEntries.size(); i++)
  {
	const char * csName = 0;

    guint uiKeyval = inputGetKeymap(PAD_MAIN, m_aeKeys[i]);
    int dev = uiKeyval >> 16;
    if (dev == 0)
    {
      csName = gdk_keyval_name(uiKeyval);
    }
    else
    {
      int what = uiKeyval & 0xffff;
      std::stringstream os;
      os << "Joy " << dev;

      if(what >= 128)
      {
        // joystick button
        int button = what - 128;
        os << " Button " << button;
      }
      else if (what < 0x20)
      {
        // joystick axis
        int dir = what & 1;
		what >>= 1;
	    os << " Axis " << what << (dir?'-':'+');
      }
      else if (what < 0x30)
      {
        // joystick hat
        what = (what & 15);
        what >>= 2;
        os << " Hat " << what;
      }

      csName = os.str().c_str();
    }

    if (csName == 0)
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
  m_iCurrentEntry = _uiEntry;

  return false;
}

bool JoypadConfigDialog::bOnEntryFocusOut(GdkEventFocus * _pstEvent)
{
  m_iCurrentEntry = -1;

  return false;
}

bool JoypadConfigDialog::on_key_press_event(GdkEventKey * _pstEvent)
{
  if (m_iCurrentEntry < 0)
  {
    return Gtk::Window::on_key_press_event(_pstEvent);
  }

  // Forward the keyboard event by faking a SDL event
  SDL_Event event;
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = (SDLKey)_pstEvent->keyval;
  vOnInputEvent(event);

  return true;
}

void JoypadConfigDialog::vOnInputEvent(const SDL_Event &event)
{
  if (m_iCurrentEntry < 0)
  {
	return;
  }

  int code = inputGetEventCode(event);
  inputSetKeymap(PAD_MAIN, m_aeKeys[m_iCurrentEntry], code);
  vUpdateEntries();

  if (m_iCurrentEntry + 1 < (gint)m_oEntries.size())
  {
    m_oEntries[m_iCurrentEntry + 1]->grab_focus();
  }
  else
  {
    m_poOkButton->grab_focus();
  }
}

bool JoypadConfigDialog::bOnConfigIdle()
{
  SDL_Event event;
  while(SDL_PollEvent(&event))
  {
	switch(event.type)
	{
	  case SDL_JOYAXISMOTION:
		if (event.jaxis.which != m_oPreviousEvent.jaxis.which ||
			event.jaxis.axis != m_oPreviousEvent.jaxis.axis	||
			(event.jaxis.value > 0 && m_oPreviousEvent.jaxis.value < 0) ||
			(event.jaxis.value < 0 && m_oPreviousEvent.jaxis.value > 0))
		{
		  vOnInputEvent(event);
		  m_oPreviousEvent = event;
		}
		vEmptyEventQueue();
		break;
	  case SDL_JOYHATMOTION:
	  case SDL_JOYBUTTONUP:
        vOnInputEvent(event);
		vEmptyEventQueue();
		break;
	}
  }

  return true;
}

void JoypadConfigDialog::vEmptyEventQueue()
{
  // Empty the SDL event queue
  SDL_Event event;
  while(SDL_PollEvent(&event));
}

} // namespace VBA
