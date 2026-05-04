#ifndef VBAM_WX_WAYLAND_H_
#define VBAM_WX_WAYLAND_H_

#include <wx/config.h>

class wxGLCanvas;	// Forward declare.
class wxGLCanvasEGL;

// ----------------------------------------------------------------------------
// Wayland support detection.
//
// HAVE_WAYLAND_SUPPORT is defined if Wayland support is compiled into this
// build. Both this header and wayland.cpp derive every other decision from
// this one macro, which is the only way to guarantee the declarations here
// can never drift out of sync with the definitions in the .cpp file.
//
// Inputs (do NOT change individually -- read the chain below):
//
//   __WXGTK__              defined by wxWidgets if wx is built for GTK.
//                          Without it, no GTK/Wayland code is meaningful.
//
//   GDK_WINDOWING_WAYLAND  defined by <gdk/gdk.h> iff the installed GDK
//                          was compiled with the Wayland backend. May be
//                          undefined on minimal GDK builds, on non-Linux,
//                          or on very old GDKs without Wayland support --
//                          the constexpr stubs at the bottom cover that.
//
//   NO_WAYLAND             build option that forces Wayland off even when
//                          GDK supports it. Set in cmake/Options.cmake; can
//                          also be passed by the user via -DNO_WAYLAND=ON.
//
// Derivation:
//
//   HAVE_WAYLAND_SUPPORT := __WXGTK__ AND GDK_WINDOWING_WAYLAND AND !NO_WAYLAND
//   HAVE_WAYLAND_EGL     := HAVE_WAYLAND_SUPPORT AND HAVE_EGL
//                           AND wx >= 3.2.0 AND wxUSE_GLCANVAS_EGL
//   WAYLAND_MOVE_SUBSURFACE_BACKPORT
//                        := HAVE_WAYLAND_EGL AND wx < 3.2.2
// ----------------------------------------------------------------------------

#if defined(__WXGTK__) && !defined(NO_WAYLAND)
#  include <gdk/gdk.h>
#  ifdef GDK_WINDOWING_WAYLAND
#    define HAVE_WAYLAND_SUPPORT 1
#  endif
#endif

#if defined(HAVE_WAYLAND_SUPPORT) && defined(HAVE_EGL) && \
    wxCHECK_VERSION(3, 2, 0) && wxUSE_GLCANVAS_EGL
#  define HAVE_WAYLAND_EGL 1
#endif

// Temporary hack to backport wxWidgets commit 800d6ed69b until 3.2.2 is
// released and widely deployed. Older wx needs us to reposition the
// wl_subsurface manually; newer wx does it itself.
#if defined(HAVE_WAYLAND_EGL) && !wxCHECK_VERSION(3, 2, 2)
#  define WAYLAND_MOVE_SUBSURFACE_BACKPORT 1
#endif

// ----------------------------------------------------------------------------
// Public interface.
//
// Both functions are ALWAYS callable. When the corresponding feature isn't
// compiled in they collapse to compile-time no-ops, so callers can write
// straight-line code without ever touching #ifndef NO_WAYLAND or
// #ifdef HAVE_WAYLAND_SUPPORT around the call site:
//
//     if (IsWayland()) { ...wayland-specific path... }   // always compiles
//
// The optimizer will dead-strip the wayland branch when IsWayland is the
// constexpr stub, so there is zero runtime cost on builds without support.
// ----------------------------------------------------------------------------

#ifdef HAVE_WAYLAND_SUPPORT
bool IsWayland();
#else
constexpr bool IsWayland() { return false; }
#endif

#ifdef WAYLAND_MOVE_SUBSURFACE_BACKPORT
void MoveWaylandSubsurface(wxGLCanvasEGL* win);
#else
inline void MoveWaylandSubsurface([[maybe_unused]] wxGLCanvas* win) {}
#endif

#endif // VBAM_WX_WAYLAND_H_
