// Work around a stray non-Apple <uuid/uuid.h> (e.g. a libuuid install in
// /usr/local/include, which Apple clang searches before the SDK) shadowing
// the SDK header: pre-declare the type the SDK's hfs_format.h needs. The
// real SDK uuid.h honors the _UUID_STRING_T guard.
#include <sys/types.h>
#ifndef _UUID_STRING_T
#define _UUID_STRING_T
typedef __darwin_uuid_string_t uuid_string_t;
#endif

#include "wx/widgets/dpi-support.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include <wx/window.h>

namespace widgets {

double DPIScaleFactorForWindow(wxWindow* window) {
#if WX_HAS_NATIVE_HI_DPI_SUPPORT
    return window->GetDPIScaleFactor();
#else
    NSWindow* ns_window = [(NSView*)window->GetHandle() window];

    if ([ns_window respondsToSelector:@selector(backingScaleFactor)]) {
        return [ns_window backingScaleFactor];
    }
    return 1.0;
#endif  // WX_HAS_NATIVE_HI_DPI_SUPPORT
}

void RequestHighResolutionOpenGlSurfaceForWindow([[maybe_unused]] wxWindow* window) {
#if !WX_HAS_NATIVE_HI_DPI_SUPPORT
    NSView* view = (NSView*)(window->GetHandle());

    if ([view respondsToSelector:@selector
              (setWantsBestResolutionOpenGLSurface:)]) {
        [view setWantsBestResolutionOpenGLSurface:YES];
    }
#endif  // !WX_HAS_NATIVE_HI_DPI_SUPPORT
}

void GetRealPixelClientSize(wxWindow* window, int* x, int* y) {
#if WX_HAS_NATIVE_HI_DPI_SUPPORT
    window->GetClientSize(x, y);
#else
    NSView* view = (NSView*)(window->GetHandle());

    NSSize backing_size = [view convertSizeToBacking:view.bounds.size];

    *x = backing_size.width;
    *y = backing_size.height;
#endif  // WX_HAS_NATIVE_HI_DPI_SUPPORT
}

}  // namespace widgets
