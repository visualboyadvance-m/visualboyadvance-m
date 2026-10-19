/*
See the LICENSE.txt file for this sample’s licensing information.

Abstract:
Metal shaders used for this sample
*/

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Include header shared between this Metal shader code and C code executing Metal API commands
#include "AAPLShaderTypes.h"

struct RasterizerData
{
    // The [[position]] attribute qualifier of this member indicates this value is
    // the clip space position of the vertex when this structure is returned from
    // the vertex shader
    float4 position [[position]];

    // Since this member does not have a special attribute qualifier, the rasterizer
    // will interpolate its value with values of other vertices making up the triangle
    // and pass that interpolated value to the fragment shader for each fragment in
    // that triangle.
    float2 textureCoordinate;

};

// Vertex Function
vertex RasterizerData
vertexShader(uint vertexID [[ vertex_id ]],
             constant AAPLVertex *vertexArray [[ buffer(AAPLVertexInputIndexVertices) ]],
             constant vector_uint2 *viewportSizePointer  [[ buffer(AAPLVertexInputIndexViewportSize) ]])
{

    RasterizerData out;

    // Index into the array of positions to get the current vertex.
    //   Positions are specified in pixel dimensions (i.e. a value of 100 is 100 pixels from
    //   the origin)
    float2 pixelSpacePosition = vertexArray[vertexID].position.xy;

    // Get the viewport size and cast to float.
    float2 viewportSize = float2(*viewportSizePointer);

    // To convert from positions in pixel space to positions in clip-space,
    //  divide the pixel coordinates by half the size of the viewport.
    // Z is set to 0.0 and w to 1.0 because this is 2D sample.
    out.position = vector_float4(0.0, 0.0, 0.0, 1.0);
    out.position.xy = pixelSpacePosition / viewportSize;

    // Pass the input textureCoordinate straight to the output RasterizerData. This value will be
    //   interpolated with the other textureCoordinate values in the vertices that make up the
    //   triangle.
    out.textureCoordinate = vertexArray[vertexID].textureCoordinate;

    return out;
}

// Fragment function
fragment float4
samplingShader(RasterizerData in [[stage_in]],
               texture2d<half> colorTexture [[ texture(AAPLTextureIndexBaseColor) ]],
               sampler textureSampler [[ sampler(0) ]])
{
    // Sample the texture to obtain a color using the sampler provided from the app
    // This allows bilinear or nearest filtering based on user preferences
    const half4 colorSample = colorTexture.sample(textureSampler, in.textureCoordinate);

    // return the color of the texture
    return float4(colorSample);
}


// hdr::GpuTransfer (src/wx/hdr.h), field for field.
struct HdrScRGBTransfer
{
    float lin[256];
    float4 m[3];
    float4 luma;
    float ref, peak, knee, shadow_contrast;
    float floor_nits, ramp_top, white, pad;
};

constant uchar kBayer8[64] = {
     0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21,
};

// hdr.cpp's DitherLin().
static float HdrDitherLin(uint v, float d, constant HdrScRGBTransfer& p)
{
    if (v == 0)
        return 0.0f;
    const float lo = p.lin[v];
    const float hi = p.lin[min(v + 1, 255u)];
    return max(0.0f, lo + (d - 0.5f) * (hi - lo));
}

// hdr.cpp's LumScale(), where the reasoning is.
static float HdrLumScale(float L, float top_channel, constant HdrScRGBTransfer& p)
{
    const float ref = p.ref;
    const float knee = p.knee;
    const float sc = p.shadow_contrast;

    if (knee >= 1.0f || L <= knee) {
        if (L <= 0.0f)
            return ref;
        const float pivot = knee > 0.0f ? knee : 1.0f;
        const float top = ref * pivot;
        const float floor_nits = p.floor_nits;
        if (floor_nits <= 0.0f) {
            if (sc != 1.0f)
                return ref * pivot * precise::pow(L / pivot, sc) / L;
            return ref;
        }
        const float t = sc != 1.0f ? precise::pow(L / pivot, sc) : L / pivot;
        const float ramp = min(1.0f, top_channel / p.ramp_top);
        return (floor_nits * ramp + (top - floor_nits) * t) / L;
    }

    const float dx = 1.0f - knee;
    const float t = (L - knee) / dx;
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float p0 = ref * knee;
    const float m0 = ref * dx;
    const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const float h10 = t3 - 2.0f * t2 + t;
    const float h01 = -2.0f * t3 + 3.0f * t2;
    return (h00 * p0 + h10 * m0 + h01 * p.peak) / L;
}

static float3 HdrPrimaries(float3 v, constant HdrScRGBTransfer& p)
{
    return float3(dot(p.m[0].xyz, v), dot(p.m[1].xyz, v), dot(p.m[2].xyz, v));
}

// hdr::EncodeScRGBFp16() a pixel a thread: the corrected RGBA8 frame to linear scRGB.
kernel void
hdrEncodeScRGB(texture2d<float, access::read> src [[ texture(0) ]],
               texture2d<float, access::write> dst [[ texture(1) ]],
               constant HdrScRGBTransfer& p [[ buffer(0) ]],
               uint2 gid [[ thread_position_in_grid ]])
{
    if (gid.x >= dst.get_width() || gid.y >= dst.get_height())
        return;

    const uint3 c = uint3(rint(src.read(gid).rgb * 255.0f));

    // Dithered linearize, then the target primaries.
    const float d = (kBayer8[(gid.y & 7) * 8 + (gid.x & 7)] + 0.5f) * (1.0f / 64.0f);
    const float3 lin = HdrPrimaries(float3(HdrDitherLin(c.r, d, p),
                                           HdrDitherLin(c.g, d, p),
                                           HdrDitherLin(c.b, d, p)), p);

    // The scale from the undithered pixel, as PixelScale().
    const float3 un = HdrPrimaries(float3(p.lin[c.r], p.lin[c.g], p.lin[c.b]), p);
    const float s = HdrLumScale(dot(p.luma.xyz, un), max(un.r, max(un.g, un.b)), p);

    dst.write(float4(lin * s / p.white, 1.0f), gid);
}
