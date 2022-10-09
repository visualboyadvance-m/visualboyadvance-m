#include "widgets/dpi-support.h"

#include <Windows.h>
#include <VersionHelpers.h>
#include <shellscalingapi.h>

#include <wx/window.h>

namespace widgets {

double DPIScaleFactorForWindow(wxWindow* window) {
#if WX_HAS_NATIVE_HI_DPI_SUPPORT
    return window->GetDPIScaleFactor();
#else
    static constexpr double kStandardDpi = 96.0;
    HWND wnd = window->GetHWND();
    if (IsWindows10OrGreater()) {
        // We can't properly resize on DPI/Monitor change, but neither can
        // wxWidgets so we're consistent.
        UINT dpi = GetDpiForWindow(wnd);
        if (dpi != 0) {
            return static_cast<int>(dpi) / kStandardDpi;
        }
    }

    if (IsWindows8Point1OrGreater()) {
        HMONITOR monitor = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
        UINT xdpi;
        UINT ydpi;
        HRESULT success =
            GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
        if (success == S_OK) {
            return static_cast<int>(ydpi) / kStandardDpi;
        }
    }

    HDC dc = GetDC(wnd);
    if (dc == nullptr) {
        return 1.0;
    }

    int ydpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);

    return ydpi / kStandardDpi;
#endif  // WX_HAS_NATIVE_HI_DPI_SUPPORT
}

void RequestHighResolutionOpenGlSurfaceForWindow(wxWindow*) {}

void GetRealPixelClientSize(wxWindow* window, int* x, int* y) {
    window->GetClientSize(x, y);
}

}  // namespace widgets
