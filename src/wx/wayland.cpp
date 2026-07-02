#include "wx/wayland.h"

#ifdef HAVE_WAYLAND_SUPPORT
#include <gdk/gdkwayland.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

bool IsWayland() {
    return GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default());
}

bool KdeHdrDisabled() {
    // Only meaningful for a KDE/Plasma (KWin) Wayland session.
    if (!IsWayland())
        return false;
    const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
    const bool kde = (desktop && std::strstr(desktop, "KDE")) ||
                     std::getenv("KDE_FULL_SESSION");
    if (!kde)
        return false;

    // KWin records each output's HDR toggle in kwinoutputconfig.json as
    // "highDynamicRange": true/false. WaylandHdrPqSupported() only tells us the
    // compositor *can* do PQ (always true on KWin); this tells us whether the
    // display is actually in HDR mode right now. Reading the saved config is
    // simpler and more reliable than the async color-management surface feedback.
    std::string path;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        path = std::string(xdg) + "/kwinoutputconfig.json";
    else if (const char* home = std::getenv("HOME"))
        path = std::string(home) + "/.config/kwinoutputconfig.json";
    else
        return false;

    std::ifstream f(path);
    if (!f)
        return false;  // can't tell -> don't suppress HDR
    const std::string s((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    // HDR is "off" only if the key is present and every occurrence is false. If
    // any output has it true (or the key is absent / unreadable), don't suppress.
    const std::string key = "\"highDynamicRange\"";
    bool any_key = false, any_on = false;
    for (size_t pos = 0; (pos = s.find(key, pos)) != std::string::npos;) {
        any_key = true;
        size_t colon = s.find(':', pos + key.size());
        if (colon == std::string::npos)
            break;
        size_t v = s.find_first_not_of(" \t\r\n", colon + 1);
        if (v != std::string::npos && s.compare(v, 4, "true") == 0)
            any_on = true;
        pos = colon + 1;
    }
    return any_key && !any_on;
}
#endif // HAVE_WAYLAND_SUPPORT

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
#endif // WAYLAND_MOVE_SUBSURFACE_BACKPORT

#ifdef HAVE_WAYLAND_CM
#include <algorithm>
#include <cstring>
#include <map>
#include <unistd.h>

#include <wayland-client.h>
#include <gdk/gdkwayland.h>
#ifdef HAVE_WAYLAND_EGL
#include <wx/glcanvas.h>
#endif

#include "color-management-v1-client-protocol.h"

// HDR (BT.2020 PQ) presentation for the OpenGL/EGL renderer via the compositor
// color-management protocol. Mesa does not expose
// EGL_EXT_gl_colorspace_bt2020_pq, so instead of asking EGL for a PQ surface we
// render PQ-encoded 10-bit content into the ordinary EGL surface and tag that
// surface's wl_surface as BT.2020 PQ through wp_color_manager_v1. The compositor
// (e.g. KWin) then treats the buffer as HDR.

namespace {

// One shared color manager for the process, bound on its own event queue so our
// roundtrips never dispatch (and reenter) GTK's default-queue handlers.
struct ColorManager {
    wl_display*          display   = nullptr;
    wl_event_queue*      queue     = nullptr;
    wp_color_manager_v1* manager   = nullptr;
    bool probed           = false;  // enumeration roundtrip completed
    bool feat_parametric  = false;
    bool feat_mastering   = false;  // set_mastering_display_primaries / luminance
    bool tf_pq            = false;
    bool prim_bt2020      = false;
};

ColorManager& Cm() { static ColorManager cm; return cm; }

void CmIntent (void*, wp_color_manager_v1*, uint32_t) {}
void CmFeature(void* d, wp_color_manager_v1*, uint32_t feature) {
    auto* cm = static_cast<ColorManager*>(d);
    if (feature == WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC)
        cm->feat_parametric = true;
    else if (feature == WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES)
        cm->feat_mastering = true;
}
void CmTfNamed(void* d, wp_color_manager_v1*, uint32_t tf) {
    if (tf == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ)
        static_cast<ColorManager*>(d)->tf_pq = true;
}
void CmPrimariesNamed(void* d, wp_color_manager_v1*, uint32_t primaries) {
    if (primaries == WP_COLOR_MANAGER_V1_PRIMARIES_BT2020)
        static_cast<ColorManager*>(d)->prim_bt2020 = true;
}
void CmDone(void*, wp_color_manager_v1*) {}

const wp_color_manager_v1_listener kCmListener = {
    CmIntent, CmFeature, CmTfNamed, CmPrimariesNamed, CmDone,
};

void RegGlobal(void* d, wl_registry* reg, uint32_t name,
               const char* iface, uint32_t version) {
    if (std::strcmp(iface, wp_color_manager_v1_interface.name) == 0) {
        auto* cm = static_cast<ColorManager*>(d);
        const uint32_t bind_ver = version < 1u ? version : 1u;
        cm->manager = static_cast<wp_color_manager_v1*>(
            wl_registry_bind(reg, name, &wp_color_manager_v1_interface, bind_ver));
    }
}
void RegGlobalRemove(void*, wl_registry*, uint32_t) {}
const wl_registry_listener kRegListener = { RegGlobal, RegGlobalRemove };

// Bind the manager and enumerate its capabilities exactly once.
bool EnsureColorManager() {
    ColorManager& cm = Cm();
    if (cm.probed)
        return cm.manager != nullptr;
    cm.probed = true;

    GdkDisplay* gd = gdk_display_get_default();
    if (!gd || !GDK_IS_WAYLAND_DISPLAY(gd))
        return false;
    cm.display = gdk_wayland_display_get_wl_display(gd);
    if (!cm.display)
        return false;

    cm.queue = wl_display_create_queue(cm.display);
    wl_registry* reg = wl_display_get_registry(cm.display);
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(reg), cm.queue);
    wl_registry_add_listener(reg, &kRegListener, &cm);
    wl_display_roundtrip_queue(cm.display, cm.queue);  // -> RegGlobal

    if (cm.manager) {
        wp_color_manager_v1_add_listener(cm.manager, &kCmListener, &cm);
        wl_display_roundtrip_queue(cm.display, cm.queue);  // -> supported_*/done
    }
    wl_registry_destroy(reg);
    return cm.manager != nullptr;
}

// Per-surface color-management object, keyed by the wl_surface it tags.
// get_surface raises a protocol error if called twice for one wl_surface, so
// this cache guarantees one per surface.
std::map<wl_surface*, wp_color_management_surface_v1*>& SurfaceMap() {
    static std::map<wl_surface*, wp_color_management_surface_v1*> m;
    return m;
}

#ifdef HAVE_WAYLAND_EGL
// Reach the GL canvas's private wl_surface using the same legal explicit
// template-instantiation trick the subsurface backport above relies on.
wl_surface* wxGLCanvasEGL::* g_wlSurfacePtr = nullptr;
template <auto Ptr> struct SurfaceAccessor { static int force_init; };
template <auto Ptr> int SurfaceAccessor<Ptr>::force_init = ((g_wlSurfacePtr = Ptr), 0);
template struct SurfaceAccessor<&wxGLCanvasEGL::m_wlSurface>;
#endif

// Wait for an image description to become ready (or fail).
struct DescState { bool done = false; bool ready = false; };
void DescFailed(void* d, wp_image_description_v1*, uint32_t, const char*) {
    static_cast<DescState*>(d)->done = true;
}
void DescReady(void* d, wp_image_description_v1*, uint32_t) {
    auto* s = static_cast<DescState*>(d); s->done = true; s->ready = true;
}
// ready2 (protocol v2) is intentionally left null: we bind wp_color_manager_v1
// at version 1, so only the v1 'ready' event is ever delivered. Zero-initialize
// the whole listener and set just the fields we use, which stays warning-free
// (no -Wmissing-field-initializers) and correct on older wayland-protocols
// without ready2, no matter how many events the listener grows.
const wp_image_description_v1_listener kDescListener = [] {
    wp_image_description_v1_listener l = {};
    l.failed = DescFailed;
    l.ready  = DescReady;
    return l;
}();

// Collect an image description's luminance info to read the display peak. The
// protocol carries max luminances in whole cd/m² (nits); min/reference are
// ignored here. target_luminance is the mastering/display target; luminances is
// the description's own range -- prefer the target, fall back to the range.
struct InfoState {
    uint32_t lum_max        = 0;  // luminances.max_lum
    uint32_t target_lum_max = 0;  // target_luminance.max_lum
    bool     done           = false;
};
void InfoDone(void* d, wp_image_description_info_v1*) {
    static_cast<InfoState*>(d)->done = true;
}
void InfoLuminances(void* d, wp_image_description_info_v1*,
                    uint32_t /*min*/, uint32_t max_lum, uint32_t /*ref*/) {
    static_cast<InfoState*>(d)->lum_max = max_lum;
}
void InfoTargetLuminance(void* d, wp_image_description_info_v1*,
                         uint32_t /*min*/, uint32_t max_lum) {
    static_cast<InfoState*>(d)->target_lum_max = max_lum;
}

// The compositor emits one event per property it knows about, so describing an
// HDR output delivers primaries/transfer/target events too, not just the
// luminances we read. libwayland dispatches every event the compositor sends
// and calls wl_abort (or, on older versions, jumps through a null pointer) if
// the listener slot for a delivered event is NULL -- so every slot must point
// at a real function even when we ignore the payload. These are the no-op stubs
// for the events we don't consume.
void InfoIccFile(void*, wp_image_description_info_v1*, int32_t fd, uint32_t) {
    // The fd is handed to us and we own it; close it so each probe doesn't leak.
    if (fd >= 0)
        close(fd);
}
void InfoPrimaries(void*, wp_image_description_info_v1*,
                   int32_t, int32_t, int32_t, int32_t,
                   int32_t, int32_t, int32_t, int32_t) {}
void InfoPrimariesNamed(void*, wp_image_description_info_v1*, uint32_t) {}
void InfoTfPower(void*, wp_image_description_info_v1*, uint32_t) {}
void InfoTfNamed(void*, wp_image_description_info_v1*, uint32_t) {}
void InfoTargetPrimaries(void*, wp_image_description_info_v1*,
                         int32_t, int32_t, int32_t, int32_t,
                         int32_t, int32_t, int32_t, int32_t) {}
void InfoTargetMaxCll(void*, wp_image_description_info_v1*, uint32_t) {}
void InfoTargetMaxFall(void*, wp_image_description_info_v1*, uint32_t) {}

// Every event slot is populated (real handler or no-op stub); see above.
const wp_image_description_info_v1_listener kInfoListener = [] {
    wp_image_description_info_v1_listener l = {};
    l.done             = InfoDone;
    l.icc_file         = InfoIccFile;
    l.primaries        = InfoPrimaries;
    l.primaries_named  = InfoPrimariesNamed;
    l.tf_power         = InfoTfPower;
    l.tf_named         = InfoTfNamed;
    l.luminances       = InfoLuminances;
    l.target_primaries = InfoTargetPrimaries;
    l.target_luminance = InfoTargetLuminance;
    l.target_max_cll   = InfoTargetMaxCll;
    l.target_max_fall  = InfoTargetMaxFall;
    return l;
}();

}  // namespace

bool WaylandHdrPqSupported() {
    if (!IsWayland() || !EnsureColorManager())
        return false;
    const ColorManager& cm = Cm();
    return cm.feat_parametric && cm.tf_pq && cm.prim_bt2020;
}

bool WaylandSetWlSurfaceHdrPq(wl_surface* wls,
                              float ref_white_nits, float peak_nits) {
    if (!wls || !WaylandHdrPqSupported())
        return false;
    ColorManager& cm = Cm();

    // Build a parametric BT.2020 + ST2084 PQ image description. tf + primaries
    // is a complete parameter set, but a PQ description with no luminance
    // metadata leaves the compositor assuming the PQ default content range (up
    // to 10000 nits). KWin then tone-maps that phantom 0..10000 range down to
    // the display, squashing our real highlights -- which only reach peak_nits
    // -- back down near the reference white, so the highlight-knee boost is
    // invisible and white reads no brighter than SDR. Declare the actual
    // content luminance envelope so the compositor maps our peak to the display
    // peak instead. The named-PQ default reference white (203 nits, the SDR
    // anchor) is intentionally left untouched: our encoder bakes the chosen
    // reference white into the signal as absolute nits, and re-anchoring it
    // here would cancel that out.
    wp_image_description_creator_params_v1* params =
        wp_color_manager_v1_create_parametric_creator(cm.manager);
    wp_image_description_creator_params_v1_set_tf_named(
        params, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ);
    wp_image_description_creator_params_v1_set_primaries_named(
        params, WP_COLOR_MANAGER_V1_PRIMARIES_BT2020);

    // Round to whole nits; the protocol carries these as integer cd/m².
    const uint32_t peak_cd  = static_cast<uint32_t>(
        std::max(1.0f, peak_nits) + 0.5f);
    const uint32_t ref_cd   = static_cast<uint32_t>(
        std::max(1.0f, ref_white_nits) + 0.5f);
    // max_fall (frame-average) must not exceed max_cll (peak).
    const uint32_t fall_cd  = std::min(peak_cd, ref_cd);

    // Mastering luminance defines the target color volume the compositor tone-
    // maps toward. min_lum is cd/m² * 10000 (here 0); max_lum is unscaled. This
    // is the request that tells KWin "content tops out at peak_nits, not
    // 10000". Gated on the set_mastering_display_primaries feature.
    if (cm.feat_mastering) {
        wp_image_description_creator_params_v1_set_mastering_luminance(
            params, 0u, peak_cd);
    }
    // Static HDR (CTA-861) content light levels; ungated, undefined by default.
    // These also feed the compositor's tone-mapping ceiling.
    wp_image_description_creator_params_v1_set_max_cll(params, peak_cd);
    wp_image_description_creator_params_v1_set_max_fall(params, fall_cd);

    // create() consumes (destroys) the params object.
    wp_image_description_v1* desc =
        wp_image_description_creator_params_v1_create(params);

    DescState st;
    wp_image_description_v1_add_listener(desc, &kDescListener, &st);
    while (!st.done) {
        if (wl_display_roundtrip_queue(cm.display, cm.queue) < 0)
            break;
    }
    if (!st.ready) {
        wp_image_description_v1_destroy(desc);
        return false;
    }

    wp_color_management_surface_v1*& surf = SurfaceMap()[wls];
    if (!surf) {
        surf = wp_color_manager_v1_get_surface(cm.manager, wls);
        wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(surf), cm.queue);
    }
    // Pending surface state; applied on the next wl_surface commit (the
    // renderer's eglSwapBuffers / shm attach+commit). The compositor keeps its
    // own reference to the description, so we can drop ours immediately.
    wp_color_management_surface_v1_set_image_description(
        surf, desc, WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
    wp_image_description_v1_destroy(desc);
    wl_display_flush(cm.display);
    return true;
}

void WaylandClearWlSurfaceColor(wl_surface* wls) {
    auto& m = SurfaceMap();
    auto it = m.find(wls);
    if (it == m.end())
        return;
    if (it->second)
        wp_color_management_surface_v1_destroy(it->second);
    m.erase(it);
    if (Cm().display)
        wl_display_flush(Cm().display);
}

uint32_t WaylandDisplayPeakNits() {
    if (!IsWayland() || !EnsureColorManager())
        return 0;
    ColorManager& cm = Cm();

    // The wl_output for the display (primary, else the first monitor). GDK owns
    // it; we only pass it as an argument, so its event queue is irrelevant.
    GdkDisplay* gd = gdk_display_get_default();
    if (!gd)
        return 0;
    GdkMonitor* mon = gdk_display_get_primary_monitor(gd);
    if (!mon)
        mon = gdk_display_get_monitor(gd, 0);
    if (!mon || !GDK_IS_WAYLAND_MONITOR(mon))
        return 0;
    // gdk_wayland_monitor_get_wl_output() takes a GdkMonitor* (the
    // GDK_IS_WAYLAND_MONITOR() check above already confirms the backend);
    // wrapping it in GDK_WAYLAND_MONITOR() yields an incomplete type that
    // does not convert back to GdkMonitor* under C++.
    wl_output* out = gdk_wayland_monitor_get_wl_output(mon);
    if (!out)
        return 0;

    // Ask the compositor for the output's image description, then its luminance
    // info. All proxies run on our private queue so roundtrips don't reenter
    // GTK's default-queue dispatch.
    wp_color_management_output_v1* cmout =
        wp_color_manager_v1_get_output(cm.manager, out);
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(cmout), cm.queue);
    wp_image_description_v1* desc =
        wp_color_management_output_v1_get_image_description(cmout);
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(desc), cm.queue);

    uint32_t peak = 0;
    DescState ds;
    wp_image_description_v1_add_listener(desc, &kDescListener, &ds);
    while (!ds.done) {
        if (wl_display_roundtrip_queue(cm.display, cm.queue) < 0)
            break;
    }
    if (ds.ready) {
        wp_image_description_info_v1* info =
            wp_image_description_v1_get_information(desc);
        wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(info), cm.queue);
        InfoState is;
        wp_image_description_info_v1_add_listener(info, &kInfoListener, &is);
        while (!is.done) {
            if (wl_display_roundtrip_queue(cm.display, cm.queue) < 0)
                break;
        }
        peak = is.target_lum_max ? is.target_lum_max : is.lum_max;
        wp_image_description_info_v1_destroy(info);
    }

    wp_image_description_v1_destroy(desc);
    wp_color_management_output_v1_destroy(cmout);
    return peak;
}

#ifdef HAVE_WAYLAND_EGL
// Thin shims tagging the GL canvas's own EGL wl_surface.
bool WaylandSetSurfaceHdrPq(wxGLCanvasEGL* canvas,
                            float ref_white_nits, float peak_nits) {
    if (!canvas)
        return false;
    return WaylandSetWlSurfaceHdrPq(canvas->*g_wlSurfacePtr, ref_white_nits, peak_nits);
}

void WaylandClearSurfaceColor(wxGLCanvasEGL* canvas) {
    if (canvas)
        WaylandClearWlSurfaceColor(canvas->*g_wlSurfacePtr);
}
#endif  // HAVE_WAYLAND_EGL
#endif // HAVE_WAYLAND_CM
