/**     Code adapted from Exult source code by Forgotten
 **	Scale.cc - Trying to scale with bilinear interpolation.
 **
 **	Written: 6/14/00 - JSF
 **/

// 2x bilinear and "bilinear plus", 16 and 32 bits per pixel.
//
// Every source pixel a becomes a 2x2 quad from a, its right neighbour b, its
// lower neighbour c and the diagonal d (the last column and row repeat
// themselves):
//
//     a            (a + b) / 2
//     (a + c) / 2  (a + b + c + d) / 4
//
// "Plus" replaces the top-left pixel with (10a + 2b + 2c) / 16, a slight blur.
//
// 16-bit pixels are handled on their 5-bit channels, laid out by the
// systemRed/Green/BlueShift globals (RGB555 or RGB565). 32-bit pixels are
// averaged byte by byte, which is independent of the byte order and keeps the
// full 8-bit precision; the SSE2 path has always done it this way.
//
// One vector kernel per depth (SSE2 / NEON, see simd128.h) and a scalar
// kernel for the other targets; the test suite checks that the two agree.

#include <cstdint>

#include "../simd128.h"
#include "core/base/system.h"

namespace {

using namespace vbam_simd;

// ---------------------------------------------------------------------------
// 32-bit: byte-wise, rounding averages (SSE2 pavgb semantics)
// ---------------------------------------------------------------------------

VBAM_SIMD_INLINE uint32_t Avg32(uint32_t p, uint32_t q) {
    const uint32_t rb = ((p & 0x00ff00ffu) + (q & 0x00ff00ffu) + 0x00010001u) >> 1;
    const uint32_t ga = ((p >> 8 & 0x00ff00ffu) + (q >> 8 & 0x00ff00ffu) + 0x00010001u) >> 1;
    return (rb & 0x00ff00ffu) | ((ga & 0x00ff00ffu) << 8);
}

// (10a + 2b + 2c) / 16 per byte
VBAM_SIMD_INLINE uint32_t Plus32(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const uint32_t v = (10 * ((a >> shift) & 0xff) + 2 * ((b >> shift) & 0xff) + 2 * ((c >> shift) & 0xff)) >> 4;
        out |= v << shift;
    }
    return out;
}

template <bool kPlus>
void Bilinear32Scalar(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                      int height) {
    for (int y = 0; y < height; y++) {
        const uint32_t* src0 = reinterpret_cast<const uint32_t*>(srcPtr + y * srcPitch);
        const uint32_t* src1 = (y + 1 < height) ? reinterpret_cast<const uint32_t*>(srcPtr + (y + 1) * srcPitch) : src0;
        uint32_t* dst0 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2) * dstPitch);
        uint32_t* dst1 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2 + 1) * dstPitch);
        for (int x = 0; x < width; x++) {
            const uint32_t a = src0[x];
            const uint32_t b = (x + 1 < width) ? src0[x + 1] : a;
            const uint32_t c = src1[x];
            const uint32_t d = (x + 1 < width) ? src1[x + 1] : c;
            const uint32_t ab = Avg32(a, b);
            dst0[x * 2] = kPlus ? Plus32(a, b, c) : a;
            dst0[x * 2 + 1] = ab;
            dst1[x * 2] = Avg32(a, c);
            dst1[x * 2 + 1] = Avg32(ab, Avg32(c, d));
        }
    }
}

// (10a + 2b + 2c) / 16 per byte, on 16-bit lanes
VBAM_SIMD_INLINE V Plus32Vec(V a, V b, V c) {
    const V z = zero();
    const V ten = set1_u16(10), two = set1_u16(2);
    auto half = [&](V al, V bl, V cl) {
        const V sum = add_u16(add_u16(mullo_u16(al, ten), mullo_u16(bl, two)), mullo_u16(cl, two));
        return srl_u16<4>(sum);
    };
    const V lo = half(unpacklo_u8(a, z), unpacklo_u8(b, z), unpacklo_u8(c, z));
    const V hi = half(unpackhi_u8(a, z), unpackhi_u8(b, z), unpackhi_u8(c, z));
    return packus_s16_u8(lo, hi);
}

template <bool kPlus>
void Bilinear32Vector(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                      int height) {
    for (int y = 0; y < height; y++) {
        const uint32_t* src0 = reinterpret_cast<const uint32_t*>(srcPtr + y * srcPitch);
        const uint32_t* src1 = (y + 1 < height) ? reinterpret_cast<const uint32_t*>(srcPtr + (y + 1) * srcPitch) : src0;
        uint32_t* dst0 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2) * dstPitch);
        uint32_t* dst1 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        for (; x + 4 <= width; x += 4) {
            const V a = load(src0 + x);
            const V c = load(src1 + x);
            V b, d;
            if (x + 4 < width) {
                b = load(src0 + x + 1);
                d = load(src1 + x + 1);
            } else {
                // last group: shift down one pixel and repeat the last one
                b = or_(bsrl<4>(a), bsll<12>(dup_u32<3>(a)));
                d = or_(bsrl<4>(c), bsll<12>(dup_u32<3>(c)));
            }
            const V ab = avg_u8(a, b);
            const V ac = avg_u8(a, c);
            const V abcd = avg_u8(ab, avg_u8(c, d));
            const V tl = kPlus ? Plus32Vec(a, b, c) : a;
            store2_u32(dst0 + x * 2, tl, ab);
            store2_u32(dst1 + x * 2, ac, abcd);
        }
        for (; x < width; x++) {
            const uint32_t a = src0[x];
            const uint32_t b = (x + 1 < width) ? src0[x + 1] : a;
            const uint32_t c = src1[x];
            const uint32_t d = (x + 1 < width) ? src1[x + 1] : c;
            const uint32_t ab = Avg32(a, b);
            dst0[x * 2] = kPlus ? Plus32(a, b, c) : a;
            dst0[x * 2 + 1] = ab;
            dst1[x * 2] = Avg32(a, c);
            dst1[x * 2 + 1] = Avg32(ab, Avg32(c, d));
        }
    }
}

// ---------------------------------------------------------------------------
// 16-bit: three 5-bit channels at the system shifts
// ---------------------------------------------------------------------------

struct Channels16 {
    int r, g, b;
};

VBAM_SIMD_INLINE Channels16 Unpack16(uint16_t p) {
    return {(p >> systemRedShift) & 0x1f, (p >> systemGreenShift) & 0x1f, (p >> systemBlueShift) & 0x1f};
}

VBAM_SIMD_INLINE uint16_t Pack16(int r, int g, int b) {
    return static_cast<uint16_t>((r << systemRedShift) | (g << systemGreenShift) | (b << systemBlueShift));
}

template <bool kPlus>
void Bilinear16Scalar(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                      int height) {
    for (int y = 0; y < height; y++) {
        const uint16_t* src0 = reinterpret_cast<const uint16_t*>(srcPtr + y * srcPitch);
        const uint16_t* src1 = (y + 1 < height) ? reinterpret_cast<const uint16_t*>(srcPtr + (y + 1) * srcPitch) : src0;
        uint16_t* dst0 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2) * dstPitch);
        uint16_t* dst1 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2 + 1) * dstPitch);
        for (int x = 0; x < width; x++) {
            const Channels16 a = Unpack16(src0[x]);
            const Channels16 b = Unpack16((x + 1 < width) ? src0[x + 1] : src0[x]);
            const Channels16 c = Unpack16(src1[x]);
            const Channels16 d = Unpack16((x + 1 < width) ? src1[x + 1] : src1[x]);
            dst0[x * 2] = kPlus ? Pack16((5 * a.r + b.r + c.r) >> 3, (5 * a.g + b.g + c.g) >> 3, (5 * a.b + b.b + c.b) >> 3)
                                : src0[x];
            dst0[x * 2 + 1] = Pack16((a.r + b.r) >> 1, (a.g + b.g) >> 1, (a.b + b.b) >> 1);
            dst1[x * 2] = Pack16((a.r + c.r) >> 1, (a.g + c.g) >> 1, (a.b + c.b) >> 1);
            dst1[x * 2 + 1] = Pack16((a.r + b.r + c.r + d.r) >> 2, (a.g + b.g + c.g + d.g) >> 2,
                                     (a.b + b.b + c.b + d.b) >> 2);
        }
    }
}

struct Channels16Vec {
    V r, g, b;
};

VBAM_SIMD_INLINE Channels16Vec Unpack16Vec(V p) {
    const V m = set1_u16(0x1f);
    return {and_(srl_u16_var(p, systemRedShift), m), and_(srl_u16_var(p, systemGreenShift), m),
            and_(srl_u16_var(p, systemBlueShift), m)};
}

VBAM_SIMD_INLINE V Pack16Vec(V r, V g, V b) {
    return or_(or_(sll_u16_var(r, systemRedShift), sll_u16_var(g, systemGreenShift)), sll_u16_var(b, systemBlueShift));
}

template <bool kPlus>
void Bilinear16Vector(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                      int height) {
    const V five = set1_u16(5);
    for (int y = 0; y < height; y++) {
        const uint16_t* src0 = reinterpret_cast<const uint16_t*>(srcPtr + y * srcPitch);
        const uint16_t* src1 = (y + 1 < height) ? reinterpret_cast<const uint16_t*>(srcPtr + (y + 1) * srcPitch) : src0;
        uint16_t* dst0 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2) * dstPitch);
        uint16_t* dst1 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        for (; x + 8 <= width; x += 8) {
            const V pa = load(src0 + x);
            const V pc = load(src1 + x);
            V pb, pd;
            if (x + 8 < width) {
                pb = load(src0 + x + 1);
                pd = load(src1 + x + 1);
            } else {
                pb = or_(bsrl<2>(pa), bsll<14>(set1_u16(src0[width - 1])));
                pd = or_(bsrl<2>(pc), bsll<14>(set1_u16(src1[width - 1])));
            }
            const Channels16Vec a = Unpack16Vec(pa), b = Unpack16Vec(pb), c = Unpack16Vec(pc), d = Unpack16Vec(pd);

            auto avg = [](V p, V q) { return srl_u16<1>(add_u16(p, q)); };
            auto avg4 = [](V p, V q, V r, V s) { return srl_u16<2>(add_u16(add_u16(p, q), add_u16(r, s))); };
            auto plus = [&](V p, V q, V r) { return srl_u16<3>(add_u16(add_u16(mullo_u16(p, five), q), r)); };

            const V tl = kPlus ? Pack16Vec(plus(a.r, b.r, c.r), plus(a.g, b.g, c.g), plus(a.b, b.b, c.b)) : pa;
            const V ab = Pack16Vec(avg(a.r, b.r), avg(a.g, b.g), avg(a.b, b.b));
            const V ac = Pack16Vec(avg(a.r, c.r), avg(a.g, c.g), avg(a.b, c.b));
            const V abcd = Pack16Vec(avg4(a.r, b.r, c.r, d.r), avg4(a.g, b.g, c.g, d.g), avg4(a.b, b.b, c.b, d.b));
            store2_u16(dst0 + x * 2, tl, ab);
            store2_u16(dst1 + x * 2, ac, abcd);
        }
        for (; x < width; x++) {
            const Channels16 a = Unpack16(src0[x]);
            const Channels16 b = Unpack16((x + 1 < width) ? src0[x + 1] : src0[x]);
            const Channels16 c = Unpack16(src1[x]);
            const Channels16 d = Unpack16((x + 1 < width) ? src1[x + 1] : src1[x]);
            dst0[x * 2] = kPlus ? Pack16((5 * a.r + b.r + c.r) >> 3, (5 * a.g + b.g + c.g) >> 3, (5 * a.b + b.b + c.b) >> 3)
                                : src0[x];
            dst0[x * 2 + 1] = Pack16((a.r + b.r) >> 1, (a.g + b.g) >> 1, (a.b + b.b) >> 1);
            dst1[x * 2] = Pack16((a.r + c.r) >> 1, (a.g + c.g) >> 1, (a.b + c.b) >> 1);
            dst1[x * 2 + 1] = Pack16((a.r + b.r + c.r + d.r) >> 2, (a.g + b.g + c.g + d.g) >> 2,
                                     (a.b + b.b + c.b + d.b) >> 2);
        }
    }
}

}  // namespace

void Bilinear(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
              int width, int height) {
    if (Enabled())
        Bilinear16Vector<false>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        Bilinear16Scalar<false>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void BilinearPlus(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                  int width, int height) {
    if (Enabled())
        Bilinear16Vector<true>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        Bilinear16Scalar<true>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Bilinear32(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                int width, int height) {
    if (Enabled())
        Bilinear32Vector<false>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        Bilinear32Scalar<false>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void BilinearPlus32(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                    int width, int height) {
    if (Enabled())
        Bilinear32Vector<true>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        Bilinear32Scalar<true>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}
