#include "wx/widgets/dpi-support.h"

#include <cmath>

#include <wx/window.h>

namespace widgets {

double DPIScaleFactorForWindow(wxWindow* window) {
#if WX_HAS_NATIVE_HI_DPI_SUPPORT
    return window->GetDPIScaleFactor();
#else
    return window->GetContentScaleFactor();
#endif  // WX_HAS_NATIVE_HI_DPI_SUPPORT
}

void RequestHighResolutionOpenGlSurfaceForWindow(wxWindow*) {}

void GetRealPixelClientSize(wxWindow* window, int* x, int* y) {
    window->GetClientSize(x, y);
#ifdef __WXGTK__
    // wxGLCanvasEGL sizes its wl_egl_window backing buffer to
    // client_size * gdk_window_get_scale_factor() and calls
    // wl_surface_set_buffer_scale() to match, so the buffer is in physical
    // pixels while GetClientSize() returns logical DIPs. The GL viewport must
    // use physical pixels; otherwise on a scaled (scale > 1) display the image
    // is rendered into only the lower-left 1/scale^2 of the surface. On X11 the
    // scale factor is normally 1, so this is a no-op there.
    const double scale = DPIScaleFactorForWindow(window);
    if (x) *x = std::lround(*x * scale);
    if (y) *y = std::lround(*y * scale);
#endif  // __WXGTK__
}

}  // namespace widgets
