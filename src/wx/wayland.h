#ifndef VBAM_WX_WAYLAND_H_
#define VBAM_WX_WAYLAND_H_

#include <cstdint>

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

//   HAVE_WAYLAND_HDR     := HAVE_WAYLAND_EGL AND HAVE_WAYLAND_COLOR_PROTOCOL
//                          HDR (BT.2020 PQ) for the OpenGL/EGL renderer is
//                          driven by tagging the EGL surface via the compositor
//                          color-management protocol (wp_color_manager_v1).
//                          HAVE_WAYLAND_COLOR_PROTOCOL is set by CMake when
//                          wayland-scanner and the staging color-management XML
//                          are available to generate the client glue.
#if defined(HAVE_WAYLAND_EGL) && defined(HAVE_WAYLAND_COLOR_PROTOCOL)
#  define HAVE_WAYLAND_HDR 1
#endif

//   HAVE_WAYLAND_CM      := HAVE_WAYLAND_SUPPORT AND HAVE_WAYLAND_COLOR_PROTOCOL
//                          The compositor color-management tagging (BT.2020 PQ)
//                          needs only the generated protocol glue, not EGL, so it
//                          is available to the software "Simple" shm renderer as
//                          well as the OpenGL/EGL one. HAVE_WAYLAND_HDR (EGL) is a
//                          strict subset.
#if defined(HAVE_WAYLAND_SUPPORT) && defined(HAVE_WAYLAND_COLOR_PROTOCOL)
#  define HAVE_WAYLAND_CM 1
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
// True only when this is a KDE/Plasma (KWin) Wayland session whose display HDR
// mode is currently OFF (read from kwinoutputconfig.json). Used to treat HDR as
// unavailable -- and suppress the "no effect" warning -- when the user has
// turned HDR off in KDE, rather than offering an HDR toggle that does nothing.
bool KdeHdrDisabled();
#else
constexpr bool IsWayland() { return false; }
constexpr bool KdeHdrDisabled() { return false; }
#endif

#ifdef WAYLAND_MOVE_SUBSURFACE_BACKPORT
void MoveWaylandSubsurface(wxGLCanvasEGL* win);
#else
inline void MoveWaylandSubsurface([[maybe_unused]] wxGLCanvas* win) {}
#endif

// ----------------------------------------------------------------------------
// HDR (BT.2020 PQ) presentation via wp_color_manager_v1.
//
// As with the helpers above these are always callable and collapse to
// compile-time no-ops (returning false) when HDR support isn't compiled in.
//
//   WaylandHdrPqSupported()       compositor advertises parametric BT.2020 +
//                                 ST2084 PQ image descriptions.
//   WaylandSetSurfaceHdrPq(...)   tag the canvas's EGL wl_surface as BT.2020 PQ,
//                                 with optional reference-white / peak luminance
//                                 hints in nits (<= 0 skips the hint).
//   WaylandClearSurfaceColor(...) drop any tag previously set on the surface.
// ----------------------------------------------------------------------------

struct wl_surface;

#ifdef HAVE_WAYLAND_CM
// Compositor supports parametric BT.2020 + ST2084 PQ image descriptions. Usable
// by any Wayland buffer path (EGL or shm); independent of EGL.
bool WaylandHdrPqSupported();
// Tag an arbitrary wl_surface as BT.2020 PQ (used by the shm "Simple" renderer's
// subsurface). ref/peak are luminance hints in nits (<= 0 skips the hint).
bool WaylandSetWlSurfaceHdrPq(struct wl_surface* surface, float ref_white_nits, float peak_nits);
void WaylandClearWlSurfaceColor(struct wl_surface* surface);
// The compositor's reported peak luminance (nits) for the display the app is on,
// via the color-management output image description. 0 when unknown / HDR off.
// Absolute nits (Wayland HDR is PQ), so used directly as the peak ceiling.
uint32_t WaylandDisplayPeakNits();
#else
constexpr bool WaylandHdrPqSupported() { return false; }
inline bool WaylandSetWlSurfaceHdrPq([[maybe_unused]] struct wl_surface* surface,
                                     [[maybe_unused]] float ref_white_nits,
                                     [[maybe_unused]] float peak_nits) { return false; }
inline void WaylandClearWlSurfaceColor([[maybe_unused]] struct wl_surface* surface) {}
constexpr uint32_t WaylandDisplayPeakNits() { return 0; }
#endif

#ifdef HAVE_WAYLAND_HDR
// Convenience wrappers tagging the GL canvas's own EGL wl_surface (needs the wx
// EGL canvas). Thin shims over the wl_surface versions above.
bool WaylandSetSurfaceHdrPq(wxGLCanvasEGL* canvas, float ref_white_nits, float peak_nits);
void WaylandClearSurfaceColor(wxGLCanvasEGL* canvas);
#else
inline bool WaylandSetSurfaceHdrPq([[maybe_unused]] wxGLCanvasEGL* canvas,
                                   [[maybe_unused]] float ref_white_nits,
                                   [[maybe_unused]] float peak_nits) { return false; }
inline void WaylandClearSurfaceColor([[maybe_unused]] wxGLCanvasEGL* canvas) {}
#endif

#endif // VBAM_WX_WAYLAND_H_
