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

struct wl_surface;

#ifdef HAVE_WAYLAND_SUPPORT
bool IsWayland();
// True only when this is a KDE/Plasma (KWin) Wayland session whose display HDR
// mode is currently OFF (read from kwinoutputconfig.json). Used to treat HDR as
// unavailable -- and suppress the "no effect" warning -- when the user has
// turned HDR off in KDE, rather than offering an HDR toggle that does nothing.
bool KdeHdrDisabled();
// The outputs (monitors) the compositor shows a surface of ours on, from its
// wl_surface enter/leave events: a Wayland client is never told where its window
// is, only which outputs it is on. Track a surface we created (a renderer's
// subsurface) right after creating it, untrack it before destroying it.
void WaylandTrackSurfaceOutputs(struct wl_surface* surface);
void WaylandUntrackSurfaceOutputs(struct wl_surface* surface);
// Called on the GUI thread whenever a tracked surface enters or leaves an output.
void WaylandSetOutputsChangedHandler(void (*handler)());
#else
constexpr bool IsWayland() { return false; }
constexpr bool KdeHdrDisabled() { return false; }
inline void WaylandTrackSurfaceOutputs([[maybe_unused]] struct wl_surface* surface) {}
inline void WaylandUntrackSurfaceOutputs([[maybe_unused]] struct wl_surface* surface) {}
inline void WaylandSetOutputsChangedHandler([[maybe_unused]] void (*handler)()) {}
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

#include <vector>

// One output the window is on, as the compositor describes it.
struct WaylandOutputColor {
    bool hdr = false;        // its transfer function is ST2084 PQ: in HDR mode
    uint32_t peak_nits = 0;  // 0 when not reported
    float min_nits = 0.0f;
};

#ifdef HAVE_WAYLAND_CM
// Compositor supports parametric BT.2020 + ST2084 PQ image descriptions. Usable
// by any Wayland buffer path (EGL or shm); independent of EGL.
bool WaylandHdrPqSupported();
// Tag an arbitrary wl_surface as BT.2020 PQ (used by the shm "Simple" renderer's
// subsurface). ref/peak are luminance hints in nits (<= 0 skips the hint).
bool WaylandSetWlSurfaceHdrPq(struct wl_surface* surface, float ref_white_nits, float peak_nits);
void WaylandClearWlSurfaceColor(struct wl_surface* surface);
// Every output the window is on -- those the tracked surfaces are shown on, else
// the monitor GDK puts `gdk_window` (a GdkWindow*) on -- with whether each is in
// HDR mode and its luminance range, from the color-management output image
// description. False when it cannot tell (no color manager, no output known).
bool WaylandWindowOutputs(void* gdk_window, std::vector<WaylandOutputColor>* out);
// The compositor's reported peak luminance (nits) for the displays the window is
// on, the lowest where it spans several: content above it clips on one of them.
// 0 when unknown / HDR off. Absolute nits (Wayland HDR is PQ), so used directly
// as the peak ceiling.
uint32_t WaylandDisplayPeakNits(void* gdk_window);
// The compositor's reported minimum luminance (nits) for those same displays,
// the highest where it spans several: the display's own black floor including
// ambient flare. Fractional -- hundredths of a nit on an LCD, ten-thousandths on
// an OLED -- so unlike the peak this is a float. 0 when unknown or genuinely
// zero.
float WaylandDisplayMinNits(void* gdk_window);
#else
constexpr bool WaylandHdrPqSupported() { return false; }
inline bool WaylandSetWlSurfaceHdrPq([[maybe_unused]] struct wl_surface* surface,
                                     [[maybe_unused]] float ref_white_nits,
                                     [[maybe_unused]] float peak_nits) { return false; }
inline void WaylandClearWlSurfaceColor([[maybe_unused]] struct wl_surface* surface) {}
inline bool WaylandWindowOutputs([[maybe_unused]] void* gdk_window,
                                 [[maybe_unused]] std::vector<WaylandOutputColor>* out) { return false; }
constexpr uint32_t WaylandDisplayPeakNits([[maybe_unused]] void* gdk_window) { return 0; }
constexpr float WaylandDisplayMinNits([[maybe_unused]] void* gdk_window) { return 0.0f; }
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
