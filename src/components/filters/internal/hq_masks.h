// Neighbourhood difference masks for the hq2x / lq2x / hq3x / hq4x family.
//
// These filters pick one of 256 interpolation patterns per pixel from an
// 8-bit mask that says which of the eight neighbours differ "enough" from the
// centre pixel. The pattern switch itself stays scalar; the masks of a whole
// row are computed here, a lane group of pixels at a time (SSE2 / NEON, see
// simd128.h), with scalar versions of the same metrics for the other targets.
//
// Three metrics, matching the scalar code they replace exactly:
//   InterpDiffMasks  hq2x: the Y/U/V limits of interp.h (interp_16_diff / interp_32_diff)
//   NotEqualMasks    lq2x: plain inequality
//   YuvPatternMasks  hq3x / hq4x: RGBtoYUV() + Diff() of hq/c/hq_shared.h,
//                    including its abs_32() quirk (bit 31 cleared, no sign handling)
//
// Bit k of a mask is neighbour k in reading order, the centre skipped:
//   0 1 2
//   3 . 4
//   5 6 7

#ifndef VBAM_COMPONENTS_FILTERS_INTERNAL_HQ_MASKS_H_
#define VBAM_COMPONENTS_FILTERS_INTERNAL_HQ_MASKS_H_

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../simd128.h"

namespace hqmask {

using namespace vbam_simd;

// Per-thread scratch that keeps its capacity between rows (a row is processed
// many times per frame; allocating per row showed in the profile).
template <typename T, int kSlot>
inline std::vector<T>& Scratch(size_t n) {
    thread_local std::vector<T> buf;
    if (buf.size() < n)
        buf.resize(n);
    return buf;
}

// Three source rows copied with the edge pixel repeated once on the left and
// lane slack on the right, so every neighbour is an unaligned load.
template <typename Pixel>
class Padded3 {
public:
    static constexpr int kPadLeft = 4;
    static constexpr int kPadRight = 16;

    Padded3(const Pixel* s0, const Pixel* s1, const Pixel* s2, int count)
        : len_(kPadLeft + count + kPadRight), buf_(Scratch<Pixel, 0>(3 * len_)) {
        Fill(0, s0, count);
        Fill(1, s1, count);
        Fill(2, s2, count);
    }

    const Pixel* Row(int k) const { return buf_.data() + k * len_ + kPadLeft; }

private:
    void Fill(int k, const Pixel* src, int count) {
        Pixel* row = buf_.data() + k * len_ + kPadLeft;
        std::memcpy(row, src, count * sizeof(Pixel));
        std::fill(row - kPadLeft, row, src[0]);
        std::fill(row + count, row + count + kPadRight, src[count - 1]);
    }

    int len_;
    std::vector<Pixel>& buf_;
};

// The eight neighbours of the pixel at x in the three padded rows, in mask bit order
template <typename Pixel>
VBAM_SIMD_INLINE void Neighbours(const Pixel* r0, const Pixel* r1, const Pixel* r2, int x, const Pixel* (&n)[8]) {
    n[0] = r0 + x - 1;
    n[1] = r0 + x;
    n[2] = r0 + x + 1;
    n[3] = r1 + x - 1;
    n[4] = r1 + x + 1;
    n[5] = r2 + x - 1;
    n[6] = r2 + x;
    n[7] = r2 + x + 1;
}

// -------------------------------------------------------------------------
// hq2x metric (interp.h)
// -------------------------------------------------------------------------

constexpr int kInterpYLimit = 0x30 * 4;
constexpr int kInterpULimit = 0x07 * 4;
constexpr int kInterpVLimit = 0x06 * 8;

inline bool InterpDiff32(uint32_t p1, uint32_t p2) {
    if ((p1 & 0xF8F8F8) == (p2 & 0xF8F8F8))
        return false;
    const int b = static_cast<int>((p1 & 0xFF) - (p2 & 0xFF));
    const int g = static_cast<int>((p1 & 0xFF00) - (p2 & 0xFF00)) >> 8;
    const int r = static_cast<int>((p1 & 0xFF0000) - (p2 & 0xFF0000)) >> 16;
    const int y = r + g + b;
    const int u = r - b;
    const int v = -r + 2 * g - b;
    return y < -kInterpYLimit || y > kInterpYLimit || u < -kInterpULimit || u > kInterpULimit ||
           v < -kInterpVLimit || v > kInterpVLimit;
}

inline bool InterpDiff16(uint16_t p1, uint16_t p2, bool rgb565) {
    if (p1 == p2)
        return false;
    int r, g, b;
    if (rgb565) {
        b = static_cast<int>((p1 & 0x1F) - (p2 & 0x1F)) << 3;
        g = static_cast<int>((p1 & 0x7E0) - (p2 & 0x7E0)) >> 3;
        r = static_cast<int>((p1 & 0xF800) - (p2 & 0xF800)) >> 8;
    } else {
        b = static_cast<int>((p1 & 0x1F) - (p2 & 0x1F)) << 3;
        g = static_cast<int>((p1 & 0x3E0) - (p2 & 0x3E0)) >> 2;
        r = static_cast<int>((p1 & 0x7C00) - (p2 & 0x7C00)) >> 7;
    }
    const int y = r + g + b;
    const int u = r - b;
    const int v = -r + 2 * g - b;
    return y < -kInterpYLimit || y > kInterpYLimit || u < -kInterpULimit || u > kInterpULimit ||
           v < -kInterpVLimit || v > kInterpVLimit;
}

// |v| > limit per lane, v as signed 32-bit lanes
VBAM_SIMD_INLINE V OutsideS32(V v, int limit) {
    return or_(cmpgt_s32(v, set1_u32(static_cast<uint32_t>(limit))),
               cmpgt_s32(set1_u32(static_cast<uint32_t>(-limit)), v));
}
VBAM_SIMD_INLINE V OutsideS16(V v, int limit) {
    return or_(cmpgt_s16(v, set1_u16(static_cast<uint16_t>(limit))),
               cmpgt_s16(set1_u16(static_cast<uint16_t>(-limit)), v));
}

// masks[0 .. count) for the 32-bit hq2x metric
inline void InterpDiffMasks32(const uint32_t* s0, const uint32_t* s1, const uint32_t* s2, int count, uint8_t* masks) {
    if (!Enabled()) {
        for (int x = 0; x < count; x++) {
            const uint32_t c = s1[x];
            const int xm = x > 0 ? x - 1 : 0, xp = x < count - 1 ? x + 1 : x;
            const uint32_t n[8] = {s0[xm], s0[x], s0[xp], s1[xm], s1[xp], s2[xm], s2[x], s2[xp]};
            uint8_t m = 0;
            for (int k = 0; k < 8; k++)
                if (InterpDiff32(n[k], c))
                    m |= static_cast<uint8_t>(1 << k);
            masks[x] = m;
        }
        return;
    }

    Padded3<uint32_t> rows(s0, s1, s2, count);
    const V byte = set1_u32(0xff);
    const V keep = set1_u32(0xF8F8F8);
    for (int x = 0; x < count; x += 4) {
        const uint32_t* n[8];
        Neighbours(rows.Row(0), rows.Row(1), rows.Row(2), x, n);
        const V c = load(rows.Row(1) + x);
        const V cr = and_(srl_u32<16>(c), byte), cg = and_(srl_u32<8>(c), byte), cb = and_(c, byte);
        const V cKeep = and_(c, keep);
        V bits = zero();
        for (int k = 0; k < 8; k++) {
            const V p = load(n[k]);
            const V same = cmpeq_u32(and_(p, keep), cKeep);
            if (all_ones(same))
                continue;
            const V r = sub_u32(and_(srl_u32<16>(p), byte), cr);
            const V g = sub_u32(and_(srl_u32<8>(p), byte), cg);
            const V b = sub_u32(and_(p, byte), cb);
            const V y = add_u32(add_u32(r, g), b);
            const V u = sub_u32(r, b);
            const V v = sub_u32(sub_u32(add_u32(g, g), r), b);
            V diff = or_(or_(OutsideS32(y, kInterpYLimit), OutsideS32(u, kInterpULimit)), OutsideS32(v, kInterpVLimit));
            diff = andnot(same, diff);
            bits = or_(bits, and_(diff, set1_u32(1u << k)));
        }
        // the four lane masks as four bytes
        const V packed = packus_s16_u8(pack_u32_u16(bits, zero()), zero());
        if (x + 4 <= count) {
            store32(masks + x, packed);
        } else {
            uint8_t tmp[16];
            store(tmp, packed);
            std::memcpy(masks + x, tmp, count - x);
        }
    }
}

// masks[0 .. count) for the 16-bit hq2x metric
inline void InterpDiffMasks16(const uint16_t* s0, const uint16_t* s1, const uint16_t* s2, int count, bool rgb565,
                              uint8_t* masks) {
    if (!Enabled()) {
        for (int x = 0; x < count; x++) {
            const uint16_t c = s1[x];
            const int xm = x > 0 ? x - 1 : 0, xp = x < count - 1 ? x + 1 : x;
            const uint16_t n[8] = {s0[xm], s0[x], s0[xp], s1[xm], s1[xp], s2[xm], s2[x], s2[xp]};
            uint8_t m = 0;
            for (int k = 0; k < 8; k++)
                if (InterpDiff16(n[k], c, rgb565))
                    m |= static_cast<uint8_t>(1 << k);
            masks[x] = m;
        }
        return;
    }

    Padded3<uint16_t> rows(s0, s1, s2, count);
    const V m5 = set1_u16(0x1f);
    const V gmask = set1_u16(rgb565 ? 0x3f : 0x1f);
    // channel differences scaled as interp_16_diff does: b * 8, g * 4 (565) or * 8 (555), r * 8
    auto channels = [&](V p, V& r, V& g, V& b) {
        b = sll_u16<3>(and_(p, m5));
        const V g0 = and_(srl_u16<5>(p), gmask);
        g = rgb565 ? sll_u16<2>(g0) : sll_u16<3>(g0);
        r = sll_u16<3>(and_(rgb565 ? srl_u16<11>(p) : srl_u16<10>(p), m5));
    };
    for (int x = 0; x < count; x += 8) {
        const uint16_t* n[8];
        Neighbours(rows.Row(0), rows.Row(1), rows.Row(2), x, n);
        const V c = load(rows.Row(1) + x);
        V cr, cg, cb;
        channels(c, cr, cg, cb);
        V bits = zero();
        for (int k = 0; k < 8; k++) {
            const V p = load(n[k]);
            const V same = cmpeq_u16(p, c);
            if (all_ones(same))
                continue;
            V pr, pg, pb;
            channels(p, pr, pg, pb);
            const V r = sub_u16(pr, cr), g = sub_u16(pg, cg), b = sub_u16(pb, cb);
            const V y = add_u16(add_u16(r, g), b);
            const V u = sub_u16(r, b);
            const V v = sub_u16(sub_u16(add_u16(g, g), r), b);
            V diff = or_(or_(OutsideS16(y, kInterpYLimit), OutsideS16(u, kInterpULimit)), OutsideS16(v, kInterpVLimit));
            diff = andnot(same, diff);
            bits = or_(bits, and_(diff, set1_u16(static_cast<uint16_t>(1 << k))));
        }
        const V packed = packus_s16_u8(bits, zero());
        if (x + 8 <= count) {
            store64(masks + x, packed);
        } else {
            uint8_t tmp[16];
            store(tmp, packed);
            std::memcpy(masks + x, tmp, count - x);
        }
    }
}

// -------------------------------------------------------------------------
// lq2x metric: plain inequality
// -------------------------------------------------------------------------

template <typename Pixel>
inline void NotEqualMasks(const Pixel* s0, const Pixel* s1, const Pixel* s2, int count, uint8_t* masks) {
    if (!Enabled()) {
        for (int x = 0; x < count; x++) {
            const Pixel c = s1[x];
            const int xm = x > 0 ? x - 1 : 0, xp = x < count - 1 ? x + 1 : x;
            const Pixel n[8] = {s0[xm], s0[x], s0[xp], s1[xm], s1[xp], s2[xm], s2[x], s2[xp]};
            uint8_t m = 0;
            for (int k = 0; k < 8; k++)
                if (n[k] != c)
                    m |= static_cast<uint8_t>(1 << k);
            masks[x] = m;
        }
        return;
    }

    Padded3<Pixel> rows(s0, s1, s2, count);
    constexpr int kLanes = 16 / sizeof(Pixel);
    for (int x = 0; x < count; x += kLanes) {
        const Pixel* n[8];
        Neighbours(rows.Row(0), rows.Row(1), rows.Row(2), x, n);
        const V c = load(rows.Row(1) + x);
        V bits = zero();
        for (int k = 0; k < 8; k++) {
            const V p = load(n[k]);
            if constexpr (sizeof(Pixel) == 4)
                bits = or_(bits, andnot(cmpeq_u32(p, c), set1_u32(1u << k)));
            else
                bits = or_(bits, andnot(cmpeq_u16(p, c), set1_u16(static_cast<uint16_t>(1 << k))));
        }
        V packed;
        if constexpr (sizeof(Pixel) == 4)
            packed = packus_s16_u8(pack_u32_u16(bits, zero()), zero());
        else
            packed = packus_s16_u8(bits, zero());
        if (x + kLanes <= count) {
            if constexpr (sizeof(Pixel) == 4)
                store32(masks + x, packed);
            else
                store64(masks + x, packed);
        } else {
            uint8_t tmp[16];
            store(tmp, packed);
            std::memcpy(masks + x, tmp, count - x);
        }
    }
}

// -------------------------------------------------------------------------
// hq3x / hq4x metric (hq/c/hq_shared.h)
// -------------------------------------------------------------------------

inline uint32_t RgbToYuv32(uint32_t c) {
    const unsigned char b = c & 0xff;
    const unsigned char g = (c & 0xff00) >> 8;
    const unsigned char r = static_cast<unsigned char>(c >> 16);
    return ((r + g + b) << 14) + ((r - b + 512) << 4) + ((2 * g - r - b) >> 3) + 128;
}

inline uint32_t RgbToYuv16(uint16_t c, bool rgb555) {
    unsigned char r, g, b;
    if (rgb555) {
        r = static_cast<unsigned char>((c & 0x7C00) >> 7);
        g = static_cast<unsigned char>((c & 0x03E0) >> 2);
        b = static_cast<unsigned char>((c & 0x001F) << 3);
    } else {
        r = static_cast<unsigned char>((c & 0xF800) >> 8);
        g = static_cast<unsigned char>((c & 0x07E0) >> 3);
        b = static_cast<unsigned char>((c & 0x001F) << 3);
    }
    return ((r + g + b) << 14) + ((r - b + 512) << 4) + ((2 * g - r - b) >> 3) + 128;
}

// hq_shared.h Diff(): fields compared with abs_32() = "& 0x7FFFFFFF"
inline bool YuvDiff(uint32_t yuv1, uint32_t yuv2) {
    if (yuv1 == yuv2)
        return false;
    return (((yuv1 & 0x00FF0000) - (yuv2 & 0x00FF0000)) & 0x7FFFFFFF) > 0x00300000 ||
           (((yuv1 & 0x0000FF00) - (yuv2 & 0x0000FF00)) & 0x7FFFFFFF) > 0x00000700 ||
           (((yuv1 & 0x000000FF) - (yuv2 & 0x000000FF)) & 0x7FFFFFFF) > 0x00000006;
}

// YUV of 4 pixels from their 8-bit channels in u32 lanes
VBAM_SIMD_INLINE V YuvVec(V r, V g, V b) {
    const V y = sll_u32<14>(add_u32(add_u32(r, g), b));
    const V u = sll_u32<4>(add_u32(sub_u32(r, b), set1_u32(512)));
    const V v = sra_s32<3>(sub_u32(sub_u32(add_u32(g, g), r), b));
    return add_u32(add_u32(add_u32(y, u), v), set1_u32(128));
}

VBAM_SIMD_INLINE V YuvDiffVec(V yuv1, V yuv2) {
    const V top = set1_u32(0x7FFFFFFF);
    auto field = [&](uint32_t mask, uint32_t limit) {
        const V d = and_(sub_u32(and_(yuv1, set1_u32(mask)), and_(yuv2, set1_u32(mask))), top);
        return cmpgt_s32(d, set1_u32(limit));
    };
    return or_(or_(field(0x00FF0000, 0x00300000), field(0x0000FF00, 0x00000700)), field(0x000000FF, 0x00000006));
}

// 4 pixels' 8-bit channels as u32 lanes
VBAM_SIMD_INLINE void Channels32(V p, V& r, V& g, V& b) {
    const V byte = set1_u32(0xff);
    r = and_(srl_u32<16>(p), byte);
    g = and_(srl_u32<8>(p), byte);
    b = and_(p, byte);
}

// 4 x 16-bit pixels in the low 64 bits, widened, their channels as RGBtoYUV_16 scales them
VBAM_SIMD_INLINE void Channels16(V p16, bool rgb555, V& r, V& g, V& b) {
    const V p = unpacklo_u16(p16, zero());
    if (rgb555) {
        r = srl_u32<7>(and_(p, set1_u32(0x7C00)));
        g = srl_u32<2>(and_(p, set1_u32(0x03E0)));
    } else {
        r = srl_u32<8>(and_(p, set1_u32(0xF800)));
        g = srl_u32<3>(and_(p, set1_u32(0x07E0)));
    }
    b = sll_u32<3>(and_(p, set1_u32(0x001F)));
}

// The three source rows of an hq3x / hq4x output row, with their YUV values:
// the pattern masks of the row and the neighbour YUV the pattern tables test
// in their cases (Diff(yuv[2], yuv[6]) and the like). Column -1 and column
// count repeat the edge pixel.
template <typename Pixel>
class YuvRows {
public:
    YuvRows(const Pixel* s0, const Pixel* s1, const Pixel* s2, int count, bool rgb555)
        : rows_(s0, s1, s2, count), count_(count), yuvLen_(count + 2), yuv_(Scratch<uint32_t, 1>(3 * yuvLen_)),
          masks_(Scratch<uint8_t, 2>(count)) {
        for (int k = 0; k < 3; k++)
            ComputeYuv(rows_.Row(k) - 1, yuvLen_, rgb555, yuv_.data() + k * yuvLen_);
        ComputeMasks();
    }

    // YUV of row k (0 = above, 1 = centre, 2 = below) at column x, -1 <= x <= count
    uint32_t Yuv(int k, int x) const { return yuv_[k * yuvLen_ + x + 1]; }

    const uint8_t* Masks() const { return masks_.data(); }

private:
    // YUV of n pixels starting at p into out
    static void ComputeYuv(const Pixel* p, int n, bool rgb555, uint32_t* out) {
        int x = 0;
        if (Enabled()) {
            for (; x + 4 <= n; x += 4) {
                V r, g, b;
                if constexpr (sizeof(Pixel) == 4)
                    Channels32(load(p + x), r, g, b);
                else
                    Channels16(load64(p + x), rgb555, r, g, b);
                store(out + x, YuvVec(r, g, b));
            }
        }
        for (; x < n; x++) {
            if constexpr (sizeof(Pixel) == 4)
                out[x] = RgbToYuv32(p[x]);
            else
                out[x] = RgbToYuv16(p[x], rgb555);
        }
    }

    void ComputeMasks() {
        const Pixel* c0 = rows_.Row(1);
        const uint32_t* y0 = yuv_.data() + 1;
        const uint32_t* y1 = y0 + yuvLen_;
        const uint32_t* y2 = y1 + yuvLen_;
        int x = 0;
        if (Enabled()) {
            for (; x + 4 <= count_; x += 4) {
                const Pixel* n[8];
                Neighbours(rows_.Row(0), rows_.Row(1), rows_.Row(2), x, n);
                const uint32_t* ny[8];
                Neighbours(y0, y1, y2, x, ny);
                V c, cyuv = load(y1 + x);
                if constexpr (sizeof(Pixel) == 4)
                    c = load(c0 + x);
                else
                    c = unpacklo_u16(load64(c0 + x), zero());
                V bits = zero();
                for (int k = 0; k < 8; k++) {
                    V p;
                    if constexpr (sizeof(Pixel) == 4)
                        p = load(n[k]);
                    else
                        p = unpacklo_u16(load64(n[k]), zero());
                    const V same = cmpeq_u32(p, c);
                    if (all_ones(same))
                        continue;
                    const V diff = andnot(same, YuvDiffVec(cyuv, load(ny[k])));
                    bits = or_(bits, and_(diff, set1_u32(1u << k)));
                }
                store32(masks_.data() + x, packus_s16_u8(pack_u32_u16(bits, zero()), zero()));
            }
        }
        for (; x < count_; x++) {
            const Pixel c = c0[x];
            const uint32_t yuv = y1[x];
            const Pixel* n[8];
            Neighbours(rows_.Row(0), rows_.Row(1), rows_.Row(2), x, n);
            const uint32_t* ny[8];
            Neighbours(y0, y1, y2, x, ny);
            uint8_t m = 0;
            for (int k = 0; k < 8; k++)
                if (*n[k] != c && YuvDiff(yuv, *ny[k]))
                    m |= static_cast<uint8_t>(1 << k);
            masks_[x] = m;
        }
    }

    Padded3<Pixel> rows_;
    int count_;
    int yuvLen_;
    std::vector<uint32_t>& yuv_;
    std::vector<uint8_t>& masks_;
};

}  // namespace hqmask

#endif  // VBAM_COMPONENTS_FILTERS_INTERNAL_HQ_MASKS_H_
