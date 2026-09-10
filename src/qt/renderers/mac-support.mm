// Objective-C++ helpers for the macOS renderers of the Qt port. See
// mac-support.h. Everything here runs on the main (AppKit) thread. Compiled
// with -fobjc-arc (see src/qt/CMakeLists.txt).

#include "qt/renderers/mac-support.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

// A plain NSView whose backing layer is a CAMetalLayer, stacked over the Qt
// view of the panel. Qt keeps flushing its (empty, black) backing store into
// its own layer, so the swapchain must not share that layer: MoltenVK and the
// Metal renderer present into this child view instead, which sits on top.
@interface VbamMetalHostView : NSView
@end

@implementation VbamMetalHostView
- (CALayer*)makeBackingLayer {
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    layer.opaque = YES;
    layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    return layer;
}
- (BOOL)isOpaque {
    return YES;
}
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return NO;
}
- (NSView*)hitTest:(NSPoint)point {
    // Let mouse events go to the Qt view underneath.
    (void)point;
    return nil;
}
@end

void* VbamQtEnsureMetalLayer(void* ns_view) {
    NSView* view = (__bridge NSView*)ns_view;
    if (!view)
        return nullptr;

    for (NSView* sub in view.subviews) {
        if ([sub isKindOfClass:[VbamMetalHostView class]])
            return (__bridge void*)sub.layer;
    }

    VbamMetalHostView* host = [[VbamMetalHostView alloc] initWithFrame:view.bounds];
    host.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    host.wantsLayer = YES;
    host.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
    NSScreen* screen = view.window ? view.window.screen : [NSScreen mainScreen];
    host.layer.contentsScale = screen ? screen.backingScaleFactor : 1.0;
    [view addSubview:host positioned:NSWindowAbove relativeTo:nil];
    return (__bridge void*)host.layer;
}

void VbamQtMetalLayerResize(void* metal_layer, int w, int h, double scale) {
    CAMetalLayer* layer = (__bridge CAMetalLayer*)metal_layer;
    if (!layer)
        return;
    if (scale > 0.0)
        layer.contentsScale = scale;
    if (w > 0 && h > 0)
        layer.drawableSize = CGSizeMake(w, h);
}

// Capture just before SDL_CreateWindow. Retains the original contentView (SDL is
// about to displace it, dropping the window's reference); the superview and
// window are Qt-owned and outlive this panel, so they are held weakly. Outputs
// are null when the view is not yet in a window (nothing to undo).
void VbamQtSdlCaptureViewState(void* ns_view, void** out_window, void** out_content_view,
                               void** out_superview, double out_frame[4]) {
    *out_window = nullptr;
    *out_content_view = nullptr;
    *out_superview = nullptr;
    out_frame[0] = out_frame[1] = out_frame[2] = out_frame[3] = 0.0;
    NSView* v = (__bridge NSView*)ns_view;
    NSWindow* w = v.window;
    if (!w)
        return;
    *out_window = (__bridge void*)w;
    *out_content_view = (void*)CFBridgingRetain(w.contentView);
    *out_superview = (__bridge void*)v.superview;
    const NSRect f = w.frame;
    out_frame[0] = f.origin.x;
    out_frame[1] = f.origin.y;
    out_frame[2] = f.size.width;
    out_frame[3] = f.size.height;
}

// Undo SDL's takeover: re-nest SDL's view under its original superview and put
// Qt's original contentView back as the window's content. SDL keeps rendering
// into its now-nested view. Balances the contentView retain from capture.
void VbamQtSdlReattachViewState(void* ns_view, void* window, void* content_view,
                                void* superview, const double frame[4]) {
    NSView* v = (__bridge NSView*)ns_view;
    NSWindow* w = (__bridge NSWindow*)window;
    NSView* cv = (NSView*)CFBridgingRelease(content_view);
    NSView* sv = (__bridge NSView*)superview;
    if (!v || !w || !cv || !sv)
        return;
    if (v.superview != sv)
        [sv addSubview:v];
    if (w.contentView != cv)
        w.contentView = cv;
    // SDL applied its own default window size and position to the host window;
    // put the frame back where Qt had it.
    if (frame[2] > 0.0 && frame[3] > 0.0) {
        const NSRect f = NSMakeRect(frame[0], frame[1], frame[2], frame[3]);
        if (!NSEqualRects(w.frame, f))
            [w setFrame:f display:YES];
    }
}

void VbamQtRemoveSdlMetalViews(void* ns_view) {
    NSView* host = (__bridge NSView*)ns_view;
    if (!host)
        return;
    // Copy: removeFromSuperview mutates the live subviews array.
    for (NSView* sub in [host.subviews copy]) {
        if ([sub.layer isKindOfClass:[CAMetalLayer class]])
            [sub removeFromSuperview];
    }
}

void VbamQtSetAccessoryActivationPolicy() {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
