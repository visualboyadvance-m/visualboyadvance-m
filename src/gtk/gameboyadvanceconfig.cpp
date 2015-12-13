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

#include "gameboyadvanceconfig.h"

#include "intl.h"

namespace VBA
{

GameBoyAdvanceConfigDialog::GameBoyAdvanceConfigDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  Gtk::Dialog(_pstDialog),
  m_poConfig(0)
{
  refBuilder->get_widget("SaveTypeComboBox", m_poSaveTypeComboBox);
  refBuilder->get_widget("FlashSizeComboBox", m_poFlashSizeComboBox);
  refBuilder->get_widget("BiosCheckButton", m_poBiosCheckButton);
  refBuilder->get_widget("BiosFileChooserButton", m_poBiosFileChooserButton);
  refBuilder->get_widget("RTCCheckButton", m_poRTCCheckButton);

  m_poSaveTypeComboBox->signal_changed().connect(sigc::mem_fun(*this, &GameBoyAdvanceConfigDialog::vOnSaveTypeChanged));
  m_poFlashSizeComboBox->signal_changed().connect(sigc::mem_fun(*this, &GameBoyAdvanceConfigDialog::vOnFlashSizeChanged));
  m_poBiosCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &GameBoyAdvanceConfigDialog::vOnUseBiosChanged));
  m_poBiosFileChooserButton->signal_selection_changed().connect(sigc::mem_fun(*this, &GameBoyAdvanceConfigDialog::vOnBiosSelectionChanged));
  m_poRTCCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &GameBoyAdvanceConfigDialog::vOnEnableRTCChanged));
}

void GameBoyAdvanceConfigDialog::vSetConfig(Config::Section * _poConfig, VBA::Window * _poWindow)
{
  m_poConfig = _poConfig;
  m_poWindow = _poWindow;

  VBA::Window::ESaveType eDefaultSaveType = (VBA::Window::ESaveType)m_poConfig->oGetKey<int>("save_type");
  m_poSaveTypeComboBox->set_active(eDefaultSaveType);

  int iDefaultFlashSize = m_poConfig->oGetKey<int>("flash_size");
  if (iDefaultFlashSize == 128)
  {
    m_poFlashSizeComboBox->set_active(1);
  }
  else
  {
    m_poFlashSizeComboBox->set_active(0);
  }

  bool bUseBios = m_poConfig->oGetKey<bool>("use_bios_file");
  m_poBiosCheckButton->set_active(bUseBios);
  m_poBiosFileChooserButton->set_sensitive(bUseBios);
  
  std::string sBios = m_poConfig->oGetKey<std::string>("bios_file");
  m_poBiosFileChooserButton->set_filename(sBios);
  
  const char * acsPattern[] =
  {
    "*.[bB][iI][nN]", "*.[aA][gG][bB]", "*.[gG][bB][aA]",
    "*.[bB][iI][oO][sS]", "*.[zZ][iI][pP]", "*.[zZ]", "*.[gG][zZ]"
  };

#if !GTK_CHECK_VERSION(3, 0, 0)
  Gtk::FileFilter oAllFilter;
  oAllFilter.set_name(_("All files"));
  oAllFilter.add_pattern("*");

  Gtk::FileFilter oBiosFilter;
  oBiosFilter.set_name(_("Gameboy Advance BIOS"));
  for (guint i = 0; i < G_N_ELEMENTS(acsPattern); i++)
  {
    oBiosFilter.add_pattern(acsPattern[i]);
  }
#else
  const Glib::RefPtr<Gtk::FileFilter> oAllFilter = Gtk::FileFilter::create();
  oAllFilter->set_name(_("All files"));
  oAllFilter->add_pattern("*");

  const Glib::RefPtr<Gtk::FileFilter> oBiosFilter = Gtk::FileFilter::create();
  oBiosFilter->set_name(_("Gameboy Advance BIOS"));
  for (guint i = 0; i < G_N_ELEMENTS(acsPattern); i++)
  {
    oBiosFilter->add_pattern(acsPattern[i]);
  }
#endif

  m_poBiosFileChooserButton->add_filter(oAllFilter);
  m_poBiosFileChooserButton->add_filter(oBiosFilter);
  m_poBiosFileChooserButton->set_filter(oBiosFilter);

  bool bEnableRTC = m_poConfig->oGetKey<bool>("enable_rtc");
  m_poRTCCheckButton->set_active(bEnableRTC);
}

void GameBoyAdvanceConfigDialog::vOnSaveTypeChanged()
{
  int iSaveType = m_poSaveTypeComboBox->get_active_row_number();
  m_poConfig->vSetKey("save_type", iSaveType);
  m_poWindow->vApplyConfigGBASaveType();
}

void GameBoyAdvanceConfigDialog::vOnFlashSizeChanged()
{
  int iFlashSize = m_poFlashSizeComboBox->get_active_row_number();
  if (iFlashSize == 0)
  {
    m_poConfig->vSetKey("flash_size", 64);
  }
  else
  {
    m_poConfig->vSetKey("flash_size", 128);
  }

  m_poWindow->vApplyConfigGBAFlashSize();
}

void GameBoyAdvanceConfigDialog::vOnUseBiosChanged()
{
  bool bUseBios = m_poBiosCheckButton->get_active();
  m_poConfig->vSetKey("use_bios_file", bUseBios);
  m_poBiosFileChooserButton->set_sensitive(bUseBios);
}

void GameBoyAdvanceConfigDialog::vOnBiosSelectionChanged()
{
  std::string sBios = m_poBiosFileChooserButton->get_filename();
  m_poConfig->vSetKey("bios_file", sBios);
}

void GameBoyAdvanceConfigDialog::vOnEnableRTCChanged()
{
  bool bEnableRTC = m_poRTCCheckButton->get_active();
  m_poConfig->vSetKey("enable_rtc", bEnableRTC);
}

} // namespace VBA
