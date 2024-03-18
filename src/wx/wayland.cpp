#include "wx/wayland.h"

#ifdef HAVE_WAYLAND_SUPPORT

#include <gdk/gdkwayland.h>

bool IsWayland() { return GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default()); }

#endif

// Temporary hack to backport 800d6ed69b from wxWidgets until 3.2.2 is released.
#ifdef WAYLAND_MOVE_SUBSURFACE_BACKPORT
#include <wayland-egl.h>
#include <wx/glcanvas.h>
#include <wx/utils.h>

wl_subsurface* wxGLCanvasEGL::* private_ptr;

template <auto Ptr>
struct accessor {
    static int force_init;
};

template <auto Ptr>
int accessor<Ptr>::force_init = ((private_ptr = Ptr), 0);

template struct accessor<&wxGLCanvasEGL::m_wlSubsurface>;

void MoveWaylandSubsurface(wxGLCanvasEGL* win)
{
    if (!IsWayland()) return;

    // Check for 3.2.2 at runtime as well, because an app may have been
    // compiled against 3.2.0 or 3.2.1 but running with 3.2.2 after a distro
    // upgrade.
    auto&& wxver = wxGetLibraryVersionInfo();
    int &&major = wxver.GetMajor(), &&minor = wxver.GetMinor(), &&micro = wxver.GetMicro();

    if (!(major > 3 || (major == 3 && (minor > 2 || (minor == 2 && micro >= 2))))) {
        int x, y;
        gdk_window_get_origin(win->GTKGetDrawingWindow(), &x, &y);
        wl_subsurface_set_position(win->*private_ptr, x, y);
    }
}
#endif
