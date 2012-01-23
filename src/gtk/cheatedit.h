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

#ifndef __VBA_CHEATEDIT_H__
#define __VBA_CHEATEDIT_H__

#include <gtkmm/combobox.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/textview.h>
#include <gtkmm/treemodel.h>

#include "window.h"

namespace VBA
{

enum ECheatType
{
  CheatGeneric,
  CheatGSA,
  CheatCBA,
  CheatGS,
  CheatGG
};

class EditCheatCodeColumns : public Gtk::TreeModel::ColumnRecord
{
  public:
    EditCheatCodeColumns()
    {
      add(uText);
      add(iType);
    }

    ~EditCheatCodeColumns() {}

    Gtk::TreeModelColumn<Glib::ustring> uText;
    Gtk::TreeModelColumn<ECheatType> iType;
};

class CheatEditDialog : public Gtk::Dialog
{
public:
  CheatEditDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder);
  Glib::RefPtr<Gtk::TextBuffer> vGetCode();
  Glib::ustring vGetDesc();
  ECheatType vGetType();
  void vSetWindow(VBA::Window * _poWindow);

private:
  void vOnApply();
  void vOnCancel();

  VBA::Window *                 m_poWindow;

  Gtk::Entry *                  m_poCheatDescEntry;
  Gtk::ComboBox *               m_poCheatTypeComboBox;
  Gtk::TextView *               m_poCheatInputTextView;
  Gtk::Button *                 m_poCheatApplyButton;
  Gtk::Button *                 m_poCheatCancelButton;
  Glib::RefPtr<Gtk::TextBuffer> m_poCheatInputBuffer;
  Glib::RefPtr<Gtk::ListStore>  m_poCheatTypeStore;
  EditCheatCodeColumns          m_oTypeModel;
};

} // namespace VBA


#endif
