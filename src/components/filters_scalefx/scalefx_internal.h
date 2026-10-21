// ScaleFX internals shared by the passes: the float4-per-pixel buffer layout,
// clamped access to it, and the vector layer (SSE2 / NEON / scalar, see
// components/filters/simd128.h).
//
// Every pass has a vector kernel that handles four consecutive pixels per lane
// group and a scalar kernel with the same arithmetic in the same order, so the
// two produce identical bits; the test suite checks that.

#ifndef SCALEFX_INTERNAL_H
#define SCALEFX_INTERNAL_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "components/filters/simd128.h"

namespace scalefx {

using namespace vbam_simd;

// Helper struct for float4 values
struct Float4 {
    float x, y, z, w;
};

// Helper struct for bool4 values
struct Bool4 {
    bool x, y, z, w;
};

// Get data at position with boundary clamping
static inline Float4 GetData(const float* buf, int width, int height, int px, int py) {
    px = (px < 0) ? 0 : (px >= width) ? width - 1 : px;
    py = (py < 0) ? 0 : (py >= height) ? height - 1 : py;
    int idx = (py * width + px) * 4;
    return {buf[idx + 0], buf[idx + 1], buf[idx + 2], buf[idx + 3]};
}

// The four fields of a float4 pixel, for four consecutive pixels: one vector
// per field, lane i = pixel i
struct Float4Lanes {
    VF x, y, z, w;
};

// Four consecutive float4 pixels (16 floats) transposed into field lanes
VBAM_SIMD_INLINE Float4Lanes LoadLanes(const float* p) {
    const V p0 = fbits(fload(p)), p1 = fbits(fload(p + 4)), p2 = fbits(fload(p + 8)), p3 = fbits(fload(p + 12));
    const V t0 = unpacklo_u32(p0, p1);  // x0 x1 y0 y1
    const V t1 = unpackhi_u32(p0, p1);  // z0 z1 w0 w1
    const V t2 = unpacklo_u32(p2, p3);  // x2 x3 y2 y3
    const V t3 = unpackhi_u32(p2, p3);  // z2 z3 w2 w3
    return {ffrombits(unpacklo_u64(t0, t2)), ffrombits(unpackhi_u64(t0, t2)), ffrombits(unpacklo_u64(t1, t3)),
            ffrombits(unpackhi_u64(t1, t3))};
}

// Field lanes of four pixels stored back as four float4 pixels
VBAM_SIMD_INLINE void StoreLanes(float* p, VF x, VF y, VF z, VF w) {
    const V t0 = unpacklo_u32(fbits(x), fbits(y));  // x0 y0 x1 y1
    const V t1 = unpackhi_u32(fbits(x), fbits(y));  // x2 y2 x3 y3
    const V t2 = unpacklo_u32(fbits(z), fbits(w));  // z0 w0 z1 w1
    const V t3 = unpackhi_u32(fbits(z), fbits(w));  // z2 w2 z3 w3
    fstore(p, ffrombits(unpacklo_u64(t0, t2)));
    fstore(p + 4, ffrombits(unpackhi_u64(t0, t2)));
    fstore(p + 8, ffrombits(unpacklo_u64(t1, t3)));
    fstore(p + 12, ffrombits(unpackhi_u64(t1, t3)));
}

// Rows of a float4 buffer with the edge pixel repeated kPad times on either
// side, kept in a ring so a row is copied once as the output row advances.
// Row(y) clamps y to the image.
template <int kPad, int kRows>
class PaddedF4Rows {
public:
    static constexpr int kPadRight = kPad + 4;  // the last lane group reads up to 3 pixels further

    PaddedF4Rows(const float* buf, int width, int height)
        : buf_(buf), width_(width), height_(height), rowLen_((kPad + width + kPadRight) * 4), ring_(kRows * rowLen_) {
        for (int i = 0; i < kRows; i++)
            slot_[i] = -1000000;
    }

    // float4 pixel column 0 of source row y (clamped)
    const float* Row(int y) {
        y = y < 0 ? 0 : y >= height_ ? height_ - 1 : y;
        const int slot = ((y % kRows) + kRows) % kRows;
        float* row = ring_.data() + slot * rowLen_ + kPad * 4;
        if (slot_[slot] != y) {
            slot_[slot] = y;
            const float* src = buf_ + static_cast<size_t>(y) * width_ * 4;
            std::memcpy(row, src, width_ * 16);
            for (int i = 1; i <= kPad; i++)
                std::memcpy(row - 4 * i, src, 16);
            for (int i = 0; i < kPadRight; i++)
                std::memcpy(row + 4 * (width_ + i), src + 4 * (width_ - 1), 16);
        }
        return row;
    }

private:
    const float* buf_;
    int width_, height_, rowLen_;
    std::vector<float> ring_;
    int slot_[kRows];
};

// mask -> 1.0f / 0.0f
VBAM_SIMD_INLINE VF MaskToFloat(V m) { return fand(fset1(1.0f), m); }

// 1 where the lanes are non-zero
VBAM_SIMD_INLINE V NonZero(V a) { return not_(cmpeq_u32(a, zero())); }

}  // namespace scalefx

#endif  // SCALEFX_INTERNAL_H
