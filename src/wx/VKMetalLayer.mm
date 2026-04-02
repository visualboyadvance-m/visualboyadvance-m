#ifndef NO_METAL

// ============================================================================
//  vkdrawingpanel_metal.mm  —  Objective-C++ translation unit
//
//  Must be compiled as Objective-C++ (.mm).  Add to CMakeLists.txt:
//
//    target_sources(visualboyadvance-m PRIVATE
//        src/wx/vkdrawingpanel_metal.mm)
//    set_source_files_properties(src/wx/vkdrawingpanel_metal.mm
//        PROPERTIES COMPILE_FLAGS "-x objective-c++")
//
//  Link against:
//    QuartzCore   (for CAMetalLayer)
//    AppKit       (for NSView / NSWindow)
// ============================================================================
 
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
 
// ---------------------------------------------------------------------------
// VKBEnsureMetalLayer(ns_view)
//
// Given an NSView*, guarantees it has a CAMetalLayer as its wantsLayer
// backing.  Returns the CAMetalLayer* so it can be passed directly to
// VkMetalSurfaceCreateInfoEXT::pLayer.
//
// Why this is necessary:
//   vkCreateMetalSurfaceEXT accepts either an NSView* or a CAMetalLayer*.
//   When passed an NSView* MoltenVK calls [view.layer isKindOfClass:
//   [CAMetalLayer class]].  If the layer is nil or a plain CALayer it throws
//   an ObjC exception → "Unhandled unknown exception" → process exit 255.
//
// Thread safety: call only from the main thread (AppKit requirement).
// ---------------------------------------------------------------------------
extern "C"
void* VKBEnsureMetalLayer(void* ns_view_ptr)
{
    NSView* view = (__bridge NSView*)ns_view_ptr;
    if (!view)
        return nullptr;
 
    // wantsLayer must be set before accessing .layer on a non-layer-backed view.
    if (!view.wantsLayer)
        view.wantsLayer = YES;
 
    // If there's already a CAMetalLayer we're done.
    if ([view.layer isKindOfClass:[CAMetalLayer class]])
        return (__bridge void*)view.layer;
 
    // Install a fresh CAMetalLayer.
    CAMetalLayer* metal_layer = [CAMetalLayer layer];
 
    // Match the view's natural pixel density (Retina / non-Retina).
    // NSScreen.mainScreen may be wrong if the window is on a secondary
    // display; we prefer the window's own screen when it's available.
    NSScreen* screen = view.window ? view.window.screen
                                   : [NSScreen mainScreen];
    metal_layer.contentsScale = screen ? screen.backingScaleFactor : 1.0;
 
    // Pixel format must match what we'll request for the swapchain.
    // BGRa8Unorm is the only format guaranteed by MoltenVK.
    metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
 
    // Disable the implicit Core Animation animations on resize — we drive
    // the frame rate ourselves via Vulkan present.
    metal_layer.displaySyncEnabled   = NO;   // we handle vsync in Vulkan
    metal_layer.allowsNextDrawableTimeout = YES;
 
    view.layer = metal_layer;
    return (__bridge void*)metal_layer;
}
 
// ---------------------------------------------------------------------------
// VKBGetMetalLayer(ns_view)
//
// Returns the existing CAMetalLayer* for a view, or nullptr.
// Safe to call without side effects (read-only).
// ---------------------------------------------------------------------------
extern "C"
void* VKBGetMetalLayer(void* ns_view_ptr)
{
    NSView* view = (__bridge NSView*)ns_view_ptr;
    if (!view || !view.layer)
        return nullptr;
    if ([view.layer isKindOfClass:[CAMetalLayer class]])
        return (__bridge void*)view.layer;
    return nullptr;
}

#endif

