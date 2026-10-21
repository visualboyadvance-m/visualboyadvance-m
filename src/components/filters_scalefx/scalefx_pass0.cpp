// ScaleFX Pass 0: Metric Data Preparation
// Calculates CIE-weighted color distance between center pixel and neighbors
// Output: float4 per pixel containing distances (E,A), (E,B), (E,C), (E,F)
//
// Based on ScaleFX by Sp00kyFox, 2017-03-01
// CPU implementation: four pixels per lane group (SSE2 / NEON) or scalar

#include <cmath>
#include <cstdint>

#include "scalefx_internal.h"

namespace scalefx {

namespace {

constexpr float kInv255 = 1.0f / 255.0f;
constexpr float kInv3 = 1.0f / 3.0f;

// Get pixel with boundary clamping (scalar)
inline uint32_t GetPixelClamped(const uint8_t* src, int srcPitch, int width, int height, int x, int y) {
    x = (x < 0) ? 0 : (x >= width) ? width - 1 : x;
    y = (y < 0) ? 0 : (y >= height) ? height - 1 : y;
    const uint32_t* row = reinterpret_cast<const uint32_t*>(src + y * srcPitch);
    return row[x];
}

// CIE color distance (scalar); the operations in the order of the vector kernel
inline float ColorDistScalar(uint32_t c1, uint32_t c2) {
    const float r1 = static_cast<float>((c1 >> 16) & 0xFF) * kInv255;
    const float g1 = static_cast<float>((c1 >> 8) & 0xFF) * kInv255;
    const float b1 = static_cast<float>(c1 & 0xFF) * kInv255;

    const float r2 = static_cast<float>((c2 >> 16) & 0xFF) * kInv255;
    const float g2 = static_cast<float>((c2 >> 8) & 0xFF) * kInv255;
    const float b2 = static_cast<float>(c2 & 0xFF) * kInv255;

    const float r = 0.5f * (r1 + r2);
    const float dr = r1 - r2;
    const float dg = g1 - g2;
    const float db = b1 - b2;

    const float cr = 2.0f + r;
    const float cb = 3.0f - r;

    const float sum = (cr * (dr * dr) + 4.0f * (dg * dg)) + cb * (db * db);
    return std::sqrt(sum) * kInv3;
}

struct RgbLanes {
    VF r, g, b;
};

VBAM_SIMD_INLINE RgbLanes UnpackRgb(V p) {
    const V byte = set1_u32(0xff);
    const VF inv255 = fset1(kInv255);
    return {fmul(cvt_s32_f(and_(srl_u32<16>(p), byte)), inv255), fmul(cvt_s32_f(and_(srl_u32<8>(p), byte)), inv255),
            fmul(cvt_s32_f(and_(p, byte)), inv255)};
}

VBAM_SIMD_INLINE VF ColorDistVec(const RgbLanes& e, V neighbour) {
    const RgbLanes n = UnpackRgb(neighbour);
    const VF r = fmul(fset1(0.5f), fadd(e.r, n.r));
    const VF dr = fsub(e.r, n.r);
    const VF dg = fsub(e.g, n.g);
    const VF db = fsub(e.b, n.b);
    const VF cr = fadd(fset1(2.0f), r);
    const VF cb = fsub(fset1(3.0f), r);
    const VF sum = fadd(fadd(fmul(cr, fmul(dr, dr)), fmul(fset1(4.0f), fmul(dg, dg))), fmul(cb, fmul(db, db)));
    return fmul(fsqrt(sum), fset1(kInv3));
}

void Pass0Vector(const uint32_t* src, int srcPitch, float* dst, int width, int height) {
    const uint8_t* srcBytes = reinterpret_cast<const uint8_t*>(src);
    // a copy of the two rows with the edge pixel repeated: every neighbour is an unaligned load
    const int rowLen = width + 8;
    std::vector<uint32_t> rows(2 * rowLen);
    auto pad = [&](int y, uint32_t* row) {
        y = y < 0 ? 0 : y;
        const uint32_t* s = reinterpret_cast<const uint32_t*>(srcBytes + y * srcPitch);
        std::memcpy(row + 1, s, width * 4);
        row[0] = s[0];
        for (int i = 0; i < 7; i++)
            row[1 + width + i] = s[width - 1];
    };

    for (int y = 0; y < height; y++) {
        uint32_t* rowE = rows.data() + 1;
        uint32_t* rowB = rows.data() + rowLen + 1;
        pad(y, rows.data());
        pad(y - 1, rows.data() + rowLen);

        int x = 0;
        for (; x + 4 <= width; x += 4) {
            const RgbLanes e = UnpackRgb(load(rowE + x));
            const VF ea = ColorDistVec(e, load(rowB + x - 1));
            const VF eb = ColorDistVec(e, load(rowB + x));
            const VF ec = ColorDistVec(e, load(rowB + x + 1));
            const VF ef = ColorDistVec(e, load(rowE + x + 1));
            StoreLanes(dst + (static_cast<size_t>(y) * width + x) * 4, ea, eb, ec, ef);
        }
        for (; x < width; x++) {
            const uint32_t E = rowE[x];
            const size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            dst[idx + 0] = ColorDistScalar(E, rowB[x - 1]);
            dst[idx + 1] = ColorDistScalar(E, rowB[x]);
            dst[idx + 2] = ColorDistScalar(E, rowB[x + 1]);
            dst[idx + 3] = ColorDistScalar(E, rowE[x + 1]);
        }
    }
}

void Pass0Scalar(const uint32_t* src, int srcPitch, float* dst, int width, int height) {
    const uint8_t* srcBytes = reinterpret_cast<const uint8_t*>(src);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t E = GetPixelClamped(srcBytes, srcPitch, width, height, x, y);
            uint32_t A = GetPixelClamped(srcBytes, srcPitch, width, height, x - 1, y - 1);
            uint32_t B = GetPixelClamped(srcBytes, srcPitch, width, height, x, y - 1);
            uint32_t C = GetPixelClamped(srcBytes, srcPitch, width, height, x + 1, y - 1);
            uint32_t F = GetPixelClamped(srcBytes, srcPitch, width, height, x + 1, y);

            const size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            dst[idx + 0] = ColorDistScalar(E, A);
            dst[idx + 1] = ColorDistScalar(E, B);
            dst[idx + 2] = ColorDistScalar(E, C);
            dst[idx + 3] = ColorDistScalar(E, F);
        }
    }
}

}  // namespace

void Pass0(const uint32_t* src, int srcPitch, float* dst, int width, int height) {
    if (Enabled())
        Pass0Vector(src, srcPitch, dst, width, height);
    else
        Pass0Scalar(src, srcPitch, dst, width, height);
}

}  // namespace scalefx
