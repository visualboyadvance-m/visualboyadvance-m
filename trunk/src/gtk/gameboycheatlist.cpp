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

#include "gameboycheatlist.h"
#include "tools.h"

#include <vector>

namespace VBA
{

GameBoyCheatListDialog::GameBoyCheatListDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  CheatListDialog(_pstDialog, refBuilder)
{
  vUpdateList();
}

void GameBoyCheatListDialog::vAddCheat(Glib::ustring sDesc, ECheatType type, Glib::RefPtr<Gtk::TextBuffer> buffer)
{
  int previous = gbCheatNumber;

  switch (type)
  {
  // GameShark
  case CheatGS:
  {
    std::vector<Glib::ustring> tokens;

    vTokenize(buffer->get_text(), tokens);

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

    vTokenize(buffer->get_text(), tokens);

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
}

bool GameBoyCheatListDialog::vCheatListOpen(const char *file)
{
  return gbCheatsLoadCheatList(file);
}

void GameBoyCheatListDialog::vCheatListSave(const char *file)
{
  gbCheatsSaveCheatList(file);
}

void GameBoyCheatListDialog::vRemoveCheat(int index)
{
  gbCheatRemove(index);
}

void GameBoyCheatListDialog::vRemoveAllCheats()
{
  gbCheatRemoveAll();
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
