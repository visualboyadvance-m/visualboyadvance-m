#ifndef VBAM_WX_WIDGETS_DPI_SUPPORT_
#define VBAM_WX_WIDGETS_DPI_SUPPORT_

#include <wx/version.h>

#if wxCHECK_VERSION(3, 1, 4)
#define WX_HAS_NATIVE_HI_DPI_SUPPORT 1
#else
#define WX_HAS_NATIVE_HI_DPI_SUPPORT 0
#endif  // wxCHECK_VERSION(3,1,4)

// Forward declaration.
class wxWindow;

namespace widgets {

double DPIScaleFactorForWindow(wxWindow* window);
void RequestHighResolutionOpenGlSurfaceForWindow(wxWindow* window);
void GetRealPixelClientSize(wxWindow* window, int* x, int* y);

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_DPI_SUPPORT_
