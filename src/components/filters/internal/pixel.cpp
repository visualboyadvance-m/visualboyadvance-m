// Pixelate: 2x with a grid, the right and lower neighbours of each pixel at 1/4
// brightness. 16 and 32 bits per pixel.
//
// One vector kernel per depth (SSE2 / NEON, see simd128.h) and a scalar kernel
// for the other targets; the test suite checks that the two agree.

#include <cstdint>
#include <cstring>

#include "../simd128.h"

extern int RGB_LOW_BITS_MASK;

namespace {

using namespace vbam_simd;

struct Depth32 {
    using Pixel = uint32_t;
    static constexpr int kLanes = 4;
    static void Store2(Pixel* p, V a, V b) { store2_u32(p, a, b); }
    static V Quarter(V a, V mask) { return srl_u32<2>(and_(a, mask)); }
    static V Set1(Pixel p) { return set1_u32(p); }
};

struct Depth16 {
    using Pixel = uint16_t;
    static constexpr int kLanes = 8;
    static void Store2(Pixel* p, V a, V b) { store2_u16(p, a, b); }
    static V Quarter(V a, V mask) { return srl_u16<2>(and_(a, mask)); }
    static V Set1(Pixel p) { return set1_u16(p); }
};

// mask that clears the two low bits of every channel: (c & mask) >> 2 is the
// pixel at a quarter without carries between channels
template <class D>
typename D::Pixel QuarterMask() {
    return static_cast<typename D::Pixel>(~(RGB_LOW_BITS_MASK | (RGB_LOW_BITS_MASK << 1)));
}

template <class D>
void PixelateScalar(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                    int height) {
    using Pixel = typename D::Pixel;
    const Pixel mask = QuarterMask<D>();
    for (int y = 0; y < height; y++) {
        const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr + y * srcPitch);
        Pixel* dst0 = reinterpret_cast<Pixel*>(dstPtr + (y * 2) * dstPitch);
        Pixel* dst1 = reinterpret_cast<Pixel*>(dstPtr + (y * 2 + 1) * dstPitch);
        for (int x = 0; x < width; x++) {
            const Pixel c = src[x];
            const Pixel q = static_cast<Pixel>((c & mask) >> 2);
            dst0[x * 2] = c;
            dst0[x * 2 + 1] = q;
            dst1[x * 2] = q;
            dst1[x * 2 + 1] = q;
        }
    }
}

template <class D>
void PixelateVector(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                    int height) {
    using Pixel = typename D::Pixel;
    constexpr int L = D::kLanes;
    const Pixel maskPixel = QuarterMask<D>();
    const V mask = D::Set1(maskPixel);
    for (int y = 0; y < height; y++) {
        const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr + y * srcPitch);
        Pixel* dst0 = reinterpret_cast<Pixel*>(dstPtr + (y * 2) * dstPitch);
        Pixel* dst1 = reinterpret_cast<Pixel*>(dstPtr + (y * 2 + 1) * dstPitch);
        int x = 0;
        for (; x + L <= width; x += L) {
            const V c = load(src + x);
            const V q = D::Quarter(c, mask);
            D::Store2(dst0 + x * 2, c, q);
            D::Store2(dst1 + x * 2, q, q);
        }
        for (; x < width; x++) {
            const Pixel c = src[x];
            const Pixel q = static_cast<Pixel>((c & maskPixel) >> 2);
            dst0[x * 2] = c;
            dst0[x * 2 + 1] = q;
            dst1[x * 2] = q;
            dst1[x * 2 + 1] = q;
        }
    }
}

}  // namespace

void Pixelate(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* deltaPtr, uint8_t* dstPtr, uint32_t dstPitch, int width,
              int height) {
    if (Enabled())
        PixelateVector<Depth16>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        PixelateScalar<Depth16>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);

    // the delta buffer records the frame that was filtered
    if (deltaPtr)
        for (int y = 0; y < height; y++)
            std::memcpy(deltaPtr + y * srcPitch, srcPtr + y * srcPitch, width * sizeof(uint16_t));
}

void Pixelate32(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                int width, int height) {
    if (Enabled())
        PixelateVector<Depth32>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    else
        PixelateScalar<Depth32>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}
