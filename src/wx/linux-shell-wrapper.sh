#!/bin/sh

# Force Adwaita-dark theme for GTK2 and 3+, other themes can cause serious
# issues:

# For GTK2:
if [ -e /usr/share/themes/Adwaita-dark/gtk-2.0/gtkrc ]; then
	GTK2_RC_FILES=/usr/share/themes/Adwaita-dark/gtk-2.0/gtkrc
fi

# For GTK3+:
GTK_THEME=Adwaita:dark

# Turn off vsync:

# for mesa drivers.
vblank_mode=0

# for Nvidia drivers.
__GL_SYNC_TO_VBLANK=0

export GTK2_RC_FILES GTK_THEME vblank_mode __GL_SYNC_TO_VBLANK

${0%/*}/visualboyadvance-m.real "$@"
