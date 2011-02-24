#!/bin/sh

intltool-extract --local --type=gettext/glade ../src/gtk/ui/cheatedit.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/cheatlist.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/display.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/gameboyadvance.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/gameboy.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/preferences.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/sound.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/vbam.ui

xgettext -k_ -kN_ -o gvbam.pot ../src/gtk/*.cpp tmp/*.h
