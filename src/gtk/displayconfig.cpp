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

#include "displayconfig.h"

#include <gtkmm/stock.h>
#include <gtkmm/frame.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/liststore.h>

#include "intl.h"
#include "filters.h"

namespace VBA
{

DisplayConfigDialog::DisplayConfigDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  Gtk::Dialog(_pstDialog),
  m_poConfig(0)
{
  //TODO Move to filters.h
  struct
  {
    const char *    m_csName;
    const EFilter2x m_eFilter;
  }
  astFilter[] =
  {
    { "None",                FilterNone         },
    { "2xSaI",               Filter2xSaI        },
    { "Super 2xSaI",         FilterSuper2xSaI   },
    { "Super Eagle",         FilterSuperEagle   },
    { "Pixelate",            FilterPixelate     },
    { "AdvanceMAME Scale2x", FilterAdMame2x     },
    { "Bilinear",            FilterBilinear     },
    { "Bilinear Plus",       FilterBilinearPlus },
    { "Scanlines",           FilterScanlines    },
    { "TV Mode",             FilterScanlinesTV  },
    { "hq2x",                FilterHq2x         },
    { "lq2x",                FilterLq2x         }
  };

  struct
  {
    const char *    m_csName;
    const EFilterIB m_eFilterIB;
  }
  astFilterIB[] =
  {
    { "None",                      FilterIBNone       },
    { "Smart interframe blending", FilterIBSmart      },
    { "Interframe motion blur",    FilterIBMotionBlur }
  };

  refBuilder->get_widget("FiltersComboBox", m_poFiltersComboBox);
  refBuilder->get_widget("IBFiltersComboBox", m_poIBFiltersComboBox);
  refBuilder->get_widget("OutputOpenGL", m_poOutputOpenGLRadioButton);
  refBuilder->get_widget("OutputCairo", m_poOutputCairoRadioButton);
  refBuilder->get_widget("OutputXv", m_poOutputXvRadioButton);

  m_poFiltersComboBox->signal_changed().connect(sigc::mem_fun(*this, &DisplayConfigDialog::vOnFilterChanged));
  m_poIBFiltersComboBox->signal_changed().connect(sigc::mem_fun(*this, &DisplayConfigDialog::vOnFilterIBChanged));
  m_poOutputOpenGLRadioButton->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &DisplayConfigDialog::vOnOutputChanged), VBA::Window::OutputOpenGL));
  m_poOutputCairoRadioButton->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &DisplayConfigDialog::vOnOutputChanged), VBA::Window::OutputCairo));
  m_poOutputXvRadioButton->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &DisplayConfigDialog::vOnOutputChanged), VBA::Window::OutputXvideo));


  // Populate the filters combobox
  Glib::RefPtr<Gtk::ListStore> poFiltersListStore;
  poFiltersListStore = Glib::RefPtr<Gtk::ListStore>::cast_static(refBuilder->get_object("FiltersListStore"));

  for (guint i = 0; i < G_N_ELEMENTS(astFilter); i++)
  {
    Gtk::TreeModel::Row row = *(poFiltersListStore->append());
    row->set_value(0, std::string(astFilter[i].m_csName));
  }

  // Populate the interframe blending filters combobox
  Glib::RefPtr<Gtk::ListStore> poIBFiltersListStore;
  poIBFiltersListStore = Glib::RefPtr<Gtk::ListStore>::cast_static(refBuilder->get_object("IBFiltersListStore"));

  for (guint i = 0; i < G_N_ELEMENTS(astFilterIB); i++)
  {
    Gtk::TreeModel::Row row = *(poIBFiltersListStore->append());
    row->set_value(0, std::string(astFilterIB[i].m_csName));
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

  // Set the default output module
  VBA::Window::EVideoOutput _eOutput = (VBA::Window::EVideoOutput)m_poConfig->oGetKey<int>("output");
  switch (_eOutput)
  {
    case VBA::Window::OutputOpenGL:
      m_poOutputOpenGLRadioButton->set_active();
      break;
    case VBA::Window::OutputXvideo:
      m_poOutputXvRadioButton->set_active();
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
  if (_eOutput == VBA::Window::OutputOpenGL && m_poOutputOpenGLRadioButton->get_active())
    m_poConfig->vSetKey("output", VBA::Window::OutputOpenGL);
  else if (_eOutput == VBA::Window::OutputCairo && m_poOutputCairoRadioButton->get_active())
    m_poConfig->vSetKey("output", VBA::Window::OutputCairo);
  else if (_eOutput == VBA::Window::OutputXvideo && m_poOutputXvRadioButton->get_active())
    m_poConfig->vSetKey("output", VBA::Window::OutputXvideo);

  m_poWindow->vApplyConfigScreenArea();
}

} // namespace VBA
