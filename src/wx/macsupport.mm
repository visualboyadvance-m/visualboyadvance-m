#include <cmath>
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <IOKit/IOKitLib.h>

#include <wx/rawbmp.h>
#include <wx/log.h>

#include "wx/drawing.h"
#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/wxvbam.h"

// True if any screen is capable of extended dynamic range (EDR / HDR).
//
// Use maximumPotentialExtendedDynamicRangeColorComponentValue, not
// maximumExtendedDynamicRangeColorComponentValue: the latter is the headroom
// *currently* allocated, which stays 1.0 until some layer actually requests EDR
// content, so it would report "no HDR" at startup even on an HDR display. The
// "potential" value reflects the display's capability regardless.
bool VbamProbeMacosHdr() {
    for (NSScreen* screen in [NSScreen screens]) {
        if (screen.maximumPotentialExtendedDynamicRangeColorComponentValue > 1.0)
            return true;
    }
    return false;
}

// The display's EDR headroom -- a multiplier over SDR white (1.0 == no HDR). The
// EDR path clips content above this, so the headroom times our SDR-white nits is
// the display's usable peak in our luminance model. Uses the window's screen when
// available, else the brightest connected screen. Returns 1.0 when HDR is off or
// unsupported. "Potential" (capability) rather than the currently-allocated value.
double VbamMacosMaxEdrHeadroom() {
    NSScreen* window_screen =
        wxGetApp().frame ? ((NSView*)wxGetApp().frame->GetHandle()).window.screen : nil;
    if (window_screen)
        return window_screen.maximumPotentialExtendedDynamicRangeColorComponentValue;

    double best = 1.0;
    for (NSScreen* screen in [NSScreen screens]) {
        double h = screen.maximumPotentialExtendedDynamicRangeColorComponentValue;
        if (h > best)
            best = h;
    }
    return best;
}

// SDL, given an external Cocoa view, makes that view the host NSWindow's
// contentView (its macOS embedding model wraps/takes over a window), which
// detaches the entire wx view tree -- including the status bar -- from the
// window. We undo that after SDL sets up: capture the view SDL was handed, its
// original superview, and the window's original contentView before the takeover
// so it can be reattached.
//
// Capture just before SDL_CreateWindow. Retains the original contentView (SDL is
// about to displace it, dropping the window's reference); the superview and
// window are wx-owned and outlive this panel, so they are held weakly. Outputs
// are null when the view is not yet in a window (nothing to undo).
void VbamSdlCaptureViewState(void* sdl_view, void** out_window,
                             void** out_content_view, void** out_superview) {
    *out_window = nullptr;
    *out_content_view = nullptr;
    *out_superview = nullptr;
    NSView* v = (__bridge NSView*)sdl_view;
    NSWindow* w = v.window;
    if (!w)
        return;
    *out_window = (__bridge void*)w;
    *out_content_view = (void*)CFBridgingRetain(w.contentView);
    *out_superview = (__bridge void*)v.superview;
}

// Undo SDL's takeover: re-nest SDL's view under its original superview and put
// wx's original contentView back as the window's content. SDL keeps rendering
// into its now-nested view, and the wx tree (status bar) is live and laid out by
// wx again. Balances the contentView retain from capture.
void VbamSdlReattachViewState(void* sdl_view, void* window,
                              void* content_view, void* superview) {
    NSView* v = (__bridge NSView*)sdl_view;
    NSWindow* w = (__bridge NSWindow*)window;
    NSView* cv = (NSView*)CFBridgingRelease(content_view);
    NSView* sv = (__bridge NSView*)superview;
    if (!v || !w || !cv || !sv)
        return;
    if (v.superview != sv)
        [sv addSubview:v];
    if (w.contentView != cv)
        w.contentView = cv;
}

void VbamRemoveSdlMetalViews() {
    NSView* host = (NSView*)wxGetApp().frame->GetPanel()->GetHandle();
    if (!host)
        return;
    // Copy: removeFromSuperview mutates the live subviews array.
    for (NSView* sub in [[host.subviews copy] autorelease]) {
        // The wx panel subviews are plain (layer=nil); only SDL's leftover
        // metal view is CAMetalLayer-backed, so this targets it uniquely.
        if ([sub.layer isKindOfClass:[CAMetalLayer class]])
            [sub removeFromSuperview];
    }
}

// Parse an EDID's CTA-861 extension block(s) for the HDR Static Metadata Data
// Block (extended tag 0x06). Returns true when it advertises a PQ (SMPTE ST.2084)
// or HLG EOTF -- i.e. the display is intrinsically HDR-capable. This reflects the
// panel's hardware capability and is independent of the current macOS HDR mode,
// unlike the NSScreen EDR headroom (which collapses to 1.0 when HDR is off).
static bool EdidDeclaresHdr(const uint8_t* edid, size_t len) {
    if (len < 128)
        return false;
    const int extensions = edid[126];
    for (int e = 1; e <= extensions; ++e) {
        const size_t base = static_cast<size_t>(e) * 128;
        if (base + 128 > len)
            break;
        const uint8_t* ext = edid + base;
        if (ext[0] != 0x02)              // CTA-861 extension tag
            continue;
        const int dtd_start = ext[2];    // end of the data-block collection
        if (dtd_start < 4)
            continue;
        size_t i = 4;
        while (i < static_cast<size_t>(dtd_start) && i < 128) {
            const uint8_t tag = ext[i] >> 5;
            const uint8_t blocklen = ext[i] & 0x1f;
            if (blocklen == 0)
                break;
            // Use-Extended-Tag block (tag 7), extended tag 0x06 = HDR Static
            // Metadata. Byte after the ext tag is the supported-EOTF bitmap:
            // bit0 SDR, bit1 traditional HDR, bit2 PQ (ST.2084), bit3 HLG.
            if (tag == 7 && blocklen >= 2 && ext[i + 1] == 0x06) {
                if (ext[i + 2] & 0x0c)   // PQ or HLG
                    return true;
            }
            i += 1 + blocklen;
        }
    }
    return false;
}

// True if a connected display is HDR-capable (per its EDID) but macOS is not
// currently presenting HDR -- i.e. the System Settings "High Dynamic Range"
// toggle is off. Detected as: no screen has EDR headroom (so VbamProbeMacosHdr
// would report unavailable) yet some display's EDID advertises HDR. Mirrors the
// Windows "supported but off" case so the dialog can suppress the "HDR not
// available" warning for a deliberate, reversible off-state.
bool VbamMacosHdrSupportedButOff() {
    // Any EDR headroom means HDR is actually on -- not the "off" case.
    for (NSScreen* screen in [NSScreen screens]) {
        if (screen.maximumPotentialExtendedDynamicRangeColorComponentValue > 1.0)
            return false;
    }
    // No headroom anywhere: is any connected panel HDR-capable in hardware?
    bool capable = false;
    io_iterator_t it = 0;
    if (IOServiceGetMatchingServices(kIOMainPortDefault,
            IOServiceMatching("IODisplay"), &it) == KERN_SUCCESS) {
        io_service_t svc;
        while ((svc = IOIteratorNext(it))) {
            CFMutableDictionaryRef props = NULL;
            if (IORegistryEntryCreateCFProperties(
                    svc, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
                NSData* edid = ((__bridge NSDictionary*)props)[@"IODisplayEDID"];
                if (edid && EdidDeclaresHdr(
                        static_cast<const uint8_t*>(edid.bytes), edid.length))
                    capable = true;
                CFRelease(props);
            }
            IOObjectRelease(svc);
            if (capable)
                break;
        }
        IOObjectRelease(it);
    }
    return capable;
}

// macOS version checks. Not Metal-specific: the Vulkan (MoltenVK) path also
// relies on these, so they must exist even when the Metal renderer is absent.
bool is_macosx_1013_or_newer()
{
    // Mac OS X 10.13 version check
    if (NSAppKitVersionNumber >= 1561) {
        return true;
    }

    return false;
}

bool is_macosx_1012_or_newer()
{
    // Mac OS X 10.12 version check
    if (NSAppKitVersionNumber >= 1504) {
        return true;
    }

    return false;
}

bool is_macosx_11_or_newer()
{
    // macOS 11.0 version check
    if (NSAppKitVersionNumber >= 2022) {
        return true;
    }

    return false;
}

#ifndef NO_METAL
MetalDrawingPanel::~MetalDrawingPanel()
{
    if (did_init)
    {
        // Wait for all pending GPU work to complete before releasing resources
        if (_commandQueue != nil) {
            // Finish any pending command buffers to prevent use-after-free
            id<MTLCommandBuffer> syncBuffer = [_commandQueue commandBuffer];
            [syncBuffer commit];
            [syncBuffer waitUntilCompleted];
        }

        // Clean up conversion buffer
        if (_conversion_buffer != NULL) {
            free(_conversion_buffer);
            _conversion_buffer = NULL;
        }

        if (metalView != nil) {
            [metalView removeFromSuperview];
            [metalView release];
            metalView = nil;
        }

        if (_texture != nil) {
            [_texture release];
            _texture = nil;
        }

        if (_vertices != nil) {
            [_vertices release];
            _vertices = nil;
        }

        if (_sampler != nil) {
            [_sampler release];
            _sampler = nil;
        }

        if (_pipelineState != nil) {
            [_pipelineState release];
            _pipelineState = nil;
        }

        if (_commandQueue != nil) {
            [_commandQueue release];
            _commandQueue = nil;
        }

        if (_device != nil) {
            [_device release];
            _device = nil;
        }

        did_init = false;
    }
}

void MetalDrawingPanel::EraseBackground(wxEraseEvent& ev)
{
    (void)ev; // unused params
    // do nothing, do not allow propagation
}

// The view's backing scale factor (2 or 3 on Retina/HiDPI). A CAMetalLayer's
// drawableSize is in physical pixels, so point sizes must be multiplied by this
// or the framebuffer comes up at half resolution and gets upscaled (soft).
static CGFloat MetalBackingScale(NSView* v) {
    CGFloat sf = v.window ? v.window.backingScaleFactor : 0.0;
    if (sf <= 0.0)
        sf = NSScreen.mainScreen.backingScaleFactor;
    return sf > 0.0 ? sf : 1.0;
}

void MetalDrawingPanel::CreateMetalView()
{
    view = (NSView *)wxGetApp().frame->GetPanel()->GetHandle();
    view.layerContentsPlacement = NSViewLayerContentsPlacementCenter;
    view.layer.backgroundColor = [NSColor colorWithCalibratedRed:0.0f
                                                           green:0.0f
                                                            blue:0.0f
                                                           alpha:0.0f].CGColor;

    metalView = [[MTKView alloc] init];

    // We drive rendering imperatively from DrawArea() (per emulated frame and on
    // paint), acquiring and presenting currentDrawable ourselves. Leave MTKView's
    // own CADisplayLink draw loop off, otherwise it cycles/presents drawables on
    // its vsync timer concurrently with our manual presents, which intermittently
    // leaves part of an older frame on screen (e.g. a stale strip at one edge).
    metalView.paused = YES;
    metalView.enableSetNeedsDisplay = NO;

    metalView.layer.backgroundColor = [NSColor colorWithCalibratedRed:0.0f
                                                                green:0.0f
                                                                 blue:0.0f
                                                                alpha:0.0f].CGColor;
    metalView.device = MTLCreateSystemDefaultDevice();

    // HDR / EDR: when requested and the screen has dynamic-range headroom, use a
    // float back buffer in linear extended-sRGB so values above 1.0 map to the
    // panel's extra brightness. The encoder produces matching scRGB fp16 data.
    metal_is_hdr_ = false;
    if (OPTION(kDispHDR)) {
        NSScreen* screen = view.window.screen ?: [NSScreen mainScreen];
        // Potential, not current: the layer has not requested EDR content yet,
        // so the current headroom is still 1.0 at this point.
        if (screen.maximumPotentialExtendedDynamicRangeColorComponentValue > 1.0)
            metal_is_hdr_ = true;
    }

    metalView.colorPixelFormat =
        metal_is_hdr_ ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm;

    if (metal_is_hdr_) {
        CAMetalLayer* hdrLayer = (CAMetalLayer*)metalView.layer;
        hdrLayer.wantsExtendedDynamicRangeContent = YES;
        CGColorSpaceRef cs =
            CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearDisplayP3);
        hdrLayer.colorspace = cs;
        if (cs) CGColorSpaceRelease(cs);
    }

    metalView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
    metalView.layerContentsPlacement = NSViewLayerContentsPlacementCenter;
    metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    metalView.layer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;
    metalView.layer.needsDisplayOnBoundsChange = YES;

    CAMetalLayer *metalLayer = (CAMetalLayer *)metalView.layer;
    metalLayer.device = metalView.device;

    // Enable VSync based on user preference
    metalLayer.displaySyncEnabled = OPTION(kPrefVsync);

    _device = [metalView.device retain];

    const AAPLVertex quadVertices[] =
    {
        // Pixel positions, Texture coordinates
        { { (float)(width * scale),  (float)-(height * scale) },  { 1.f, 1.f } },
        { { (float)-(width * scale), (float)-(height * scale) },  { 0.f, 1.f } },
        { { (float)-(width * scale), (float)(height * scale) },  { 0.f, 0.f } },

        { { (float)(width * scale), (float)-(height * scale) },  { 1.f, 1.f } },
        { { (float)-(width * scale), (float)(height * scale) },  { 0.f, 0.f } },
        { { (float)(width * scale), (float)(height * scale) },  { 1.f, 0.f } },
    };

    // Create a vertex buffer, and initialize it with the quadVertices array
    _vertices = [_device newBufferWithBytes:quadVertices
                                     length:sizeof(quadVertices)
                                    options:MTLResourceStorageModeShared];

    // Calculate the number of vertices by dividing the byte length by the size of each vertex
    _numVertices = sizeof(quadVertices) / sizeof(AAPLVertex);

    /// Create the render pipeline.

    @autoreleasepool {
        // Load the shaders from the default library
        id<MTLLibrary> defaultLibrary = [_device newDefaultLibrary];
        id<MTLFunction> vertexFunction = [defaultLibrary newFunctionWithName:@"vertexShader"];
        id<MTLFunction> fragmentFunction = [defaultLibrary newFunctionWithName:@"samplingShader"];

        // Set up a descriptor for creating a pipeline state object
        MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.label = @"Texturing Pipeline";
        pipelineStateDescriptor.vertexFunction = vertexFunction;
        pipelineStateDescriptor.fragmentFunction = fragmentFunction;
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = metalView.colorPixelFormat;

        NSError *error = NULL;
        _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor
                                                                 error:&error];
        if (!_pipelineState) {
            wxLogError(_("Failed to create Metal pipeline state: %s"), [[error localizedDescription] UTF8String]);
            [pipelineStateDescriptor release];
            [fragmentFunction release];
            [vertexFunction release];
            [defaultLibrary release];
            return;
        }

        // Release temporary objects
        [pipelineStateDescriptor release];
        [fragmentFunction release];
        [vertexFunction release];
        [defaultLibrary release];

        _commandQueue = [_device newCommandQueue];
    }

    // Create sampler for texture filtering (bilinear or nearest)
    MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    samplerDescriptor.minFilter = OPTION(kDispBilinear) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    samplerDescriptor.magFilter = OPTION(kDispBilinear) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    _sampler = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    [samplerDescriptor release];

    if (OPTION(kDispStretch) == false) {
        metalView.frame = view.frame;
        metalFrame = view.frame;
    } else {
        float scaleX = view.frame.size.width / (width * scale);
        float scaleY = view.frame.size.height / (height * scale);
        float scaleF = 0;
        if (scaleX < scaleY) {
            scaleF = scaleX;
        } else {
            scaleF = scaleY;
        }
        
        if (scaleF == 0) {
            scaleF = 1;
        }
        
        metalFrame.size.width = (width * scale) * scaleF;
        metalFrame.size.height = (height * scale) * scaleF;
        metalFrame.origin.x = (view.frame.size.width - metalFrame.size.width) / 2;
        metalFrame.origin.y = (view.frame.size.height - metalFrame.size.height) / 2;
        metalView.frame = metalFrame;
    }

    // drawableSize is in physical pixels: scale the point-sized frame by the
    // backing factor so the framebuffer (and the render viewport) is at native
    // resolution rather than half-res upscaled by Core Animation on Retina.
    const CGFloat sf = MetalBackingScale(view);
    const CGSize drawablePx =
        CGSizeMake(std::ceil(metalFrame.size.width * sf),
                   std::ceil(metalFrame.size.height * sf));

    metalView.wantsLayer = YES;
    metalView.layer.contentsScale = sf;
    metalView.autoResizeDrawable = NO;
    metalView.drawableSize = drawablePx;
    ((CAMetalLayer *)metalView.layer).drawableSize = drawablePx;

    _contentSize.x = drawablePx.width;
    _contentSize.y = drawablePx.height;
    _viewportSize.x = width * scale;
    _viewportSize.y = height * scale;

    [view addSubview:metalView];
}

void MetalDrawingPanel::DrawingPanelInit()
{
    // Initialize Objective-C pointers to nil to prevent accessing uninitialized memory
    metalView = nil;
    _device = nil;
    _commandQueue = nil;
    _pipelineState = nil;
    _texture = nil;
    _vertices = nil;
    _sampler = nil;
    _conversion_buffer = NULL;
    _conversion_buffer_size = 0;

    CreateMetalView();

    // CreateMetalView() has decided metal_is_hdr_; fold the brightness settings
    // into the encoder and activate the scRGB encoding if HDR is on.
    UpdateHdrState();

    did_init = true;
}

id<MTLTexture> MetalDrawingPanel::CreateTextureWithData(void *data, NSUInteger bytesPerRow)
{
    // Create a new texture each frame to avoid GPU synchronization issues
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
    textureDescriptor.pixelFormat = metalView.colorPixelFormat;
    textureDescriptor.usage = MTLTextureUsageShaderRead;
    textureDescriptor.width = width * scale;
    textureDescriptor.height = height * scale;

    id<MTLTexture> texture = [_device newTextureWithDescriptor:textureDescriptor];
    [textureDescriptor release];

    MTLRegion region = {
        { 0, 0, 0 },                   // MTLOrigin
        {(NSUInteger)(width * scale), (NSUInteger)(height * scale), 1} // MTLSize
    };

    // Upload pixel data to the new texture
    [texture replaceRegion:region
                mipmapLevel:0
                  withBytes:data
                bytesPerRow:bytesPerRow];

    return texture;
}

void MetalDrawingPanel::OnSize(wxSizeEvent& ev)
{
    if (OPTION(kDispStretch) == false) {
        metalView.frame = view.frame;
        metalFrame = view.frame;
    } else {
        float scaleX = view.frame.size.width / (width * scale);
        float scaleY = view.frame.size.height / (height * scale);
        float scaleF = 0;
        if (scaleX < scaleY) {
            scaleF = scaleX;
        } else {
            scaleF = scaleY;
        }
        
        if (scaleF == 0) {
            scaleF = 1;
        }
        
        metalFrame.size.width = (width * scale) * scaleF;
        metalFrame.size.height = (height * scale) * scaleF;
        metalFrame.origin.x = (view.frame.size.width - metalFrame.size.width) / 2;
        metalFrame.origin.y = (view.frame.size.height - metalFrame.size.height) / 2;
        metalView.frame = metalFrame;
    }

    // Match CreateMetalView: drawable in physical pixels (native, not half-res).
    const CGFloat sf = MetalBackingScale(view);
    const CGSize drawablePx =
        CGSizeMake(std::ceil(metalFrame.size.width * sf),
                   std::ceil(metalFrame.size.height * sf));

    metalView.layer.contentsScale = sf;
    metalView.drawableSize = drawablePx;
    ((CAMetalLayer *)metalView.layer).drawableSize = drawablePx;

    _contentSize.x = drawablePx.width;
    _contentSize.y = drawablePx.height;
    _viewportSize.x = width * scale;
    _viewportSize.y = height * scale;

    if (todraw) {
        DrawArea();
    }

    ev.Skip();
}

void MetalDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc;
    DrawArea();
}

void MetalDrawingPanel::DrawArea()
{
    @autoreleasepool {
        uint32_t srcPitch = 0;

        if (!did_init)
            DrawingPanelInit();

        int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
        if (panel_color_depth_ == 8) {
            srcPitch = std::ceil((width + inrb) * scale * 1);
        } else if (panel_color_depth_ == 16) {
            srcPitch = std::ceil((width + inrb) * scale * 2);
        } else if (panel_color_depth_ == 24) {
            srcPitch = std::ceil(width * scale * 3);
        } else {
            srcPitch = std::ceil((width + inrb) * scale * 4);
        }

        if (panel_color_depth_ == 8) {
            // Idiomatic Metal: pre-allocate and reuse conversion buffer
            size_t required_size = (width * scale) * (height * scale) * sizeof(uint32_t);
            if (_conversion_buffer_size < required_size) {
                if (_conversion_buffer != NULL) {
                    free(_conversion_buffer);
                }
                _conversion_buffer = (uint32_t *)malloc(required_size);
                _conversion_buffer_size = required_size;
            }

            int pos = 0;
            int src_pos = 0;
            int scaled_border = (int)std::ceil(inrb * scale);
            // Skip 1 row of top border
            uint8_t *src = todraw + srcPitch;

            for (int y = 0; y < (height * scale); y++) {
                for (int x = 0; x < (width * scale); x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                    if (src[src_pos] == 0xff) {
                        _conversion_buffer[pos] = 0x00ffffff;
                    } else {
                        _conversion_buffer[pos] = (src[src_pos] & 0xe0) + ((src[src_pos] & 0x1c) << 11) + ((src[src_pos] & 0x3) << 22);
                    }
#else
                    if (src[src_pos] == 0xff) {
                        _conversion_buffer[pos] = 0xffffff00;
                    } else {
                        _conversion_buffer[pos] = ((src[src_pos] & 0xe0) << 24) + ((src[src_pos] & 0x1c) << 19) + ((src[src_pos] & 0x3) << 14);
                    }
#endif
                    pos++;
                    src_pos++;
                }
                src_pos += scaled_border;
            }

            // Release old texture and create new one with converted data
            if (_texture != nil) {
                [_texture release];
            }
            _texture = CreateTextureWithData(_conversion_buffer, (width * scale) * 4);
        } else if (panel_color_depth_ == 16) {
            // Idiomatic Metal: pre-allocate and reuse conversion buffer
            size_t required_size = (width * scale) * (height * scale) * sizeof(uint32_t);
            if (_conversion_buffer_size < required_size) {
                if (_conversion_buffer != NULL) {
                    free(_conversion_buffer);
                }
                _conversion_buffer = (uint32_t *)malloc(required_size);
                _conversion_buffer_size = required_size;
            }

            int pos = 0;
            int src_pos = 0;
            int scaled_border = (int)std::ceil(inrb * scale);
            // Skip 1 row of top border
            uint8_t *src = todraw + srcPitch;
            uint16_t *src16 = (uint16_t *)src;

            for (int y = 0; y < (height * scale); y++) {
                for (int x = 0; x < (width * scale); x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                    if (src16[src_pos] == 0x7fff) {
                        _conversion_buffer[pos] = 0x00ffffff;
                    } else {
                        _conversion_buffer[pos] = ((src16[src_pos] & 0x7c00) >> 7) + ((src16[src_pos] & 0x03e0) << 6) + ((src16[src_pos] & 0x1f) << 19);
                    }
#else
                    if (src16[src_pos] == 0x7fff) {
                        _conversion_buffer[pos] = 0xffffff00;
                    } else {
                        _conversion_buffer[pos] = ((src16[src_pos] & 0x7c00) << 17) + ((src16[src_pos] & 0x03e0) << 14) + ((src16[src_pos] & 0x1f) << 11);
                    }
#endif
                    pos++;
                    src_pos++;
                }
                src_pos += scaled_border;
            }

            // Release old texture and create new one with converted data
            if (_texture != nil) {
                [_texture release];
            }
            _texture = CreateTextureWithData(_conversion_buffer, (width * scale) * 4);
        } else if (panel_color_depth_ == 24) {
            // Idiomatic Metal: pre-allocate and reuse conversion buffer
            size_t required_size = (width * scale) * (height * scale) * 4; // RGBA
            if (_conversion_buffer_size < required_size) {
                if (_conversion_buffer != NULL) {
                    free(_conversion_buffer);
                }
                _conversion_buffer = (uint32_t *)malloc(required_size);
                _conversion_buffer_size = required_size;
            }

            int pos = 0;
            int src_pos = 0;
            // Skip 1 row of top border
            uint8_t *src = todraw + srcPitch;
            uint8_t *dst = (uint8_t *)_conversion_buffer;

            for (int y = 0; y < (height * scale); y++) {
                for (int x = 0; x < (width * scale); x++) {
                    dst[pos] = src[src_pos];
                    dst[pos+1] = src[src_pos+1];
                    dst[pos+2] = src[src_pos+2];
                    dst[pos+3] = 0;

                    pos += 4;
                    src_pos += 3;
                }
            }

            // Release old texture and create new one with converted data
            if (_texture != nil) {
                [_texture release];
            }
            _texture = CreateTextureWithData(_conversion_buffer, (width * scale) * 4);
        } else if (HdrActive() && metal_is_hdr_ && panel_color_depth_ == 32) {
            // HDR: encode the corrected RGBA8 image to linear scRGB fp16
            // (8 bytes/pixel) matching the RGBA16Float EDR layer.
            int w = (int)(width * scale);
            int h = (int)(height * scale);
            size_t required_size = (size_t)w * h * 8;
            if (_conversion_buffer_size < required_size) {
                if (_conversion_buffer != NULL) free(_conversion_buffer);
                _conversion_buffer = (uint32_t*)malloc(required_size);
                _conversion_buffer_size = required_size;
            }
            hdr::EncodeScRGBFp16(todraw + srcPitch, srcPitch, w, h,
                                 (uint16_t*)_conversion_buffer, w * 8);
            if (_texture != nil) {
                [_texture release];
            }
            _texture = CreateTextureWithData(_conversion_buffer, w * 8);
        } else {
            // Release old texture and create new one
            // 32bpp todraw has borders; skip 1 row of top border
            if (_texture != nil) {
                [_texture release];
            }
            _texture = CreateTextureWithData(todraw + srcPitch, srcPitch);
        }

        // Acquire the drawable straight from the CAMetalLayer rather than
        // MTKView.currentDrawable: with the view paused (no internal draw loop)
        // currentDrawable is only vended inside MTKView's own draw cycle and would
        // be nil here. nextDrawable gives us a fresh drawable per manual present,
        // with no contention from the view's vsync timer.
        CAMetalLayer* metalLayer = (CAMetalLayer*)metalView.layer;
        id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
        if (drawable == nil) {
            return;  // No drawable available, skip this frame
        }

        // Create a new command buffer for each render pass to the next drawable
        id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
        commandBuffer.label = @"VBAMFrame";

        // Create render pass descriptor with the drawable's texture
        MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

        // Create render encoder and encode rendering commands
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"VBAMRenderEncoder";

        [renderEncoder setViewport:(MTLViewport){0.0, 0.0, (double)(_contentSize.x), (double)(_contentSize.y), 0.0, 1.0 }];
        [renderEncoder setRenderPipelineState:_pipelineState];

        [renderEncoder setVertexBuffer:_vertices
                                offset:0
                              atIndex:AAPLVertexInputIndexVertices];

        [renderEncoder setVertexBytes:&_viewportSize
                               length:sizeof(_viewportSize)
                              atIndex:AAPLVertexInputIndexViewportSize];

        [renderEncoder setFragmentTexture:_texture
                                  atIndex:AAPLTextureIndexBaseColor];

        // Set the sampler for texture filtering
        [renderEncoder setFragmentSamplerState:_sampler
                                       atIndex:0];

        // Draw the quad
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:_numVertices];

        [renderEncoder endEncoding];

        // Schedule presentation and commit to GPU
        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];
    }
}
#endif

Quartz2DDrawingPanel::Quartz2DDrawingPanel(wxWindow* parent, int _width, int _height)
    : BasicDrawingPanel(parent, _width, _height)
{
    // BasicDrawingPanel never runs DrawingPanelInit(), so set the HDR state up
    // here (availability is already known from the startup probe).
    UpdateHdrState();
    // Mark initialized so GameArea runs the HDR "working" check
    // (EvaluateHdrRenderer): if this panel isn't presenting HDR while HDR is
    // available, it records the failure and falls back to another HDR renderer
    // (e.g. Metal or Vulkan).
    did_init = true;
}

bool Quartz2DDrawingPanel::SupportsHdr() const
{
    // Probe the EDR headroom of the screen this window is currently on, rather
    // than the global startup probe (which only checks the main screen). EDR is
    // presentable when the screen's maximum extended-dynamic-range component
    // value exceeds 1.0. Falls back to the main screen before the window is
    // placed (e.g. when called from the constructor's UpdateHdrState()).
    NSView* view =
        (NSView*)const_cast<Quartz2DDrawingPanel*>(this)->GetWindow()->GetHandle();
    NSScreen* screen = view ? view.window.screen : nil;
    if (!screen)
        screen = [NSScreen mainScreen];
    if (!screen)
        return false;
    return screen.maximumExtendedDynamicRangeColorComponentValue > 1.0;
}

void Quartz2DDrawingPanel::DrawImage([[maybe_unused]] wxWindowDC& dc, wxImage* im)
{
    NSView *view = (NSView *)GetWindow()->GetHandle();
    size_t w    = std::ceil(width  * scale);
    size_t h    = std::ceil(height * scale);

    const bool hdr = HdrActive() && panel_color_depth_ == 32;

    CGDataProviderRef provider = nullptr;
    CGColorSpaceRef color_space = nullptr;
    CGImageRef image = nil;

    if (hdr) {
        // Enable an EDR, float-backed layer for the view.
        view.wantsLayer = YES;
        view.layer.contentsFormat = kCAContentsFormatRGBA16Float;
        if ([view.layer respondsToSelector:@selector(setWantsExtendedDynamicRangeContent:)])
            [view.layer setValue:@YES forKey:@"wantsExtendedDynamicRangeContent"];

        // Encode the corrected image to linear scRGB fp16 (RGBA halves).
        int rowlen = (int)std::ceil((width + 1) * scale);  // 1px filter border
        const uint8_t* tex_ptr = todraw + (size_t)rowlen * 4;
        int bpp = 0;
        const uint8_t* enc = EncodeHdr(tex_ptr, rowlen * 4, (int)w, (int)h, &bpp);

        provider = CGDataProviderCreateWithData(NULL, enc, w * h * 8, NULL);
        color_space = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearDisplayP3);
        const CGBitmapInfo info = kCGBitmapByteOrder16Little |
                                  kCGBitmapFloatComponents |
                                  (CGBitmapInfo)kCGImageAlphaLast;
        image = CGImageCreate(w, h, 16, 64, w * 8, color_space, info,
                              provider, NULL, false, kCGRenderingIntentDefault);
    } else {
        size_t size = w * h * 3;
        provider = CGDataProviderCreateWithData(NULL, im->GetData(), size, NULL);
        color_space = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
        image = CGImageCreate(w, h, 8, 24, w * 3, color_space, kCGBitmapByteOrderDefault,
                              provider, NULL, true, kCGRenderingIntentDefault);
    }

    // draw the image
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
#else
    [view lockFocus];

    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
#endif

    CGContextSaveGState(context);

    CGContextSetAllowsAntialiasing(context, false);
    CGContextSetRGBFillColor(context, 0, 0, 0, 1.0);
    CGContextSetRGBStrokeColor(context, 0, 0, 0, 1.0);

    CGContextTranslateCTM(context, 0, view.bounds.size.height);
    CGContextScaleCTM(context, 1.0, -1.0);

    CGContextDrawImage(context, NSRectToCGRect(view.bounds), image);

    CGContextRestoreGState(context);
    CGContextFlush(context);

    [view setNeedsDisplay:YES];

#if MAC_OS_X_VERSION_MIN_REQUIRED < 101000
    [view unlockFocus];
#endif

    // and release everything

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(color_space);
    CGImageRelease(image);
}
