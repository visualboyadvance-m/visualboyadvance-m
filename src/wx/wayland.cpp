#include "wayland.h"

#ifdef HAVE_WAYLAND_SUPPORT

#include <gdk/gdkwayland.h>

bool IsWayland() { return GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default()); }

#endif

// Temporary hack to backport 800d6ed69b from wxWidgets until 3.2.2 is released.
#ifdef WAYLAND_MOVE_SUBSURFACE_BACKPORT
#include <wayland-egl.h>
#include <wx/glcanvas.h>

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

    int x, y;
    gdk_window_get_origin(win->GTKGetDrawingWindow(), &x, &y);
    wl_subsurface_set_position(win->*private_ptr, x, y);
}
#endif
