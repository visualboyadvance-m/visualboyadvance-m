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

#include "gameboycheatlist.h"
#include "tools.h"

#include <gtkmm/stock.h>

#include "intl.h"
#include <vector>

namespace VBA
{

GameBoyCheatListDialog::GameBoyCheatListDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  Gtk::Dialog(_pstDialog)
{
  refBuilder->get_widget("CheatOpenButton", m_poCheatOpenButton);
  refBuilder->get_widget("CheatSaveButton", m_poCheatSaveButton);
  refBuilder->get_widget("CheatAddButton", m_poCheatAddButton);
  refBuilder->get_widget("CheatRemoveButton", m_poCheatRemoveButton);
  refBuilder->get_widget("CheatRemoveAllButton", m_poCheatRemoveAllButton);
  refBuilder->get_widget("CheatMarkAllButton", m_poCheatMarkAllButton);
  refBuilder->get_widget("CheatTreeView", m_poCheatTreeView);

  // Tree View model
  m_poCheatListStore = Gtk::ListStore::create(m_oRecordModel);

  m_poCheatTreeView->set_model(m_poCheatListStore);

  Gtk::CellRendererToggle* pRenderer = Gtk::manage(new Gtk::CellRendererToggle());

  int cols_count =  m_poCheatTreeView->append_column("", *pRenderer);

  pRenderer->signal_toggled().connect(sigc::mem_fun(*this, &GameBoyCheatListDialog::vOnCheatToggled));

  Gtk::TreeViewColumn* pColumn = m_poCheatTreeView->get_column(cols_count - 1);

  if (pColumn)
    pColumn->add_attribute(pRenderer->property_active(), m_oRecordModel.bEnabled);

  m_poCheatTreeView->append_column("Description", m_oRecordModel.uDesc);

  m_poCheatOpenButton->signal_clicked().connect(sigc::mem_fun(*this, &GameBoyCheatListDialog::vOnCheatOpen));
  m_poCheatSaveButton->signal_clicked().connect(sigc::mem_fun(*this, &GameBoyCheatListDialog::vOnCheatSave));
  m_poCheatAddButton->signal_clicked().connect(sigc::mem_fun(*this, &GameBoyCheatListDialog::vOnCheatAdd));
  m_poCheatRemoveButton->signal_clicked().connect(sigc::mem_fun(*this, &GameBoyCheatListDialog::vOnCheatRemove));
  m_poCheatRemoveAllButton->signal_clicked().connect(sigc::mem_fun(*this, &GameBoyCheatListDialog::vOnCheatRemoveAll));
  m_poCheatMarkAllButton->signal_clicked().connect(sigc::mem_fun(*this, &GameBoyCheatListDialog::vOnCheatMarkAll));

  bMark = false;

  vUpdateList();
}

void GameBoyCheatListDialog::vOnCheatOpen()
{
  Gtk::FileChooserDialog oDialog(*this, _("Open cheat list"));
  oDialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  oDialog.add_button(Gtk::Stock::OPEN,   Gtk::RESPONSE_OK);

  oDialog.set_current_folder(Glib::get_home_dir());

  while (oDialog.run() == Gtk::RESPONSE_OK)
  {
    // delete existing cheats before loading the list
    gbCheatRemoveAll();

    m_poCheatListStore->clear();

    if (gbCheatsLoadCheatList(oDialog.get_filename().c_str()))
    {
      vUpdateList();
      break;
    }
  }
}

void GameBoyCheatListDialog::vOnCheatSave()
{
  Gtk::FileChooserDialog sDialog(*this, _("Save cheat list"));
  sDialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  sDialog.add_button(Gtk::Stock::SAVE,   Gtk::RESPONSE_OK);

  sDialog.set_current_folder(Glib::get_home_dir());

  if (sDialog.run() == Gtk::RESPONSE_OK)
    gbCheatsSaveCheatList(sDialog.get_filename().c_str());
}

void GameBoyCheatListDialog::vOnCheatAdd()
{
  std::string sUiFile = VBA::Window::sGetUiFilePath("cheatedit.ui");
  Glib::RefPtr<Gtk::Builder> poBuilder = Gtk::Builder::create_from_file(sUiFile);

  CheatEditDialog * poDialog = 0;
  poBuilder->get_widget_derived("CheatEditDialog", poDialog);
  poDialog->set_transient_for(*this);
  poDialog->vSetWindow(m_poWindow);
  int response = poDialog->run();
  poDialog->hide();

  if (response == Gtk::RESPONSE_APPLY)
  {
    Glib::ustring sDesc = poDialog->vGetDesc();

    int previous = gbCheatNumber;

    switch (poDialog->vGetType())
    {
    // GameShark
    case CheatGS:
    {
      std::vector<Glib::ustring> tokens;
      Glib::RefPtr<Gtk::TextBuffer> code_buffer = poDialog->vGetCode();

      vTokenize(code_buffer->get_text(), tokens);

      for (std::vector<Glib::ustring>::iterator it = tokens.begin();
          it != tokens.end();
          it++)
      {
        Glib::ustring sToken = it->uppercase();

        gbAddGsCheat(sToken.c_str(), sDesc.c_str());
      }

      break;
    }
    // GameGenie
    case CheatGG:
    {
      std::vector<Glib::ustring> tokens;
      Glib::RefPtr<Gtk::TextBuffer> code_buffer = poDialog->vGetCode();

      vTokenize(code_buffer->get_text(), tokens);

      for (std::vector<Glib::ustring>::iterator it = tokens.begin();
          it != tokens.end();
          it++)
      {

        Glib::ustring sToken = it->uppercase();

        gbAddGgCheat(sToken.c_str(), sDesc.c_str());
      }

      break;
    }
    default:; // silence warnings
    }
    // end of switch

    vUpdateList(previous);

  } // end of condition
}

void GameBoyCheatListDialog::vOnCheatRemove()
{
  Gtk::TreeModel::iterator iter = m_poCheatTreeView->get_selection()->get_selected();

  if (iter)
  {
    Gtk::TreeModel::Row row = *iter;

    gbCheatRemove(row[m_oRecordModel.iIndex]);

    m_poCheatListStore->erase(iter);
  }
}

void GameBoyCheatListDialog::vOnCheatRemoveAll()
{
  gbCheatRemoveAll();

  m_poCheatListStore->clear();
}

void GameBoyCheatListDialog::vOnCheatMarkAll()
{
  Gtk::TreeModel::Children cListEntries = m_poCheatListStore->children();

  for (Gtk::TreeModel::iterator iter = cListEntries.begin(); iter; iter++)
  {
    Gtk::TreeModel::Row row = *iter;

    row[m_oRecordModel.bEnabled] = bMark;

    vToggleCheat(row[m_oRecordModel.iIndex], row[m_oRecordModel.bEnabled]);
  }

  bMark = !bMark;
}

void GameBoyCheatListDialog::vOnCheatToggled(Glib::ustring const& string_path)
{
  Gtk::TreeIter iter = m_poCheatListStore->get_iter(string_path);

  Gtk::TreeModel::Row row = *iter;

  row[m_oRecordModel.bEnabled] = !row[m_oRecordModel.bEnabled];

  vToggleCheat(row[m_oRecordModel.iIndex], row[m_oRecordModel.bEnabled]);
}

void GameBoyCheatListDialog::vSetWindow(VBA::Window * _poWindow)
{
  m_poWindow = _poWindow;
}

void GameBoyCheatListDialog::vToggleCheat(int index, bool enable) {
  if (enable)
    gbCheatEnable(index);
  else
    gbCheatDisable(index);
}

void GameBoyCheatListDialog::vUpdateList(int previous)
{
  for (int i = previous; i < gbCheatNumber;  i++)
  {
    // Add row for each newly added cheat
    Gtk::TreeModel::Row row = *(m_poCheatListStore->append());

    row[m_oRecordModel.iIndex] = i;
    row[m_oRecordModel.bEnabled] = gbCheatList[i].enabled;
    row[m_oRecordModel.uDesc] = gbCheatList[i].cheatDesc;
  }
}

} // namespace VBA
