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
#include "../getopt.h"

#include <list>

#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/messagedialog.h>
#include <libglademm.h>

#include "images/vba-wm-pixbufs.h"

#include "window.h"
#include "intl.h"

using Gnome::Glade::Xml;

static const char * csProgramName;

static int iShowHelp;
static int iShowVersion;

// Non-characters used for long options that have no equivalent short option
enum
{
  IGNORED_OPTION = CHAR_MAX + 1
};

static const char csShortOptions[] = "V";

static const struct option astLongOptions[] =
{
  { "help",    no_argument, &iShowHelp, IGNORED_OPTION },
  { "version", no_argument, NULL,       'V'            },
  { 0, 0, 0, 0 }
};

static void vUsage(int iStatus)
{
  if (iStatus != 0)
  {
    g_printerr(_("Try `%s --help' for more information.\n"), csProgramName);
  }
  else
  {
    g_print(_("Usage: %s [option ...] [file]\n"), csProgramName);
    g_print(_("\
\n\
Options:\n\
      --help            Output this help.\n\
  -V, --version         Output version information.\n\
"));
  }

  exit(iStatus);
}

static void vSetDefaultWindowIcon()
{
  const guint8 * apuiInlinePixbuf[] =
  {
    stock_vba_wm_16,
    stock_vba_wm_32,
    stock_vba_wm_48,
    stock_vba_wm_64
  };

  std::list<Glib::RefPtr<Gdk::Pixbuf> > listPixbuf;
  for (guint i = 0; i < G_N_ELEMENTS(apuiInlinePixbuf); i++)
  {
    listPixbuf.push_back(
      Gdk::Pixbuf::create_from_inline(-1, apuiInlinePixbuf[i]));
  }

  Gtk::Window::set_default_icon_list(listPixbuf);
}

int main(int argc, char * argv[])
{
  csProgramName = argv[0];

#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  textdomain(GETTEXT_PACKAGE);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif // ENABLE_NLS

  Gtk::Main oKit(argc, argv);

  int iOpt;
  while ((iOpt = getopt_long(argc, argv, csShortOptions, astLongOptions, NULL))
         != -1)
  {
    switch (iOpt)
    {
    case 'V':
      iShowVersion = 1;
      break;
    case 0:
      // Long options
      break;
    default:
      vUsage(1);
      break;
    }
  }

  if (iShowVersion)
  {
    g_print(_("VisualBoyAdvance version %s [GTK+]\n"), VERSION);
    exit(0);
  }

  if (iShowHelp)
  {
    vUsage(0);
  }

  vSetDefaultWindowIcon();

  Glib::RefPtr<Xml> poXml;
  try
  {
    poXml = Xml::create(PKGDATADIR "/vba.glade", "MainWindow");
  }
  catch (const Xml::Error & e)
  {
    Gtk::MessageDialog oDialog(e.what(),
#ifndef GTKMM20
                               false,
#endif // ! GTKMM20
                               Gtk::MESSAGE_ERROR,
                               Gtk::BUTTONS_OK);
    oDialog.run();
    return 1;
  }

  VBA::Window * poWindow = NULL;
  poXml->get_widget_derived<VBA::Window>("MainWindow", poWindow);

  if (optind < argc)
  {
    // Display the window before loading the file
    poWindow->show();
    while (Gtk::Main::events_pending())
    {
      Gtk::Main::iteration();
    }

    poWindow->bLoadROM(argv[optind]);
  }

  Gtk::Main::run(*poWindow);
  delete poWindow;

  return 0;
}
