#ifdef __WXGTK__
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
        bool IsItWayland() { return GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default()); }
#else
        bool IsItWayland() { return false; }
#endif
#else
    bool IsItWayland() { return false; }
#endif
