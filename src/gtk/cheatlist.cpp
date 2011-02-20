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

#include "cheatlist.h"

#include "intl.h"
#include "stringtokenizer.h"
#include <vector>

namespace VBA
{

class CheatCodeColumns : public Gtk::TreeModel::ColumnRecord
{
  public:
    CheatCodeColumns()
    {
      add(iIndex);
      add(bEnabled);
      add(uDesc);
    }

    ~CheatCodeColumns() {}

    Gtk::TreeModelColumn<int> iIndex;
    Gtk::TreeModelColumn<bool> bEnabled;
    Gtk::TreeModelColumn<Glib::ustring> uDesc;
};

CheatCodeColumns cRecordModel;

CheatListDialog::CheatListDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
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
  m_poCheatListStore = Gtk::ListStore::create(cRecordModel);

  m_poCheatTreeView->set_model(m_poCheatListStore);

  Gtk::CellRendererToggle* pRenderer = Gtk::manage(new Gtk::CellRendererToggle());

  int cols_count =  m_poCheatTreeView->append_column("", *pRenderer);

  pRenderer->signal_toggled().connect(sigc::mem_fun(*this, &CheatListDialog::vOnCheatToggled));

  Gtk::TreeViewColumn* pColumn = m_poCheatTreeView->get_column(cols_count - 1);

  if (pColumn)
    pColumn->add_attribute(pRenderer->property_active(), cRecordModel.bEnabled);

  m_poCheatTreeView->append_column("Description", cRecordModel.uDesc);

  m_poCheatOpenButton->signal_clicked().connect(sigc::mem_fun(*this, &CheatListDialog::vOnCheatOpen));
  m_poCheatSaveButton->signal_clicked().connect(sigc::mem_fun(*this, &CheatListDialog::vOnCheatSave));
  m_poCheatAddButton->signal_clicked().connect(sigc::mem_fun(*this, &CheatListDialog::vOnCheatAdd));
  m_poCheatRemoveButton->signal_clicked().connect(sigc::mem_fun(*this, &CheatListDialog::vOnCheatRemove));
  m_poCheatRemoveAllButton->signal_clicked().connect(sigc::mem_fun(*this, &CheatListDialog::vOnCheatRemoveAll));
  m_poCheatMarkAllButton->signal_clicked().connect(sigc::mem_fun(*this, &CheatListDialog::vOnCheatMarkAll));

  bMark = false;

  vUpdateList();
}

void CheatListDialog::vOnCheatOpen()
{
  Gtk::FileChooserDialog oDialog(*this, _("Open cheat list"));
  oDialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  oDialog.add_button(Gtk::Stock::OPEN,   Gtk::RESPONSE_OK);

  oDialog.set_current_folder(Glib::get_home_dir()); // TODO: remember dir?

  while (oDialog.run() == Gtk::RESPONSE_OK)
  {
    // delete existing cheats before loading the list
    cheatsDeleteAll(false);

    m_poCheatListStore->clear();

    if (cheatsLoadCheatList(oDialog.get_filename().c_str()))
    {
      vUpdateList();
      break;
    }
  }
}

void CheatListDialog::vOnCheatSave()
{
  Gtk::FileChooserDialog sDialog(*this, _("Save cheat list"));
  sDialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  sDialog.add_button(Gtk::Stock::SAVE,   Gtk::RESPONSE_OK);

  sDialog.set_current_folder(Glib::get_home_dir());

  if (sDialog.run() == Gtk::RESPONSE_OK)
    cheatsSaveCheatList(sDialog.get_filename().c_str());
}

void CheatListDialog::vOnCheatAdd()
{
  std::string sUiFile = VBA::Window::sGetUiFilePath("cheatedit.ui");
  Glib::RefPtr<Gtk::Builder> poBuilder = Gtk::Builder::create_from_file(sUiFile);

  CheatEditDialog * poDialog = 0;
  poBuilder->get_widget_derived("CheatEditDialog", poDialog);
  poDialog->set_transient_for(*this);
  int response = poDialog->run();
  poDialog->hide();

  if (response == Gtk::RESPONSE_APPLY)
  {
    Glib::ustring sDesc = poDialog->vGetDesc();

    int previous = cheatsNumber;

    switch (poDialog->vGetType())
    {
    // Generic Code
    case CheatGeneric:
    {
      std::vector<Glib::ustring> tokens;
      Glib::RefPtr<Gtk::TextBuffer> code_buffer = poDialog->vGetCode();

      StringTokenizer::tokenize(code_buffer->get_text(), tokens);

      for (std::vector<Glib::ustring>::iterator it = tokens.begin();
          it != tokens.end();
          it++)
      {
        Glib::ustring sToken = it->uppercase();

        cheatsAddCheatCode(sToken.c_str(), sDesc.c_str());
      }

      break;
    }
    // Gameshark Advance & CodeBreaker Advance
    case CheatGSA:
    case CheatCBA:
    {
      std::vector<Glib::ustring> tokens;
      Glib::RefPtr<Gtk::TextBuffer> code_buffer = poDialog->vGetCode();

      Glib::ustring sToken;
      Glib::ustring sCode;
      Glib::ustring sPart = "";

      StringTokenizer::tokenize(code_buffer->get_text(), tokens);

      for (std::vector<Glib::ustring>::iterator it = tokens.begin();
          it != tokens.end();
          it++)
      {

        sToken = it->uppercase();
        const char *cToken = sToken.c_str();

        if (sToken.size() == 16)
        {
          cheatsAddGSACode(cToken, sDesc.c_str(), false);
        }
        else if (sToken.size() == 12)
        {
          sCode = sToken.substr(0,8);
          sCode += " ";
          sCode += sToken.substr(9,4); // TODO: is this safe?
          cheatsAddCBACode(sCode.c_str(), sDesc.c_str());
        }
        else
          if (sPart.empty())
          {
            sPart = sToken;
          }
          else
          {
            if (sToken.size() == 4)
            {
              sCode = sPart;
              sCode += " ";
              sCode += cToken;
              cheatsAddCBACode(sCode.c_str(), sDesc.c_str());
            }
            else
            {
              sCode = sPart + sToken;
              cheatsAddGSACode(sCode.c_str(), sDesc.c_str(), true);
            }

            sPart = "";
          }
      } // end of loop

    } // end of case
    } // end of switch

    vUpdateList(previous);

  } // end of condition
}

void CheatListDialog::vOnCheatRemove()
{
  Gtk::TreeModel::iterator iter = m_poCheatTreeView->get_selection()->get_selected();

  if (iter)
  {
    Gtk::TreeModel::Row row = *iter;

    cheatsDelete(row[cRecordModel.iIndex], false);

    m_poCheatListStore->erase(iter);
  }
}

void CheatListDialog::vOnCheatRemoveAll()
{
  cheatsDeleteAll(false);

  m_poCheatListStore->clear();
}

void CheatListDialog::vOnCheatMarkAll()
{
  Gtk::TreeModel::Children cListEntries = m_poCheatListStore->children();

  for (Gtk::TreeModel::iterator iter = cListEntries.begin(); iter; iter++)
  {
    Gtk::TreeModel::Row row = *iter;

    row[cRecordModel.bEnabled] = bMark;

    vToggleCheat(row[cRecordModel.iIndex], row[cRecordModel.bEnabled]);
  }

  bMark = !bMark;
}

void CheatListDialog::vOnCheatToggled(Glib::ustring const& string_path)
{
  Gtk::TreeIter iter = m_poCheatListStore->get_iter(string_path);

  Gtk::TreeModel::Row row = *iter;

  row[cRecordModel.bEnabled] = !row[cRecordModel.bEnabled];

  vToggleCheat(row[cRecordModel.iIndex], row[cRecordModel.bEnabled]);
}

void CheatListDialog::vToggleCheat(int index, bool enable) {
  if (enable)
    cheatsEnable(index);
  else
    cheatsDisable(index);
}

void CheatListDialog::vUpdateList(int previous)
{
  for (int i = previous; i < cheatsNumber;  i++)
  {
    // Add row for each newly added cheat
    Gtk::TreeModel::Row row = *(m_poCheatListStore->append());

    row[cRecordModel.iIndex] = i;
    row[cRecordModel.bEnabled] = cheatsList[i].enabled;
    row[cRecordModel.uDesc] = cheatsList[i].desc;
  }
}

} // namespace VBA
