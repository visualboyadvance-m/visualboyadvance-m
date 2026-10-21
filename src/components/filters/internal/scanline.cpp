// Scanlines (every other output row black) and TV mode (every other output
// row and column darkened to 3/4), 16 and 32 bits per pixel.
//
// One vector kernel per filter and depth (SSE2 / NEON, see simd128.h) and a
// scalar kernel for the other targets; the test suite checks that the two agree.

#include <cstdint>

#include "../simd128.h"

extern int RGB_LOW_BITS_MASK;

namespace {

using namespace vbam_simd;

// Pixel traits: lane width and the scalar operations of each depth
struct Depth32 {
    using Pixel = uint32_t;
    static constexpr int kLanes = 4;
    static void Store2(Pixel* p, V a, V b) { store2_u32(p, a, b); }
    static V Halve(V a, V mask) { return srl_u32<1>(and_(a, mask)); }
    static V Add(V a, V b) { return add_u32(a, b); }
    template <int N> static V ByteShift(V a) { return bsrl<4 * N>(a); }
    template <int N> static V ByteShiftLeft(V a) { return bsll<4 * N>(a); }
    static V Set1(Pixel p) { return set1_u32(p); }
};

struct Depth16 {
    using Pixel = uint16_t;
    static constexpr int kLanes = 8;
    static void Store2(Pixel* p, V a, V b) { store2_u16(p, a, b); }
    static V Halve(V a, V mask) { return srl_u16<1>(and_(a, mask)); }
    static V Add(V a, V b) { return add_u16(a, b); }
    template <int N> static V ByteShift(V a) { return bsrl<2 * N>(a); }
    template <int N> static V ByteShiftLeft(V a) { return bsll<2 * N>(a); }
    static V Set1(Pixel p) { return set1_u16(p); }
};

// mask that clears the low bit of every channel, as a lane constant
template <class D>
VBAM_SIMD_INLINE typename D::Pixel ColorMask() {
    return static_cast<typename D::Pixel>(~RGB_LOW_BITS_MASK);
}

// ---------------------------------------------------------------------------
// Scanlines: row 0 = pixel doubled, row 1 = black
// ---------------------------------------------------------------------------

template <class D>
void ScanlinesScalar(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                     int height) {
    using Pixel = typename D::Pixel;
    for (int y = 0; y < height; y++) {
        const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr + y * srcPitch);
        Pixel* dst0 = reinterpret_cast<Pixel*>(dstPtr + (y * 2) * dstPitch);
        Pixel* dst1 = reinterpret_cast<Pixel*>(dstPtr + (y * 2 + 1) * dstPitch);
        for (int x = 0; x < width; x++) {
            dst0[x * 2] = dst0[x * 2 + 1] = src[x];
            dst1[x * 2] = dst1[x * 2 + 1] = 0;
        }
    }
}

template <class D>
void ScanlinesVector(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                     int height) {
    using Pixel = typename D::Pixel;
    const V z = zero();
    for (int y = 0; y < height; y++) {
        const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr + y * srcPitch);
        Pixel* dst0 = reinterpret_cast<Pixel*>(dstPtr + (y * 2) * dstPitch);
        Pixel* dst1 = reinterpret_cast<Pixel*>(dstPtr + (y * 2 + 1) * dstPitch);
        int x = 0;
        for (; x + D::kLanes <= width; x += D::kLanes) {
            const V p = load(src + x);
            D::Store2(dst0 + x * 2, p, p);
            store(dst1 + x * 2, z);
            store(dst1 + x * 2 + D::kLanes, z);
        }
        for (; x < width; x++) {
            dst0[x * 2] = dst0[x * 2 + 1] = src[x];
            dst1[x * 2] = dst1[x * 2 + 1] = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// TV mode: row 0 = [pixel, average with the right neighbour], row 1 = row 0 at 3/4
// ---------------------------------------------------------------------------

template <typename Pixel>
VBAM_SIMD_INLINE Pixel Darken(Pixel c, Pixel mask) {
    const Pixel half = static_cast<Pixel>((c & mask) >> 1);
    return static_cast<Pixel>(half + ((half & mask) >> 1));
}

template <class D>
void ScanlinesTVScalar(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                       int height) {
    using Pixel = typename D::Pixel;
    const Pixel mask = ColorMask<D>();
    for (int y = 0; y < height; y++) {
        const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr + y * srcPitch);
        Pixel* dst0 = reinterpret_cast<Pixel*>(dstPtr + (y * 2) * dstPitch);
        Pixel* dst1 = reinterpret_cast<Pixel*>(dstPtr + (y * 2 + 1) * dstPitch);
        for (int x = 0; x < width; x++) {
            const Pixel a = src[x];
            const Pixel b = (x + 1 < width) ? src[x + 1] : a;
            const Pixel ab = static_cast<Pixel>(((a & mask) >> 1) + ((b & mask) >> 1));
            dst0[x * 2] = a;
            dst0[x * 2 + 1] = ab;
            dst1[x * 2] = Darken(a, mask);
            dst1[x * 2 + 1] = Darken(ab, mask);
        }
    }
}

template <class D>
void ScanlinesTVVector(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                       int height) {
    using Pixel = typename D::Pixel;
    constexpr int L = D::kLanes;
    const Pixel maskPixel = ColorMask<D>();
    const V mask = D::Set1(maskPixel);

    auto darken = [&](V c) {
        const V half = D::Halve(c, mask);
        return D::Add(half, D::Halve(half, mask));  // channels never carry into each other: their low bits are clear
    };

    for (int y = 0; y < height; y++) {
        const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr + y * srcPitch);
        Pixel* dst0 = reinterpret_cast<Pixel*>(dstPtr + (y * 2) * dstPitch);
        Pixel* dst1 = reinterpret_cast<Pixel*>(dstPtr + (y * 2 + 1) * dstPitch);
        int x = 0;
        for (; x + L <= width; x += L) {
            const V a = load(src + x);
            V b;
            if (x + L < width) {
                b = load(src + x + 1);
            } else {
                // last group: shift down one pixel and repeat the last one
                b = or_(D::template ByteShift<1>(a), D::template ByteShiftLeft<L - 1>(D::Set1(src[width - 1])));
            }
            const V ab = D::Add(D::Halve(a, mask), D::Halve(b, mask));
            D::Store2(dst0 + x * 2, a, ab);
            D::Store2(dst1 + x * 2, darken(a), darken(ab));
        }
        for (; x < width; x++) {
            const Pixel a = src[x];
            const Pixel b = (x + 1 < width) ? src[x + 1] : a;
            const Pixel ab = static_cast<Pixel>(((a & maskPixel) >> 1) + ((b & maskPixel) >> 1));
            dst0[x * 2] = a;
            dst0[x * 2 + 1] = ab;
            dst1[x * 2] = Darken(a, maskPixel);
            dst1[x * 2 + 1] = Darken(ab, maskPixel);
        }
    }
}

}  // namespace

void Scanlines(uint8_t* srcPtr, uint32_t srcPitch, uint8_t*, uint8_t* dstPtr, uint32_t dstPitch, int width,
               int height) {
    if (Enabled())
        ScanlinesVector<Depth16>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        ScanlinesScalar<Depth16>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Scanlines32(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                 int width, int height) {
    if (Enabled())
        ScanlinesVector<Depth32>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        ScanlinesScalar<Depth32>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void ScanlinesTV(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                 int width, int height) {
    if (Enabled())
        ScanlinesTVVector<Depth16>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        ScanlinesTVScalar<Depth16>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void ScanlinesTV32(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                   int width, int height) {
    if (Enabled())
        ScanlinesTVVector<Depth32>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        ScanlinesTVScalar<Depth32>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}
