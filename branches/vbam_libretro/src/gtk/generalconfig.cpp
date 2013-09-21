// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2011 VBA-M development team

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

#include "generalconfig.h"

#include <gtkmm/stock.h>
#include <gtkmm/frame.h>
#include <gtkmm/liststore.h>

#include "intl.h"

namespace VBA
{

PreferencesDialog::PreferencesDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  Gtk::Dialog(_pstDialog),
  m_poConfig(0)
{
  refBuilder->get_widget("PauseWhenInactiveCheckButton", m_poPauseWhenInactiveCheckButton);
  refBuilder->get_widget("FrameSkipAutomaticCheckButton", m_poFrameSkipAutomaticCheckButton);
  refBuilder->get_widget("FrameSkipLevelSpinButton", m_poFrameSkipLevelSpinButton);
  refBuilder->get_widget("SpeedIndicatorComboBox", m_poSpeedIndicatorComboBox);

  m_poPauseWhenInactiveCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &PreferencesDialog::vOnPauseWhenInactiveChanged));
  m_poFrameSkipAutomaticCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &PreferencesDialog::vOnFrameskipChanged));
  m_poFrameSkipLevelSpinButton->signal_changed().connect(sigc::mem_fun(*this, &PreferencesDialog::vOnFrameskipChanged));
  m_poSpeedIndicatorComboBox->signal_changed().connect(sigc::mem_fun(*this, &PreferencesDialog::vOnSpeedIndicatorChanged));

}

void PreferencesDialog::vSetConfig(Config::Section * _poConfig, VBA::Window * _poWindow)
{
  m_poConfig = _poConfig;
  m_poWindow = _poWindow;

  bool bPauseWhenInactive = m_poConfig->oGetKey<bool>("pause_when_inactive");
  m_poPauseWhenInactiveCheckButton->set_active(bPauseWhenInactive);

  std::string sFrameskip = m_poConfig->oGetKey<std::string>("frameskip");
  int iFrameskip = 0;
  bool bAutoFrameskip = false;
  
  if (sFrameskip == "auto")
    bAutoFrameskip = true;
  else
    iFrameskip = m_poConfig->oGetKey<int>("frameskip");

  m_poFrameSkipAutomaticCheckButton->set_active(bAutoFrameskip);
  m_poFrameSkipLevelSpinButton->set_sensitive(!bAutoFrameskip);
  m_poFrameSkipLevelSpinButton->set_value(iFrameskip);

  int iShowSpeed = m_poConfig->oGetKey<int>("show_speed");
  m_poSpeedIndicatorComboBox->set_active(iShowSpeed);
}

void PreferencesDialog::vOnPauseWhenInactiveChanged()
{
  bool bPauseWhenInactive = m_poPauseWhenInactiveCheckButton->get_active();
  m_poConfig->vSetKey("pause_when_inactive", bPauseWhenInactive);
}

void PreferencesDialog::vOnFrameskipChanged()
{
  bool bAutoFrameskip = m_poFrameSkipAutomaticCheckButton->get_active();
  
  if (bAutoFrameskip)
  {
    m_poConfig->vSetKey("frameskip", "auto");
  }
  else
  {
    int iFrameskip = m_poFrameSkipLevelSpinButton->get_value();
    m_poConfig->vSetKey("frameskip", iFrameskip);
  }
  
  m_poFrameSkipLevelSpinButton->set_sensitive(!bAutoFrameskip);  
  
  m_poWindow->vApplyConfigFrameskip();
}

void PreferencesDialog::vOnSpeedIndicatorChanged()
{
  int iShowSpeed = m_poSpeedIndicatorComboBox->get_active_row_number();
  m_poConfig->vSetKey<int>("show_speed", iShowSpeed);
  
  m_poWindow->vApplyConfigShowSpeed();
}

} // namespace VBA
