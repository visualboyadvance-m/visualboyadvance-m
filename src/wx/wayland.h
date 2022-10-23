#ifndef VBAM_WAYLAND_H
#define VBAM_WAYLAND_H

#include <wx/config.h>

class wxGLCanvas; // Forward declare.

#if defined(__WXGTK__)
#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND

#define HAVE_WAYLAND_SUPPORT

bool IsWayland();

#else

constexpr bool IsWayland() { return false; }

#endif // wayland

// Temporary hack to backport 800d6ed69b from wxWidgets until 3.2.2 is released.
#if defined(__WXGTK__) && defined(HAVE_EGL) && wxCHECK_VERSION(3, 2, 0) && !wxCHECK_VERSION(3, 2, 2)

#define WAYLAND_MOVE_SUBSURFACE_BACKPORT

void MoveWaylandSubsurface(wxGLCanvas* win);

#else

inline void MoveWaylandSubsurface([[maybe_unused]] wxGLCanvas* win) {};

#endif

#else // gtk

constexpr bool IsWayland() { return false; }

inline void MoveWaylandSubsurface([[maybe_unused]] wxGLCanvas* win) {};

#endif // gtk

#endif // VBAM_WAYLAND_H
