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

#include "cheatedit.h"

#include "intl.h"

namespace VBA
{

class CheatCodeColumns : public Gtk::TreeModel::ColumnRecord
{
  public:
    CheatCodeColumns()
    {
      add(uText);
      add(iType);
    }

    ~CheatCodeColumns() {}

    Gtk::TreeModelColumn<Glib::ustring> uText;
    Gtk::TreeModelColumn<ECheatType> iType;
};

CheatCodeColumns cTypeModel;

/**
 * GameBoyAdvanceCheatEditDialog
 *
 * A unified cheat editing dialog for multiple code types.
 */
CheatEditDialog::CheatEditDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  Gtk::Dialog(_pstDialog)
{
  refBuilder->get_widget("CheatDescEntry", m_poCheatDescEntry);
  refBuilder->get_widget("CheatTypeComboBox", m_poCheatTypeComboBox);
  refBuilder->get_widget("CheatInputTextView", m_poCheatInputTextView);
  refBuilder->get_widget("CheatApplyButton", m_poCheatApplyButton);
  refBuilder->get_widget("CheatCancelButton", m_poCheatCancelButton);

  // Tree View model
  m_poCheatTypeStore = Gtk::ListStore::create(cTypeModel);

  m_poCheatTypeComboBox->set_model(m_poCheatTypeStore);

  Gtk::TreeModel::Row row = *(m_poCheatTypeStore->append());

  row[cTypeModel.iType] = CheatGeneric;
  row[cTypeModel.uText] = "Generic Code";

  row = *(m_poCheatTypeStore->append());

  row[cTypeModel.iType] = CheatGSA;
  row[cTypeModel.uText] = "Gameshark Advance";

  row = *(m_poCheatTypeStore->append());

  row[cTypeModel.iType] = CheatCBA;
  row[cTypeModel.uText] = "CodeBreaker Advance";

  m_poCheatTypeComboBox->set_active(CheatGeneric);

  m_poCheatApplyButton->signal_clicked().connect(sigc::mem_fun(*this, &CheatEditDialog::vOnApply));
  m_poCheatCancelButton->signal_clicked().connect(sigc::mem_fun(*this, &CheatEditDialog::vOnCancel));
}

Glib::RefPtr<Gtk::TextBuffer> CheatEditDialog::vGetCode()
{
  return m_poCheatInputTextView->get_buffer();
}

Glib::ustring CheatEditDialog::vGetDesc()
{
  return m_poCheatDescEntry->get_text();
}

ECheatType CheatEditDialog::vGetType()
{
  Gtk::TreeModel::iterator iter = m_poCheatTypeComboBox->get_active();

  if (iter)
  {
    Gtk::TreeModel::Row row = *iter;

    return row[cTypeModel.iType];
  }

  return CheatGeneric;
}

void CheatEditDialog::vOnApply()
{
  response(Gtk::RESPONSE_APPLY);
}

void CheatEditDialog::vOnCancel() {
  response(Gtk::RESPONSE_CANCEL);
}

} // namespace VBA
