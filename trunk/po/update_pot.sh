#!/bin/sh

# Generate translation template file for the GTK+ port
intltool-extract --local --type=gettext/glade ../src/gtk/ui/cheatedit.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/cheatlist.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/display.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/gameboyadvance.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/gameboy.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/preferences.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/sound.ui
intltool-extract --local --type=gettext/glade ../src/gtk/ui/vbam.ui

xgettext -k_ -kN_ -o gvbam/gvbam.pot ../src/gtk/*.cpp tmp/*.h

rm -r tmp/

# Generate translation template file for the wxWidgets port
wxrc -g ../src/wx/xrc/*.xrc -o wx-xrc-strings.h

xgettext -k_ -kN_ -o wxvbam/wxvbam.pot ../src/wx/*.cpp ../src/wx/*.h ../src/wx/widgets/*.cpp wx-xrc-strings.h

rm -r wx-xrc-strings.h
