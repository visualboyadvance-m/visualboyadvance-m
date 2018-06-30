#include <cmath>
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

#include <wx/rawbmp.h>

#include "wxvbam.h"
#include "drawing.h"

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

void HiDPIAware::GetRealPixelClientSize(int* x, int* y)
{
    NSView* view = (NSView*)(GetWindow()->GetHandle());

    NSSize backing_size = [view convertSizeToBacking:view.bounds.size];

    *x = backing_size.width;
    *y = backing_size.height;
}

Quartz2DDrawingPanel::Quartz2DDrawingPanel(wxWindow* parent, int _width, int _height)
    : BasicDrawingPanel(parent, _width, _height)
{
}

void Quartz2DDrawingPanel::DrawImage(wxWindowDC& dc, wxImage* im)
{
    NSView* view = (NSView*)(GetWindow()->GetHandle());

    size_t w    = std::ceil(width  * scale);
    size_t h    = std::ceil(height * scale);
    size_t size = w * h * 3;

    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, im->GetData(), size, NULL);

    CGColorSpaceRef color_space = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);

    CGImageRef image = CGImageCreate(
        w, h, 8, 24, w * 3, color_space,
        kCGBitmapByteOrderDefault,
        provider, NULL, true, kCGRenderingIntentDefault
    ); 

    // draw the image

    [view lockFocus];

    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

    CGContextSaveGState(context);

    CGContextSetAllowsAntialiasing(context, false);
    CGContextSetRGBFillColor(context, 0, 0, 0, 1.0);
    CGContextSetRGBStrokeColor(context, 0, 0, 0, 1.0);

    CGContextTranslateCTM(context, 0, view.bounds.size.height);
    CGContextScaleCTM(context, 1.0, -1.0);

    CGContextDrawImage(context, NSRectToCGRect(view.bounds), image);

    CGContextRestoreGState(context);

    // have to draw something on the dc or it doesn't allow the frame to appear,
    // I don't know of any better way to do this.
    {
        wxCoord w, h;
        dc.GetSize(&w, &h);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(w-2, h-2, w, h);
    }

    [view unlockFocus];

    // and release everything

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(color_space);
    CGImageRelease(image);
}
