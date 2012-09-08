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

#include "soundconfig.h"

#include <gtkmm/stock.h>
#include <gtkmm/frame.h>
#include <gtkmm/liststore.h>

#include "intl.h"

namespace VBA
{

SoundConfigDialog::SoundConfigDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  Gtk::Dialog(_pstDialog),
  m_poConfig(0)
{
  refBuilder->get_widget("VolumeComboBox", m_poVolumeComboBox);
  refBuilder->get_widget("RateComboBox", m_poRateComboBox);

  m_poVolumeComboBox->signal_changed().connect(sigc::mem_fun(*this, &SoundConfigDialog::vOnVolumeChanged));
  m_poRateComboBox->signal_changed().connect(sigc::mem_fun(*this, &SoundConfigDialog::vOnRateChanged));
}

void SoundConfigDialog::vSetConfig(Config::Section * _poConfig, VBA::Window * _poWindow)
{
  m_poConfig = _poConfig;
  m_poWindow = _poWindow;

  bool bMute = m_poConfig->oGetKey<bool>("mute");
  float fSoundVolume = m_poConfig->oGetKey<float>("volume");

  if (bMute)
    m_poVolumeComboBox->set_active(0);
  else if (0.0f <= fSoundVolume && fSoundVolume <= 0.25f)
    m_poVolumeComboBox->set_active(1);
  else if (0.25f < fSoundVolume && fSoundVolume <= 0.50f)
    m_poVolumeComboBox->set_active(2);
  else if (1.0f < fSoundVolume && fSoundVolume <= 2.0f)
    m_poVolumeComboBox->set_active(4);
  else
    m_poVolumeComboBox->set_active(3);

  long iSoundSampleRate = m_poConfig->oGetKey<long>("sample_rate");
  switch (iSoundSampleRate)
  {
    case 11025:
      m_poRateComboBox->set_active(0);
      break;
    case 22050:
      m_poRateComboBox->set_active(1);
      break;
    default:
    case 44100:
      m_poRateComboBox->set_active(2);
      break;
    case 48000:
      m_poRateComboBox->set_active(3);
      break;
  }
}

void SoundConfigDialog::vOnVolumeChanged()
{
  int iVolume = m_poVolumeComboBox->get_active_row_number();
  switch (iVolume)
  {
    case 0: // Mute
      m_poConfig->vSetKey("mute", true);
      m_poConfig->vSetKey("volume", 1.0f);
      break;
    case 1: // 25 %
      m_poConfig->vSetKey("mute", false);
      m_poConfig->vSetKey("volume", 0.25f);
      break;
    case 2: // 50 %
      m_poConfig->vSetKey("mute", false);
      m_poConfig->vSetKey("volume", 0.50f);
      break;
    case 4: // 200 %
      m_poConfig->vSetKey("mute", false);
      m_poConfig->vSetKey("volume", 2.00f);
      break;
    case 3: // 100 %
    default:
      m_poConfig->vSetKey("mute", false);
      m_poConfig->vSetKey("volume", 1.00f);
      break;
  }

  m_poWindow->vApplyConfigMute();
  m_poWindow->vApplyConfigVolume();
}

void SoundConfigDialog::vOnRateChanged()
{
  int iRate = m_poRateComboBox->get_active_row_number();
  switch (iRate)
  {
    case 0: // 11 KHz
      m_poConfig->vSetKey("sample_rate", 11025);
      break;
    case 1: // 22 KHz
      m_poConfig->vSetKey("sample_rate", 22050);
      break;
    case 2: // 44 KHz
    default:
      m_poConfig->vSetKey("sample_rate", 44100);
      break;
    case 3: // 48 KHz
      m_poConfig->vSetKey("sample_rate", 48000);
      break;
  }

  m_poWindow->vApplyConfigSoundSampleRate();
}

} // namespace VBA
