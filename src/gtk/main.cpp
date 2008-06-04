// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

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

#include <limits.h>
#include <stdlib.h>

#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/messagedialog.h>
#include <libglademm.h>

#ifdef USE_OPENGL
#include <gtkglmm.h>
#endif // USE_OPENGL

#include "window.h"
#include "intl.h"

using Gnome::Glade::Xml;

int main(int argc, char * argv[])
{
  bool bShowVersion = false;
  Glib::OptionGroup::vecustrings listRemaining;

#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  textdomain(GETTEXT_PACKAGE);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif // ENABLE_NLS

  Gtk::Main oKit(argc, argv);
  
#ifdef USE_OPENGL
  Gtk::GL::init(argc, argv);
#endif // USE_OPENGL
  
  Glib::OptionContext oContext;
  Glib::OptionGroup oGroup("main_group", _("Main VBA-M options"));
  
  Glib::OptionEntry oVersion;
  oVersion.set_long_name("version");
  oVersion.set_short_name('v');
  oVersion.set_description(_("Output version information."));
  oGroup.add_entry(oVersion, bShowVersion);

  Glib::OptionEntry oFileName;
  oFileName.set_long_name(G_OPTION_REMAINING);
  oFileName.set_description(G_OPTION_REMAINING);
  oGroup.add_entry(oFileName, listRemaining);

  oContext.set_main_group(oGroup);

  try
  {
    oContext.parse(argc, argv);
  }
  catch (const Glib::Error& e)
  {
    Gtk::MessageDialog oDialog(e.what(),
                               false,
                               Gtk::MESSAGE_ERROR,
                               Gtk::BUTTONS_OK);
    oDialog.run();
    return 1;
  }

  if (bShowVersion)
  {
    g_print(_("VisualBoyAdvance version %s [GTK+]\n"), VERSION);
    exit(0);
  }

  Gtk::Window::set_default_icon_name("vbam");

  Glib::RefPtr<Xml> poXml;
  try
  {
    poXml = Xml::create(PKGDATADIR "/vba.glade", "MainWindow");
  }
  catch (const Xml::Error & e)
  {
    Gtk::MessageDialog oDialog(e.what(),
                               false,
                               Gtk::MESSAGE_ERROR,
                               Gtk::BUTTONS_OK);
    oDialog.run();
    return 1;
  }

  VBA::Window * poWindow = NULL;
  poXml->get_widget_derived<VBA::Window>("MainWindow", poWindow);

  if (listRemaining.size() == 1)
  {
    // Display the window before loading the file
    poWindow->show();
    while (Gtk::Main::events_pending())
    {
      Gtk::Main::iteration();
    }

    poWindow->bLoadROM(listRemaining[0]);
  }

  Gtk::Main::run(*poWindow);
  delete poWindow;

  return 0;
}
