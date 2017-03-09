#!/bin/sh
# Generate translation template file for the wxWidgets port
wxrc -g ../src/wx/xrc/*.xrc -o wx-xrc-strings.h

xgettext -k_ -kN_ -o wxvbam/wxvbam.pot ../src/wx/*.cpp ../src/wx/*.h ../src/wx/widgets/*.cpp wx-xrc-strings.h

rm -r wx-xrc-strings.h
