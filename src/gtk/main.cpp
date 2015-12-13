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
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/messagedialog.h>
#include <glibmm/miscutils.h>

#if defined(USE_OPENGL) && !GTK_CHECK_VERSION(3, 0, 0)
#include <gtkmm/gl/init.h>
#endif // USE_OPENGL

// this will be ifdefed soon
#include <X11/Xlib.h>

#include "window.h"
#include "intl.h"

int systemDebug = 0;

int main(int argc, char * argv[])
{
  bool bShowVersion = false;
  Glib::OptionGroup::vecustrings listRemaining;

#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain("gvbam", LOCALEDIR);
  textdomain("gvbam");
#endif // ENABLE_NLS
  //will be ifdefed
  XInitThreads();

  Glib::set_application_name(_("VBA-M"));

  Gtk::Main oKit(argc, argv);

#if defined(USE_OPENGL) && !GTK_CHECK_VERSION(3, 0, 0)
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

  std::string sGtkBuilderFile = VBA::Window::sGetUiFilePath("vbam.ui");

  Glib::RefPtr<Gtk::Builder> poXml;
  try
  {
    poXml = Gtk::Builder::create();
    poXml->add_from_file(sGtkBuilderFile, "accelgroup1");
    poXml->add_from_file(sGtkBuilderFile, "MainWindow");
  }
  catch (const Gtk::BuilderError & e)
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
