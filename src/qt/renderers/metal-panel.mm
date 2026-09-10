#include "qt/renderers/metal-panel.h"

#if defined(__APPLE__) && !defined(NO_METAL)

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cmath>
#include <cstring>
#include <vector>

#include <QCoreApplication>
#include <QImage>

#include "core/base/system.h"
#include "qt/config/option-proxy.h"
#include "qt/log.h"
#include "qt/renderers/mac-support.h"

namespace {

QString Tr(const char* s) {
    return QCoreApplication::translate("vbam", s);
}

// A full-screen textured quad. Positions are in clip space (the panel widget is
// already the aspect-fitted rectangle, see GameArea::resizeEvent), so the quad
// simply fills the drawable.
constexpr const char* kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

vertex VertexOut vbam_vertex(uint vid [[vertex_id]]) {
    // Triangle strip: (-1,1) (1,1) (-1,-1) (1,-1) with v flipped (row 0 on top).
    const float2 pos[4] = { float2(-1.0,  1.0), float2(1.0,  1.0),
                            float2(-1.0, -1.0), float2(1.0, -1.0) };
    const float2 uv[4]  = { float2(0.0, 0.0), float2(1.0, 0.0),
                            float2(0.0, 1.0), float2(1.0, 1.0) };
    VertexOut out;
    out.position = float4(pos[vid], 0.0, 1.0);
    out.uv = uv[vid];
    return out;
}

fragment float4 vbam_fragment(VertexOut in [[stage_in]],
                              texture2d<half> tex [[texture(0)]],
                              sampler smp [[sampler(0)]]) {
    return float4(tex.sample(smp, in.uv));
}
)";

}  // namespace

struct MetalDrawingPanel::Impl {
    CAMetalLayer* layer = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLRenderPipelineState> pipeline = nil;
    id<MTLSamplerState> sampler = nil;
    id<MTLTexture> texture = nil;
    MTLPixelFormat texture_format = MTLPixelFormatInvalid;
    int texture_w = 0, texture_h = 0;
};

MetalDrawingPanel::MetalDrawingPanel(QWidget* parent, int _width, int _height)
    : NativeDrawingPanel(parent, _width, _height), impl_(new Impl) {
    DrawingPanelInit();
}

MetalDrawingPanel::~MetalDrawingPanel() {
    StopFilterThreads();
    if (impl_) {
        if (impl_->queue) {
            // Drain pending GPU work before the texture goes away.
            id<MTLCommandBuffer> sync = [impl_->queue commandBuffer];
            [sync commit];
            [sync waitUntilCompleted];
        }
        impl_->texture = nil;
        impl_->sampler = nil;
        impl_->pipeline = nil;
        impl_->queue = nil;
        impl_->device = nil;
        impl_->layer = nil;
        delete impl_;
        impl_ = nullptr;
    }
}

void MetalDrawingPanel::DrawingPanelInit() {
    DrawingPanelBase::DrawingPanelInit();

    @autoreleasepool {
        impl_->layer = (__bridge CAMetalLayer*)VbamQtEnsureMetalLayer(NativeHandle());
        if (!impl_->layer) {
            vbam::LogError(Tr("Failed to create the Metal layer"));
            init_failed_ = true;
            return;
        }

        impl_->device = MTLCreateSystemDefaultDevice();
        if (!impl_->device) {
            vbam::LogError(Tr("No Metal device available"));
            init_failed_ = true;
            return;
        }

        impl_->layer.device = impl_->device;
        impl_->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        impl_->layer.framebufferOnly = YES;
        impl_->layer.displaySyncEnabled = OPTION(kPrefVsync) ? YES : NO;
        const QSize px = DevicePixelSize();
        VbamQtMetalLayerResize((__bridge void*)impl_->layer, px.width(), px.height(),
                               devicePixelRatioF());

        NSError* error = nil;
        id<MTLLibrary> library =
            [impl_->device newLibraryWithSource:[NSString stringWithUTF8String:kShaderSource]
                                        options:nil
                                          error:&error];
        if (!library) {
            vbam::LogError(Tr("Failed to compile the Metal shaders: %1")
                               .arg(QString::fromNSString(error.localizedDescription)));
            init_failed_ = true;
            return;
        }

        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.label = @"VBA-M Texturing Pipeline";
        desc.vertexFunction = [library newFunctionWithName:@"vbam_vertex"];
        desc.fragmentFunction = [library newFunctionWithName:@"vbam_fragment"];
        desc.colorAttachments[0].pixelFormat = impl_->layer.pixelFormat;
        impl_->pipeline = [impl_->device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!impl_->pipeline) {
            vbam::LogError(Tr("Failed to create Metal pipeline state: %1")
                               .arg(QString::fromNSString(error.localizedDescription)));
            init_failed_ = true;
            return;
        }

        MTLSamplerDescriptor* sdesc = [[MTLSamplerDescriptor alloc] init];
        const MTLSamplerMinMagFilter filter =
            OPTION(kDispBilinear) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
        sdesc.minFilter = filter;
        sdesc.magFilter = filter;
        sdesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
        sdesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
        impl_->sampler = [impl_->device newSamplerStateWithDescriptor:sdesc];

        impl_->queue = [impl_->device newCommandQueue];
        if (!impl_->queue) {
            vbam::LogError(Tr("Failed to create the Metal command queue"));
            init_failed_ = true;
            return;
        }
    }

    did_init = true;
}

void MetalDrawingPanel::OnNativeResize(const QSize& device_pixels) {
    if (impl_ && impl_->layer) {
        VbamQtMetalLayerResize((__bridge void*)impl_->layer, device_pixels.width(),
                               device_pixels.height(), devicePixelRatioF());
    }
}

void MetalDrawingPanel::Present() {
    if (!impl_ || !impl_->layer || !impl_->queue || !impl_->pipeline || init_failed_)
        return;

    @autoreleasepool {
        if (todraw) {
            const int w = ScaledWidth(), h = ScaledHeight();
            const void* pixels = nullptr;
            NSUInteger bytes_per_row = 0;
            MTLPixelFormat fmt;
            QImage converted;
            if (panel_color_depth_ == 32) {
                // The core's 32-bit frame is R,G,B,X bytes: upload as RGBA8 as-is.
                pixels = SourcePixels();
                bytes_per_row = SourcePitch();
                fmt = MTLPixelFormatRGBA8Unorm;
            } else {
                // 8/16/24-bit: expand to Qt's RGB32 (B,G,R,X on little endian).
                converted = BuildImage();
                if (converted.isNull())
                    return;
                pixels = converted.constBits();
                bytes_per_row = converted.bytesPerLine();
                fmt = MTLPixelFormatBGRA8Unorm;
            }

            if (!impl_->texture || impl_->texture_w != w || impl_->texture_h != h ||
                impl_->texture_format != fmt) {
                MTLTextureDescriptor* tdesc =
                    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                                       width:w
                                                                      height:h
                                                                   mipmapped:NO];
                tdesc.usage = MTLTextureUsageShaderRead;
                tdesc.storageMode = MTLStorageModeManaged;
                impl_->texture = [impl_->device newTextureWithDescriptor:tdesc];
                impl_->texture_w = w;
                impl_->texture_h = h;
                impl_->texture_format = fmt;
            }
            if (!impl_->texture)
                return;
            [impl_->texture replaceRegion:MTLRegionMake2D(0, 0, w, h)
                              mipmapLevel:0
                                withBytes:pixels
                              bytesPerRow:bytes_per_row];
        }

        id<CAMetalDrawable> drawable = [impl_->layer nextDrawable];
        if (!drawable)
            return;

        id<MTLCommandBuffer> cmd = [impl_->queue commandBuffer];
        cmd.label = @"VBAMFrame";

        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:pass];
        if (todraw && impl_->texture) {
            [enc setViewport:(MTLViewport){0.0, 0.0, (double)drawable.texture.width,
                                           (double)drawable.texture.height, 0.0, 1.0}];
            [enc setRenderPipelineState:impl_->pipeline];
            [enc setFragmentTexture:impl_->texture atIndex:0];
            [enc setFragmentSamplerState:impl_->sampler atIndex:0];
            [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        }
        [enc endEncoding];

        [cmd presentDrawable:drawable];
        [cmd commit];
    }
}

#endif  // __APPLE__ && !NO_METAL
