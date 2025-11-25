#include <cmath>
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

#include <wx/rawbmp.h>
#include <wx/log.h>

#include "wx/drawing.h"
#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/wxvbam.h"

#ifndef NO_METAL
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

void MetalDrawingPanel::CreateMetalView()
{
    view = (NSView *)wxGetApp().frame->GetPanel()->GetHandle();
    view.layerContentsPlacement = NSViewLayerContentsPlacementCenter;
    view.layer.backgroundColor = [NSColor colorWithCalibratedRed:0.0f
                                                           green:0.0f
                                                            blue:0.0f
                                                           alpha:0.0f].CGColor;

    metalView = [[MTKView alloc] init];

    metalView.layer.backgroundColor = [NSColor colorWithCalibratedRed:0.0f
                                                                green:0.0f
                                                                 blue:0.0f
                                                                alpha:0.0f].CGColor;
    metalView.device = MTLCreateSystemDefaultDevice();
    metalView.colorPixelFormat = MTLPixelFormatRGBA8Unorm;

    metalView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
    metalView.layerContentsPlacement = NSViewLayerContentsPlacementCenter;
    metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    metalView.layer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;
    metalView.layer.needsDisplayOnBoundsChange = YES;

    CAMetalLayer *metalLayer = (CAMetalLayer *)metalView.layer;
    metalLayer.device = metalView.device;

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

    metalView.wantsLayer = YES;
    metalView.layer.contentsScale = 1.0;
    metalView.autoResizeDrawable = NO;
    metalView.drawableSize = metalFrame.size;
    ((CAMetalLayer *)metalView.layer).drawableSize = metalFrame.size;

    _contentSize.x = metalFrame.size.width;
    _contentSize.y = metalFrame.size.height;
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
    _conversion_buffer = NULL;
    _conversion_buffer_size = 0;

    CreateMetalView();

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

    metalView.drawableSize = metalFrame.size;
    ((CAMetalLayer *)metalView.layer).drawableSize = metalFrame.size;

    _contentSize.x = metalFrame.size.width;
    _contentSize.y = metalFrame.size.height;
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

        int inrb = (systemColorDepth == 8) ? 4 : (systemColorDepth == 16) ? 2 : (systemColorDepth == 24) ? 0 : 1;
        if (systemColorDepth == 8) {
            srcPitch = std::ceil((width + inrb) * scale * 1);
        } else if (systemColorDepth == 16) {
            srcPitch = std::ceil((width + inrb) * scale * 2);
        } else if (systemColorDepth == 24) {
            srcPitch = std::ceil(width * scale * 3);
        } else {
            srcPitch = std::ceil((width + inrb) * scale * 4);
        }

        if (systemColorDepth == 8) {
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
        } else if (systemColorDepth == 16) {
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
        } else if (systemColorDepth == 24) {
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
        } else {
            // Release old texture and create new one
            // 32bpp todraw has borders; skip 1 row of top border
            if (_texture != nil) {
                [_texture release];
            }
            _texture = CreateTextureWithData(todraw + srcPitch, srcPitch);
        }

        // Get the drawable from MTKView (non-blocking)
        id<CAMetalDrawable> drawable = metalView.currentDrawable;
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
}

void Quartz2DDrawingPanel::DrawImage([[maybe_unused]] wxWindowDC& dc, wxImage* im)
{
    NSView *view = (NSView *)GetWindow()->GetHandle();
    size_t w    = std::ceil(width  * scale);
    size_t h    = std::ceil(height * scale);
    size_t size = w * h * 3;

    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, im->GetData(), size, NULL);

    CGColorSpaceRef color_space = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    CGImageRef image = nil;

    image = CGImageCreate(w, h, 8, 24, w * 3, color_space, kCGBitmapByteOrderDefault,
                          provider, NULL, true, kCGRenderingIntentDefault);

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
