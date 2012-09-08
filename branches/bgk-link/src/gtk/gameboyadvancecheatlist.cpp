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

#include "gameboyadvancecheatlist.h"
#include "tools.h"

#include <vector>

namespace VBA
{

GameBoyAdvanceCheatListDialog::GameBoyAdvanceCheatListDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder) :
  CheatListDialog(_pstDialog, refBuilder)
{
  vUpdateList();
}

void GameBoyAdvanceCheatListDialog::vAddCheat(Glib::ustring sDesc, ECheatType type, Glib::RefPtr<Gtk::TextBuffer> buffer)
{
  int previous = cheatsNumber;

  switch (type)
  {
  // Generic Code
  case CheatGeneric:
  {
    std::vector<Glib::ustring> tokens;

    vTokenize(buffer->get_text(), tokens);

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

    Glib::ustring sToken;
    Glib::ustring sCode;
    Glib::ustring sPart = "";

    vTokenize(buffer->get_text(), tokens);

    for (std::vector<Glib::ustring>::iterator it = tokens.begin();
        it != tokens.end();
        it++)
    {

      sToken = it->uppercase();
      const char *cToken = sToken.c_str();

      if (sToken.size() == 16)
        cheatsAddGSACode(cToken, sDesc.c_str(), false);
      else if (sToken.size() == 12)
      {
        sCode = sToken.substr(0,8);
        sCode += " ";
        sCode += sToken.substr(9,4);
        cheatsAddCBACode(sCode.c_str(), sDesc.c_str());
      }
      else
        if (sPart.empty())
          sPart = sToken;
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
  default:; // silence warnings
  } // end of switch

  vUpdateList(previous);
}

bool GameBoyAdvanceCheatListDialog::vCheatListOpen(const char *file)
{
  return cheatsLoadCheatList(file);
}

void GameBoyAdvanceCheatListDialog::vCheatListSave(const char *file)
{
  cheatsSaveCheatList(file);
}

void GameBoyAdvanceCheatListDialog::vRemoveCheat(int index) {
  cheatsDelete(index, false);
}

void GameBoyAdvanceCheatListDialog::vRemoveAllCheats() {
  cheatsDeleteAll(false);
}

void GameBoyAdvanceCheatListDialog::vToggleCheat(int index, bool enable) {
  if (enable)
    cheatsEnable(index);
  else
    cheatsDisable(index);
}

void GameBoyAdvanceCheatListDialog::vUpdateList(int previous)
{
  for (int i = previous; i < cheatsNumber;  i++)
  {
    // Add row for each newly added cheat
    Gtk::TreeModel::Row row = *(m_poCheatListStore->append());

    row[m_oRecordModel.iIndex] = i;
    row[m_oRecordModel.bEnabled] = cheatsList[i].enabled;
    row[m_oRecordModel.uDesc] = cheatsList[i].desc;
  }
}

} // namespace VBA
