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

#ifndef __VBA_CHEATLIST_H__
#define __VBA_CHEATLIST_H__

#include <gtkmm/toolbutton.h>

#include "cheatedit.h"

#include "window.h"

namespace VBA
{

class ListCheatCodeColumns : public Gtk::TreeModel::ColumnRecord
{
  public:
    ListCheatCodeColumns()
    {
      add(iIndex);
      add(bEnabled);
      add(uDesc);
    }

    ~ListCheatCodeColumns() {}

    Gtk::TreeModelColumn<int> iIndex;
    Gtk::TreeModelColumn<bool> bEnabled;
    Gtk::TreeModelColumn<Glib::ustring> uDesc;
};

class CheatListDialog : public Gtk::Dialog
{
public:
  CheatListDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder);
  void vSetWindow(VBA::Window * _poWindow);

protected:
  virtual void vAddCheat(Glib::ustring sDesc, ECheatType type, Glib::RefPtr<Gtk::TextBuffer> buffer) = 0;
  virtual bool vCheatListOpen(const char *file) = 0;
  virtual void vCheatListSave(const char *file) = 0;
  virtual void vRemoveCheat(int index) = 0;
  virtual void vRemoveAllCheats() = 0;
  virtual void vToggleCheat(int index, bool enable) = 0;
  virtual void vUpdateList(int previous = 0) = 0;

  Glib::RefPtr<Gtk::ListStore>  m_poCheatListStore;
  ListCheatCodeColumns          m_oRecordModel;

private:
  void vOnCheatListOpen();
  void vOnCheatListSave();
  void vOnCheatAdd();
  void vOnCheatRemove();
  void vOnCheatRemoveAll();
  void vOnCheatMarkAll();
  void vOnCheatToggled(Glib::ustring const& string_path);

  VBA::Window *                 m_poWindow;

  Gtk::ToolButton *             m_poCheatOpenButton;
  Gtk::ToolButton *             m_poCheatSaveButton;
  Gtk::ToolButton *             m_poCheatAddButton;
  Gtk::ToolButton *             m_poCheatRemoveButton;
  Gtk::ToolButton *             m_poCheatRemoveAllButton;
  Gtk::ToolButton *             m_poCheatMarkAllButton;
  Gtk::TreeView *               m_poCheatTreeView;
  
  bool bMark;
};

} // namespace VBA


#endif
