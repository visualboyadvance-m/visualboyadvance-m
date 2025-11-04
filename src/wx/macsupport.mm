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
        renderPassDescriptor = nil;
        commandBuffer = nil;
        renderEncoder = nil;

        if (metalView != nil)
            [metalView removeFromSuperview];

        if (_device != nil)
            [_device release];

        if (_commandQueue != nil)
            [_commandQueue release];

        if (_pipelineState != nil)
            [_pipelineState release];

        if (_texture != nil)
            [_texture release];

        if (_vertices != nil)
            [_vertices release];

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
    ((CAMetalLayer *)metalView.layer).device = metalView.device;

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
    CreateMetalView();

    did_init = true;
}

id<MTLTexture> MetalDrawingPanel::loadTextureUsingData(void *data)
{
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];

    // Indicate that each pixel has a blue, green, red, and alpha channel, where each channel is
    // an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and 255 maps to 1.0)
    textureDescriptor.pixelFormat = metalView.colorPixelFormat;
    textureDescriptor.usage = MTLTextureUsageShaderRead;

    // Set the pixel dimensions of the texture
    textureDescriptor.width = width * scale;
    textureDescriptor.height = height * scale;

    // Create the texture from the device by using the descriptor
    id<MTLTexture> texture = [_device newTextureWithDescriptor:textureDescriptor];

    // Release the descriptor after use
    [textureDescriptor release];

    // Calculate the number of bytes per row in the image.
    NSUInteger bytesPerRow = std::ceil((width + 1) * scale * 4);

    MTLRegion region = {
        { 0, 0, 0 },                   // MTLOrigin
        {(NSUInteger)(width * scale), (NSUInteger)(height * scale), 1} // MTLSize
    };

    // Copy the bytes from the data object into the texture
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
            int pos = 0;
            int src_pos = 0;
            int scaled_border = (int)std::ceil(inrb * scale);
            uint8_t *src = todraw + srcPitch;
            uint32_t *dst = (uint32_t *)calloc(4, std::ceil((width * scale) * (height * scale) + 1));

            for (int y = 0; y < (height * scale); y++) {
                for (int x = 0; x < (width * scale); x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                    if (src[src_pos] == 0xff) {
                        dst[pos] = 0x00ffffff;
                    } else {
                        dst[pos] = (src[src_pos] & 0xe0) + ((src[src_pos] & 0x1c) << 11) + ((src[src_pos] & 0x3) << 22);
                    }
#else
                    if (src[src_pos] == 0xff) {
                        dst[pos] = 0xffffff00;
                    } else {
                        dst[pos] = ((src[src_pos] & 0xe0) << 24) + ((src[src_pos] & 0x1c) << 19) + ((src[src_pos] & 0x3) << 14);
                    }
#endif
                    pos++;
                    src_pos++;
                }
                pos++;
                src_pos += scaled_border;
            }

            if (_texture != nil) {
                [_texture release];
                _texture = nil;
            }
            _texture = loadTextureUsingData(dst);

            if (dst != NULL) {
                free(dst);
            }
        } else if (systemColorDepth == 16) {
            int pos = 0;
            int src_pos = 0;
            int scaled_border = (int)std::ceil(inrb * scale);
            uint8_t *src = todraw + srcPitch;
            uint32_t *dst = (uint32_t *)calloc(4, std::ceil((width * scale) * (height * scale) + 1));
            uint16_t *src16 = (uint16_t *)src;

            for (int y = 0; y < (height * scale); y++) {
                for (int x = 0; x < (width * scale); x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                    if (src16[src_pos] == 0x7fff) {
                        dst[pos] = 0x00ffffff;
                    } else {
                        dst[pos] = ((src16[src_pos] & 0x7c00) >> 7) + ((src16[src_pos] & 0x03e0) << 6) + ((src16[src_pos] & 0x1f) << 19);
                    }
#else
                    if (src16[src_pos] == 0x7fff) {
                        dst[pos] = 0xffffff00;
                    } else {
                        dst[pos] = ((src16[src_pos] & 0x7c00) << 17) + ((src16[src_pos] & 0x03e0) << 14) + ((src16[src_pos] & 0x1f) << 11);
                    }
#endif
                    pos++;
                    src_pos++;
                }
                pos++;
                src_pos += scaled_border;
            }

            if (_texture != nil) {
                [_texture release];
                _texture = nil;
            }
            _texture = loadTextureUsingData(dst);

            if (dst != NULL) {
                free(dst);
            }
        } else if (systemColorDepth == 24) {
            int pos = 0;
            int src_pos = 0;
            uint8_t *src = todraw + srcPitch;
            uint8_t *dst = (uint8_t *)calloc(4, std::ceil((width * scale) * (height * scale) + 1));

            for (int y = 0; y < (height * scale); y++) {
                for (int x = 0; x < (width * scale); x++) {
                    dst[pos] = src[src_pos];
                    dst[pos+1] = src[src_pos+1];
                    dst[pos+2] = src[src_pos+2];
                    dst[pos+3] = 0;

                    pos += 4;
                    src_pos += 3;
                }

                pos += 4;
            }

            if (_texture != nil) {
                [_texture release];
                _texture = nil;
            }
            _texture = loadTextureUsingData(dst);

            if (dst != NULL) {
                free(dst);
            }
        } else {
            if (_texture != nil) {
                [_texture release];
                _texture = nil;
            }
            _texture = loadTextureUsingData(todraw + srcPitch);
        }

        // Create a new command buffer for each render pass to the current drawable
        commandBuffer = [_commandQueue commandBuffer];
        commandBuffer.label = @"MyCommand";

        // Obtain a renderPassDescriptor
        renderPassDescriptor = [metalView currentRenderPassDescriptor];

        if(renderPassDescriptor != nil)
        {
            // Cache the drawable to avoid potential race condition
            id<CAMetalDrawable> currentDrawable = metalView.currentDrawable;

            renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
            renderEncoder.label = @"MyRenderEncoder";

            [renderEncoder setViewport:(MTLViewport){0.0, 0.0, (double)(_contentSize.x), (double)(_contentSize.y), 0.0, 1.0 }];

            [renderEncoder setRenderPipelineState:_pipelineState];

            [renderEncoder setVertexBuffer:_vertices
                                    offset:0
                                  atIndex:AAPLVertexInputIndexVertices];

            [renderEncoder setVertexBytes:&_viewportSize
                                   length:sizeof(_viewportSize)
                                  atIndex:AAPLVertexInputIndexViewportSize];

            // Set the texture object.  The AAPLTextureIndexBaseColor enum value corresponds
            ///  to the 'colorMap' argument in the 'samplingShader' function because its
            //   texture attribute qualifier also uses AAPLTextureIndexBaseColor for its index.
            [renderEncoder setFragmentTexture:_texture
                                      atIndex:AAPLTextureIndexBaseColor];

            // Draw the triangles.
            [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                              vertexStart:0
                              vertexCount:_numVertices];

            [renderEncoder endEncoding];

            // Schedule a present once the framebuffer is complete using the cached drawable
            if (currentDrawable) {
                [commandBuffer presentDrawable:currentDrawable];
            }
        }

        // Finalize rendering here & push the command buffer to the GPU
        [commandBuffer commit];
    }
}
#endif

Quartz2DDrawingPanel::Quartz2DDrawingPanel(wxWindow* parent, int _width, int _height)
    : BasicDrawingPanel(parent, _width, _height)
{
}

void Quartz2DDrawingPanel::DrawImage(wxWindowDC& dc, wxImage* im)
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
