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

#include "displayconfig.h"

#include <gtkmm/stock.h>
#include <gtkmm/frame.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/liststore.h>
#include <gtkmm/radiobutton.h>

#include "intl.h"
#include "filters.h"

namespace VBA
{

DisplayConfigDialog::DisplayConfigDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  Gtk::Dialog(_pstDialog),
  m_poConfig(0)
{
  refBuilder->get_widget("FiltersComboBox", m_poFiltersComboBox);
  refBuilder->get_widget("IBFiltersComboBox", m_poIBFiltersComboBox);
  refBuilder->get_widget("DefaultScaleComboBox", m_poDefaultScaleComboBox);
  refBuilder->get_widget("OutputOpenGL", m_poOutputOpenGLRadioButton);
  refBuilder->get_widget("OutputCairo", m_poOutputCairoRadioButton);

  m_poFiltersComboBox->signal_changed().connect(sigc::mem_fun(*this, &DisplayConfigDialog::vOnFilterChanged));
  m_poIBFiltersComboBox->signal_changed().connect(sigc::mem_fun(*this, &DisplayConfigDialog::vOnFilterIBChanged));
  m_poDefaultScaleComboBox->signal_changed().connect(sigc::mem_fun(*this, &DisplayConfigDialog::vOnScaleChanged));
  m_poOutputOpenGLRadioButton->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &DisplayConfigDialog::vOnOutputChanged), VBA::Window::OutputOpenGL));
  m_poOutputCairoRadioButton->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &DisplayConfigDialog::vOnOutputChanged), VBA::Window::OutputCairo));


  // Populate the filters combobox
  Glib::RefPtr<Gtk::ListStore> poFiltersListStore;
  poFiltersListStore = Glib::RefPtr<Gtk::ListStore>::cast_static(refBuilder->get_object("FiltersListStore"));

  for (guint i = FirstFilter; i <= LastFilter; i++)
  {
    Gtk::TreeModel::Row row = *(poFiltersListStore->append());
    row->set_value(0, std::string(pcsGetFilterName((EFilter)i)));
  }

  // Populate the interframe blending filters combobox
  Glib::RefPtr<Gtk::ListStore> poIBFiltersListStore;
  poIBFiltersListStore = Glib::RefPtr<Gtk::ListStore>::cast_static(refBuilder->get_object("IBFiltersListStore"));

  for (guint i = FirstFilterIB; i <= LastFilterIB; i++)
  {
    Gtk::TreeModel::Row row = *(poIBFiltersListStore->append());
    row->set_value(0, std::string(pcsGetFilterIBName((EFilterIB)i)));
  }
}

void DisplayConfigDialog::vSetConfig(Config::Section * _poConfig, VBA::Window * _poWindow)
{
  m_poConfig = _poConfig;
  m_poWindow = _poWindow;

  int iDefaultFilter = m_poConfig->oGetKey<int>("filter2x");
  m_poFiltersComboBox->set_active(iDefaultFilter);

  int iDefaultFilterIB = m_poConfig->oGetKey<int>("filterIB");
  m_poIBFiltersComboBox->set_active(iDefaultFilterIB);

  int iDefaultScale = m_poConfig->oGetKey<int>("scale");
  m_poDefaultScaleComboBox->set_active(iDefaultScale - 1);

  // Set the default output module
  VBA::Window::EVideoOutput _eOutput = (VBA::Window::EVideoOutput)m_poConfig->oGetKey<int>("output");
  switch (_eOutput)
  {
    case VBA::Window::OutputOpenGL:
      m_poOutputOpenGLRadioButton->set_active();
      break;
    default:
      m_poOutputCairoRadioButton->set_active();
      break;
  }
}

void DisplayConfigDialog::vOnFilterChanged()
{
  int iFilter = m_poFiltersComboBox->get_active_row_number();
  if (iFilter >= 0)
  {
    m_poConfig->vSetKey("filter2x", iFilter);
    m_poWindow->vApplyConfigFilter();
  }
}

void DisplayConfigDialog::vOnFilterIBChanged()
{
  int iFilterIB = m_poIBFiltersComboBox->get_active_row_number();
  if (iFilterIB >= 0)
  {
    m_poConfig->vSetKey("filterIB", iFilterIB);
    m_poWindow->vApplyConfigFilterIB();
  }
}

void DisplayConfigDialog::vOnOutputChanged(VBA::Window::EVideoOutput _eOutput)
{
  VBA::Window::EVideoOutput eOldOutput = (VBA::Window::EVideoOutput)m_poConfig->oGetKey<int>("output");

  if (_eOutput == eOldOutput)
    return;
  
  if (_eOutput == VBA::Window::OutputOpenGL && m_poOutputOpenGLRadioButton->get_active())
  {
    m_poConfig->vSetKey("output", VBA::Window::OutputOpenGL);
    m_poWindow->vApplyConfigScreenArea();
  } else if (_eOutput == VBA::Window::OutputCairo && m_poOutputCairoRadioButton->get_active())
  {
    m_poConfig->vSetKey("output", VBA::Window::OutputCairo);
    m_poWindow->vApplyConfigScreenArea();
  }
}

void DisplayConfigDialog::vOnScaleChanged()
{
  int iScale = m_poDefaultScaleComboBox->get_active_row_number() + 1;
  if (iScale > 0)
  {
    m_poConfig->vSetKey("scale", iScale);
    m_poWindow->vUpdateScreen();
  }
}

} // namespace VBA
