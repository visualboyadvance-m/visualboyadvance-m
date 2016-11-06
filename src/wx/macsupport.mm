#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

#include "wxvbam.h"

double HiDPIAware::HiDPIScaleFactor()
{
    if (hidpi_scale_factor == 0) {
        NSWindow* window = [(NSView*)GetWindow()->GetHandle() window];

        if ([window respondsToSelector:@selector(backingScaleFactor)]) {
            hidpi_scale_factor = [window backingScaleFactor];
        }
        else {
            hidpi_scale_factor = 1.0;
        }
    }

    return hidpi_scale_factor;
}

void HiDPIAware::RequestHighResolutionOpenGLSurface()
{
    NSView* view = (NSView*)(GetWindow()->GetHandle());

    if ([view respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)]) {
        [view setWantsBestResolutionOpenGLSurface:YES];
    }
}
