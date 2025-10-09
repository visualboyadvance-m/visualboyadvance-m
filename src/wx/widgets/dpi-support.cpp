#include "wx/widgets/dpi-support.h"

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
}

}  // namespace widgets
