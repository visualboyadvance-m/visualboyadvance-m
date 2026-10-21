// Simple NxN pixel replication, 16 and 32 bits per pixel.
//
// One vector kernel per scale factor and depth (SSE2 / NEON, see simd128.h) and
// a scalar kernel for the other targets; the test suite checks that the two agree.

#include <cstdint>

#include "../simd128.h"

namespace {

using namespace vbam_simd;

// ---------------------------------------------------------------------------
// Scalar kernels
// ---------------------------------------------------------------------------

template <typename Pixel, int kScale>
void SimpleScalar(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                  int height) {
    for (int y = 0; y < height; y++) {
        const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr + y * srcPitch);
        for (int r = 0; r < kScale; r++) {
            Pixel* dst = reinterpret_cast<Pixel*>(dstPtr + (y * kScale + r) * dstPitch);
            for (int x = 0; x < width; x++) {
                const Pixel c = src[x];
                for (int k = 0; k < kScale; k++)
                    dst[x * kScale + k] = c;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Vector kernels: every lane repeated kScale times with an interleaving store
// (vst2 / vst3 / vst4 on NEON, shuffles on SSE2)
// ---------------------------------------------------------------------------

template <typename Pixel, int kScale>
VBAM_SIMD_INLINE void StoreRep(Pixel* p, V a) {
    if constexpr (sizeof(Pixel) == 4) {
        if constexpr (kScale == 2) store_rep2_u32(p, a);
        else if constexpr (kScale == 3) store_rep3_u32(p, a);
        else store_rep4_u32(p, a);
    } else {
        if constexpr (kScale == 2) store_rep2_u16(p, a);
        else if constexpr (kScale == 3) store_rep3_u16(p, a);
        else store_rep4_u16(p, a);
    }
}

template <typename Pixel, int kScale>
void SimpleVector(const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                  int height) {
    constexpr int kLanes = 16 / sizeof(Pixel);
    for (int y = 0; y < height; y++) {
        const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr + y * srcPitch);
        Pixel* rows[kScale];
        for (int r = 0; r < kScale; r++)
            rows[r] = reinterpret_cast<Pixel*>(dstPtr + (y * kScale + r) * dstPitch);
        int x = 0;
        for (; x + kLanes <= width; x += kLanes) {
            const V p = load(src + x);
            for (int r = 0; r < kScale; r++)
                StoreRep<Pixel, kScale>(rows[r] + x * kScale, p);
        }
        for (; x < width; x++) {
            const Pixel c = src[x];
            for (int r = 0; r < kScale; r++)
                for (int k = 0; k < kScale; k++)
                    rows[r][x * kScale + k] = c;
        }
    }
}

template <typename Pixel, int kScale>
void Simple(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width, int height) {
    if (Enabled()) {
        SimpleVector<Pixel, kScale>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    } else {
        SimpleScalar<Pixel, kScale>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    }
}

}  // namespace

void Simple2x16(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                int width, int height) {
    Simple<uint16_t, 2>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Simple2x32(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                int width, int height) {
    Simple<uint32_t, 2>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Simple3x16(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                int width, int height) {
    Simple<uint16_t, 3>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Simple3x32(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                int width, int height) {
    Simple<uint32_t, 3>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Simple4x16(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                int width, int height) {
    Simple<uint16_t, 4>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Simple4x32(uint8_t* srcPtr, uint32_t srcPitch, uint8_t* /* deltaPtr */, uint8_t* dstPtr, uint32_t dstPitch,
                int width, int height) {
    Simple<uint32_t, 4>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}
