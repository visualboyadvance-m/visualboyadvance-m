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

#include "gameboyconfig.h"

#include <gtkmm/stock.h>
#include <gtkmm/frame.h>
#include <gtkmm/liststore.h>

#include "intl.h"

namespace VBA
{

static const VBA::Window::EEmulatorType aEmulatorType[] =
{
  VBA::Window::EmulatorAuto,
  VBA::Window::EmulatorCGB,
  VBA::Window::EmulatorSGB,
  VBA::Window::EmulatorGB,
  VBA::Window::EmulatorGBA,
  VBA::Window::EmulatorSGB2
};

GameBoyConfigDialog::GameBoyConfigDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  Gtk::Dialog(_pstDialog),
  m_poConfig(0)
{
  refBuilder->get_widget("SystemComboBox", m_poSystemComboBox);
  refBuilder->get_widget("BorderCheckButton", m_poBorderCheckButton);
  refBuilder->get_widget("PrinterCheckButton", m_poPrinterCheckButton);
  refBuilder->get_widget("BootRomCheckButton", m_poBootRomCheckButton);
  refBuilder->get_widget("BootRomFileChooserButton", m_poBootRomFileChooserButton);

  m_poSystemComboBox->signal_changed().connect(sigc::mem_fun(*this, &GameBoyConfigDialog::vOnSystemChanged));
  m_poBorderCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &GameBoyConfigDialog::vOnBorderChanged));
  m_poPrinterCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &GameBoyConfigDialog::vOnPrinterChanged));
  m_poBootRomCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &GameBoyConfigDialog::vOnUseBootRomChanged));
  m_poBootRomFileChooserButton->signal_selection_changed().connect(sigc::mem_fun(*this, &GameBoyConfigDialog::vOnBootRomSelectionChanged));
}

void GameBoyConfigDialog::vSetConfig(Config::Section * _poConfig, VBA::Window * _poWindow)
{
  m_poConfig = _poConfig;
  m_poWindow = _poWindow;

  VBA::Window::EEmulatorType eDefaultEmulatorType = (VBA::Window::EEmulatorType)m_poConfig->oGetKey<int>("emulator_type");
  m_poSystemComboBox->set_active(aEmulatorType[eDefaultEmulatorType]);

  bool bBorder = m_poConfig->oGetKey<bool>("gb_border");
  m_poBorderCheckButton->set_active(bBorder);

  bool bPrinter = m_poConfig->oGetKey<bool>("gb_printer");
  m_poPrinterCheckButton->set_active(bPrinter);
  
  bool bUseBootRom = m_poConfig->oGetKey<bool>("gb_use_bios_file");
  m_poBootRomCheckButton->set_active(bUseBootRom);
  m_poBootRomFileChooserButton->set_sensitive(bUseBootRom);
  
  std::string sBootRom = m_poConfig->oGetKey<std::string>("gb_bios_file");
  m_poBootRomFileChooserButton->set_filename(sBootRom);
}

void GameBoyConfigDialog::vOnSystemChanged()
{
  int iSystem = m_poSystemComboBox->get_active_row_number();
  m_poConfig->vSetKey("emulator_type", aEmulatorType[iSystem]);
  
  m_poWindow->vApplyConfigGBSystem();
}

void GameBoyConfigDialog::vOnBorderChanged()
{
  bool bBorder = m_poBorderCheckButton->get_active();
  m_poConfig->vSetKey("gb_border", bBorder);
  m_poWindow->vApplyConfigGBBorder();
}

void GameBoyConfigDialog::vOnPrinterChanged()
{
  bool bPrinter = m_poPrinterCheckButton->get_active();
  m_poConfig->vSetKey("gb_printer", bPrinter);
  m_poWindow->vApplyConfigGBPrinter();
}

void GameBoyConfigDialog::vOnUseBootRomChanged()
{
  bool bUseBootRom = m_poBootRomCheckButton->get_active();
  m_poConfig->vSetKey("gb_use_bios_file", bUseBootRom);
  m_poBootRomFileChooserButton->set_sensitive(bUseBootRom);
}

void GameBoyConfigDialog::vOnBootRomSelectionChanged()
{
  std::string sBootRom = m_poBootRomFileChooserButton->get_filename();
  m_poConfig->vSetKey("gb_bios_file", sBootRom);
}

} // namespace VBA
