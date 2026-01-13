// ScaleFX Pass 4: Final 3x Output
// Generates 3x3 output block for each source pixel using subpixel tags
// Input: Pass 3 subpixel tags + original source pixels
// Output: 3x scaled image
//
// Based on ScaleFX by Sp00kyFox, 2017-03-01
// CPU implementation

#include "scalefx_simd.h"
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace scalefx {

// Subpixel offset table (maps index to pixel offset)
// Index: 0=E, 1=D, 2=D0, 3=F, 4=F0, 5=B, 6=B0, 7=H, 8=H0
static const int g_offset_x[9] = { 0, -1, -2,  1,  2,  0,  0,  0,  0 };
static const int g_offset_y[9] = { 0,  0,  0,  0,  0, -1, -2,  1,  2 };

// Unpack Pass 3 data to corner and mid indices using integer math
// Pass 3 packs as: (crn + 9*mid) / 80
// So packed * 80 = crn + 9*mid, crn = packed % 9, mid = packed / 9
static inline void UnpackPass3(float packed, int& crn, int& mid) {
    int p = static_cast<int>(packed * 80.0f + 0.5f);
    crn = p % 9;
    mid = p / 9;
}

// Get pixel with boundary clamping
static inline uint32_t GetPixel(const uint8_t* src, int srcPitch, int width, int height, int x, int y) {
    x = (x < 0) ? 0 : (x >= width) ? width - 1 : x;
    y = (y < 0) ? 0 : (y >= height) ? height - 1 : y;
    const uint32_t* row = reinterpret_cast<const uint32_t*>(src + y * srcPitch);
    return row[x];
}

// Get Pass3 data at position
static inline void GetPass3Data(const float* pass3, int width, int height, int px, int py,
                                 float& e0, float& e1, float& e2, float& e3) {
    px = (px < 0) ? 0 : (px >= width) ? width - 1 : px;
    py = (py < 0) ? 0 : (py >= height) ? height - 1 : py;
    int idx = (py * width + px) * 4;
    e0 = pass3[idx + 0];
    e1 = pass3[idx + 1];
    e2 = pass3[idx + 2];
    e3 = pass3[idx + 3];
}

// Fast pixel fetch with pre-clamped coordinates (no boundary check needed for interior)
static inline uint32_t GetPixelFast(const uint32_t* srcRow, int x, int width) {
    return srcRow[(x < 0) ? 0 : (x >= width) ? width - 1 : x];
}

void Pass4(const uint32_t* src, int srcPitch, const float* pass3,
           uint32_t* dst, int dstPitch, int width, int height,
           float /*sfx_raa*/, bool /*hybrid_mode*/) {
    uint8_t* dstBytes = reinterpret_cast<uint8_t*>(dst);

    // Precompute row pointers for source rows
    const int srcPitchPixels = srcPitch / 4;

    for (int y = 0; y < height; y++) {
        // Get destination row pointers
        uint32_t* dstRow0 = reinterpret_cast<uint32_t*>(dstBytes + (y * 3) * dstPitch);
        uint32_t* dstRow1 = reinterpret_cast<uint32_t*>(dstBytes + (y * 3 + 1) * dstPitch);
        uint32_t* dstRow2 = reinterpret_cast<uint32_t*>(dstBytes + (y * 3 + 2) * dstPitch);

        // Get source row pointers with clamping
        const uint32_t* srcRowB2 = src + ((y > 1) ? y - 2 : 0) * srcPitchPixels;
        const uint32_t* srcRowB  = src + ((y > 0) ? y - 1 : 0) * srcPitchPixels;
        const uint32_t* srcRowE  = src + y * srcPitchPixels;
        const uint32_t* srcRowH  = src + ((y < height - 1) ? y + 1 : height - 1) * srcPitchPixels;
        const uint32_t* srcRowH2 = src + ((y < height - 2) ? y + 2 : height - 1) * srcPitchPixels;

        // Pass3 row pointer
        const float* pass3Row = pass3 + y * width * 4;

        for (int x = 0; x < width; x++) {
            // Read Pass 3 data directly (no boundary check needed, we're iterating within bounds)
            int pass3Idx = x * 4;
            float E0 = pass3Row[pass3Idx + 0];
            float E1 = pass3Row[pass3Idx + 1];
            float E2 = pass3Row[pass3Idx + 2];
            float E3 = pass3Row[pass3Idx + 3];

            // Unpack corner and mid indices using integer math
            int crn_tl, mid_l, crn_tr, mid_t, crn_br, mid_r, crn_bl, mid_b;
            UnpackPass3(E0, crn_tl, mid_l);
            UnpackPass3(E1, crn_tr, mid_t);
            UnpackPass3(E2, crn_br, mid_r);
            UnpackPass3(E3, crn_bl, mid_b);

            // Output position
            int dx = x * 3;

            // Precompute x coordinates for neighbors
            int xD2 = (x > 1) ? x - 2 : 0;
            int xD  = (x > 0) ? x - 1 : 0;
            int xF  = (x < width - 1) ? x + 1 : width - 1;
            int xF2 = (x < width - 2) ? x + 2 : width - 1;

            // Lookup table for subpixel -> pixel (unrolled for speed)
            // Index: 0=E, 1=D, 2=D0, 3=F, 4=F0, 5=B, 6=B0, 7=H, 8=H0
            auto getPixel = [&](int idx) -> uint32_t {
                switch (idx) {
                    case 0: return srcRowE[x];
                    case 1: return srcRowE[xD];
                    case 2: return srcRowE[xD2];
                    case 3: return srcRowE[xF];
                    case 4: return srcRowE[xF2];
                    case 5: return srcRowB[x];
                    case 6: return srcRowB2[x];
                    case 7: return srcRowH[x];
                    case 8: return srcRowH2[x];
                    default: return srcRowE[x];
                }
            };

            // Write all 9 subpixels (fully unrolled)
            // Row 0: crn_tl, mid_t, crn_tr
            dstRow0[dx + 0] = getPixel(crn_tl);
            dstRow0[dx + 1] = getPixel(mid_t);
            dstRow0[dx + 2] = getPixel(crn_tr);

            // Row 1: mid_l, E (always 0), mid_r
            dstRow1[dx + 0] = getPixel(mid_l);
            dstRow1[dx + 1] = srcRowE[x];  // Center is always E
            dstRow1[dx + 2] = getPixel(mid_r);

            // Row 2: crn_bl, mid_b, crn_br
            dstRow2[dx + 0] = getPixel(crn_bl);
            dstRow2[dx + 1] = getPixel(mid_b);
            dstRow2[dx + 2] = getPixel(crn_br);
        }
    }
}

// Fast 50% blend between two pixels (scalar version)
static inline uint32_t Blend50(uint32_t p0, uint32_t p1) {
    // Average each channel: (a + b) / 2 = ((a ^ b) >> 1) + (a & b)
    return (((p0 ^ p1) & 0xFEFEFEFE) >> 1) + (p0 & p1);
}

#ifdef SCALEFX_USE_MMX

// MMX-optimized Pass4 for 9x output
void Pass4_9x(const uint32_t* src, int srcPitch, const float* pass3,
              uint32_t* dst, int dstPitch, int width, int height,
              float /*sfx_raa*/, bool /*hybrid_mode*/) {
    uint8_t* dstBytes = reinterpret_cast<uint8_t*>(dst);
    const int srcPitchPixels = srcPitch / 4;

    for (int y = 0; y < height; y++) {
        // Get source row pointers with clamping
        const uint32_t* srcRowB2 = src + ((y > 1) ? y - 2 : 0) * srcPitchPixels;
        const uint32_t* srcRowB  = src + ((y > 0) ? y - 1 : 0) * srcPitchPixels;
        const uint32_t* srcRowE  = src + y * srcPitchPixels;
        const uint32_t* srcRowH  = src + ((y < height - 1) ? y + 1 : height - 1) * srcPitchPixels;
        const uint32_t* srcRowH2 = src + ((y < height - 2) ? y + 2 : height - 1) * srcPitchPixels;

        // Pass3 row pointer
        const float* pass3Row = pass3 + y * width * 4;

        // Destination row pointers for this source row's 9x9 block
        int dy = y * 9;
        uint32_t* dstRows[9];
        for (int i = 0; i < 9; i++) {
            dstRows[i] = reinterpret_cast<uint32_t*>(dstBytes + (dy + i) * dstPitch);
        }

        for (int x = 0; x < width; x++) {
            // Read Pass 3 data
            int pass3Idx = x * 4;
            float E0 = pass3Row[pass3Idx + 0];
            float E1 = pass3Row[pass3Idx + 1];
            float E2 = pass3Row[pass3Idx + 2];
            float E3 = pass3Row[pass3Idx + 3];

            // Unpack corner and mid indices
            int crn_tl, mid_l, crn_tr, mid_t, crn_br, mid_r, crn_bl, mid_b;
            UnpackPass3(E0, crn_tl, mid_l);
            UnpackPass3(E1, crn_tr, mid_t);
            UnpackPass3(E2, crn_br, mid_r);
            UnpackPass3(E3, crn_bl, mid_b);

            // Precompute x coordinates for neighbors
            int xD2 = (x > 1) ? x - 2 : 0;
            int xD  = (x > 0) ? x - 1 : 0;
            int xF  = (x < width - 1) ? x + 1 : width - 1;
            int xF2 = (x < width - 2) ? x + 2 : width - 1;

            // Get the 9 key sample pixels
            auto getPixel = [&](int idx) -> uint32_t {
                switch (idx) {
                    case 0: return srcRowE[x];
                    case 1: return srcRowE[xD];
                    case 2: return srcRowE[xD2];
                    case 3: return srcRowE[xF];
                    case 4: return srcRowE[xF2];
                    case 5: return srcRowB[x];
                    case 6: return srcRowB2[x];
                    case 7: return srcRowH[x];
                    case 8: return srcRowH2[x];
                    default: return srcRowE[x];
                }
            };

            // 3x3 key samples
            uint32_t k00 = getPixel(crn_tl), k10 = getPixel(mid_t),  k20 = getPixel(crn_tr);
            uint32_t k01 = getPixel(mid_l),  k11 = srcRowE[x],       k21 = getPixel(mid_r);
            uint32_t k02 = getPixel(crn_bl), k12 = getPixel(mid_b),  k22 = getPixel(crn_br);

            // Use MMX for blending - pack pairs of pixels
            __m64 mk00k10 = _mm_set_pi32(k10, k00);
            __m64 mk10k20 = _mm_set_pi32(k20, k10);
            __m64 mk01k11 = _mm_set_pi32(k11, k01);
            __m64 mk11k21 = _mm_set_pi32(k21, k11);
            __m64 mk02k12 = _mm_set_pi32(k12, k02);
            __m64 mk12k22 = _mm_set_pi32(k22, k12);

            __m64 mk00k01 = _mm_set_pi32(k01, k00);
            __m64 mk01k02 = _mm_set_pi32(k02, k01);
            __m64 mk10k11 = _mm_set_pi32(k11, k10);
            __m64 mk11k12 = _mm_set_pi32(k12, k11);
            __m64 mk20k21 = _mm_set_pi32(k21, k20);
            __m64 mk21k22 = _mm_set_pi32(k22, k21);

            // Horizontal blends using MMX
            __m64 mb01_0_b12_0 = MMXBlend50(mk00k10, mk10k20);  // b01_0, b12_0
            __m64 mb01_1_b12_1 = MMXBlend50(mk01k11, mk11k21);  // b01_1, b12_1
            __m64 mb01_2_b12_2 = MMXBlend50(mk02k12, mk12k22);  // b01_2, b12_2

            // Vertical blends using MMX
            __m64 mb0_01_b0_12 = MMXBlend50(mk00k01, mk01k02);  // b0_01, b0_12
            __m64 mb1_01_b1_12 = MMXBlend50(mk10k11, mk11k12);  // b1_01, b1_12
            __m64 mb2_01_b2_12 = MMXBlend50(mk20k21, mk21k22);  // b2_01, b2_12

            // Extract blended values
            uint32_t b01_0 = _mm_cvtsi64_si32(mb01_0_b12_0);
            uint32_t b12_0 = _mm_cvtsi64_si32(_mm_srli_si64(mb01_0_b12_0, 32));
            uint32_t b01_1 = _mm_cvtsi64_si32(mb01_1_b12_1);
            uint32_t b12_1 = _mm_cvtsi64_si32(_mm_srli_si64(mb01_1_b12_1, 32));
            uint32_t b01_2 = _mm_cvtsi64_si32(mb01_2_b12_2);
            uint32_t b12_2 = _mm_cvtsi64_si32(_mm_srli_si64(mb01_2_b12_2, 32));

            uint32_t b0_01 = _mm_cvtsi64_si32(mb0_01_b0_12);
            uint32_t b0_12 = _mm_cvtsi64_si32(_mm_srli_si64(mb0_01_b0_12, 32));
            uint32_t b1_01 = _mm_cvtsi64_si32(mb1_01_b1_12);
            uint32_t b1_12 = _mm_cvtsi64_si32(_mm_srli_si64(mb1_01_b1_12, 32));
            uint32_t b2_01 = _mm_cvtsi64_si32(mb2_01_b2_12);
            uint32_t b2_12 = _mm_cvtsi64_si32(_mm_srli_si64(mb2_01_b2_12, 32));

            // Corner blends (4-way) - use scalar for simplicity
            uint32_t c_01_01 = Blend50(b01_0, b01_1);
            uint32_t c_12_01 = Blend50(b12_0, b12_1);
            uint32_t c_01_12 = Blend50(b01_1, b01_2);
            uint32_t c_12_12 = Blend50(b12_1, b12_2);

            int dx = x * 9;

            // Write using MMX where we have pairs of identical pixels
            __m64 mk00k00 = _mm_set1_pi32(k00);
            __m64 mk10k10 = _mm_set1_pi32(k10);
            __m64 mk20k20 = _mm_set1_pi32(k20);
            __m64 mk01k01 = _mm_set1_pi32(k01);
            __m64 mk11k11 = _mm_set1_pi32(k11);
            __m64 mk21k21 = _mm_set1_pi32(k21);
            __m64 mk02k02 = _mm_set1_pi32(k02);
            __m64 mk12k12 = _mm_set1_pi32(k12);
            __m64 mk22k22 = _mm_set1_pi32(k22);

            __m64 mb0_01x2 = _mm_set1_pi32(b0_01);
            __m64 mb1_01x2 = _mm_set1_pi32(b1_01);
            __m64 mb2_01x2 = _mm_set1_pi32(b2_01);
            __m64 mb0_12x2 = _mm_set1_pi32(b0_12);
            __m64 mb1_12x2 = _mm_set1_pi32(b1_12);
            __m64 mb2_12x2 = _mm_set1_pi32(b2_12);

            // Row 0
            MMXStore2(&dstRows[0][dx+0], mk00k00);
            dstRows[0][dx+2] = b01_0;
            MMXStore2(&dstRows[0][dx+3], mk10k10);
            dstRows[0][dx+5] = b12_0;
            MMXStore2(&dstRows[0][dx+6], mk20k20);
            dstRows[0][dx+8] = k20;

            // Row 1 (same as row 0)
            MMXStore2(&dstRows[1][dx+0], mk00k00);
            dstRows[1][dx+2] = b01_0;
            MMXStore2(&dstRows[1][dx+3], mk10k10);
            dstRows[1][dx+5] = b12_0;
            MMXStore2(&dstRows[1][dx+6], mk20k20);
            dstRows[1][dx+8] = k20;

            // Row 2: blend row
            MMXStore2(&dstRows[2][dx+0], mb0_01x2);
            dstRows[2][dx+2] = c_01_01;
            MMXStore2(&dstRows[2][dx+3], mb1_01x2);
            dstRows[2][dx+5] = c_12_01;
            MMXStore2(&dstRows[2][dx+6], mb2_01x2);
            dstRows[2][dx+8] = b2_01;

            // Row 3
            MMXStore2(&dstRows[3][dx+0], mk01k01);
            dstRows[3][dx+2] = b01_1;
            MMXStore2(&dstRows[3][dx+3], mk11k11);
            dstRows[3][dx+5] = b12_1;
            MMXStore2(&dstRows[3][dx+6], mk21k21);
            dstRows[3][dx+8] = k21;

            // Row 4 (same as row 3)
            MMXStore2(&dstRows[4][dx+0], mk01k01);
            dstRows[4][dx+2] = b01_1;
            MMXStore2(&dstRows[4][dx+3], mk11k11);
            dstRows[4][dx+5] = b12_1;
            MMXStore2(&dstRows[4][dx+6], mk21k21);
            dstRows[4][dx+8] = k21;

            // Row 5: blend row
            MMXStore2(&dstRows[5][dx+0], mb0_12x2);
            dstRows[5][dx+2] = c_01_12;
            MMXStore2(&dstRows[5][dx+3], mb1_12x2);
            dstRows[5][dx+5] = c_12_12;
            MMXStore2(&dstRows[5][dx+6], mb2_12x2);
            dstRows[5][dx+8] = b2_12;

            // Row 6
            MMXStore2(&dstRows[6][dx+0], mk02k02);
            dstRows[6][dx+2] = b01_2;
            MMXStore2(&dstRows[6][dx+3], mk12k12);
            dstRows[6][dx+5] = b12_2;
            MMXStore2(&dstRows[6][dx+6], mk22k22);
            dstRows[6][dx+8] = k22;

            // Row 7 (same as row 6)
            MMXStore2(&dstRows[7][dx+0], mk02k02);
            dstRows[7][dx+2] = b01_2;
            MMXStore2(&dstRows[7][dx+3], mk12k12);
            dstRows[7][dx+5] = b12_2;
            MMXStore2(&dstRows[7][dx+6], mk22k22);
            dstRows[7][dx+8] = k22;

            // Row 8 (same as row 6)
            MMXStore2(&dstRows[8][dx+0], mk02k02);
            dstRows[8][dx+2] = b01_2;
            MMXStore2(&dstRows[8][dx+3], mk12k12);
            dstRows[8][dx+5] = b12_2;
            MMXStore2(&dstRows[8][dx+6], mk22k22);
            dstRows[8][dx+8] = k22;
        }
    }
    MMXEnd();
}

#else // !SCALEFX_USE_MMX

// Scalar Pass4 for 9x output - fast version with minimal blending
// Uses nearest-neighbor for most pixels, blends only at block boundaries
void Pass4_9x(const uint32_t* src, int srcPitch, const float* pass3,
              uint32_t* dst, int dstPitch, int width, int height,
              float /*sfx_raa*/, bool /*hybrid_mode*/) {
    uint8_t* dstBytes = reinterpret_cast<uint8_t*>(dst);
    const int srcPitchPixels = srcPitch / 4;

    for (int y = 0; y < height; y++) {
        // Get source row pointers with clamping
        const uint32_t* srcRowB2 = src + ((y > 1) ? y - 2 : 0) * srcPitchPixels;
        const uint32_t* srcRowB  = src + ((y > 0) ? y - 1 : 0) * srcPitchPixels;
        const uint32_t* srcRowE  = src + y * srcPitchPixels;
        const uint32_t* srcRowH  = src + ((y < height - 1) ? y + 1 : height - 1) * srcPitchPixels;
        const uint32_t* srcRowH2 = src + ((y < height - 2) ? y + 2 : height - 1) * srcPitchPixels;

        // Pass3 row pointer
        const float* pass3Row = pass3 + y * width * 4;

        // Destination row pointers for this source row's 9x9 block
        int dy = y * 9;
        uint32_t* dstRows[9];
        for (int i = 0; i < 9; i++) {
            dstRows[i] = reinterpret_cast<uint32_t*>(dstBytes + (dy + i) * dstPitch);
        }

        for (int x = 0; x < width; x++) {
            // Read Pass 3 data
            int pass3Idx = x * 4;
            float E0 = pass3Row[pass3Idx + 0];
            float E1 = pass3Row[pass3Idx + 1];
            float E2 = pass3Row[pass3Idx + 2];
            float E3 = pass3Row[pass3Idx + 3];

            // Unpack corner and mid indices
            int crn_tl, mid_l, crn_tr, mid_t, crn_br, mid_r, crn_bl, mid_b;
            UnpackPass3(E0, crn_tl, mid_l);
            UnpackPass3(E1, crn_tr, mid_t);
            UnpackPass3(E2, crn_br, mid_r);
            UnpackPass3(E3, crn_bl, mid_b);

            // Precompute x coordinates for neighbors
            int xD2 = (x > 1) ? x - 2 : 0;
            int xD  = (x > 0) ? x - 1 : 0;
            int xF  = (x < width - 1) ? x + 1 : width - 1;
            int xF2 = (x < width - 2) ? x + 2 : width - 1;

            // Get the 9 key sample pixels
            auto getPixel = [&](int idx) -> uint32_t {
                switch (idx) {
                    case 0: return srcRowE[x];
                    case 1: return srcRowE[xD];
                    case 2: return srcRowE[xD2];
                    case 3: return srcRowE[xF];
                    case 4: return srcRowE[xF2];
                    case 5: return srcRowB[x];
                    case 6: return srcRowB2[x];
                    case 7: return srcRowH[x];
                    case 8: return srcRowH2[x];
                    default: return srcRowE[x];
                }
            };

            // 3x3 key samples
            uint32_t k00 = getPixel(crn_tl), k10 = getPixel(mid_t),  k20 = getPixel(crn_tr);
            uint32_t k01 = getPixel(mid_l),  k11 = srcRowE[x],       k21 = getPixel(mid_r);
            uint32_t k02 = getPixel(crn_bl), k12 = getPixel(mid_b),  k22 = getPixel(crn_br);

            // Precompute horizontal blends (for column boundaries)
            uint32_t b01_0 = Blend50(k00, k10), b12_0 = Blend50(k10, k20);
            uint32_t b01_1 = Blend50(k01, k11), b12_1 = Blend50(k11, k21);
            uint32_t b01_2 = Blend50(k02, k12), b12_2 = Blend50(k12, k22);

            // Precompute vertical blends (for row boundaries)
            uint32_t b0_01 = Blend50(k00, k01), b0_12 = Blend50(k01, k02);
            uint32_t b1_01 = Blend50(k10, k11), b1_12 = Blend50(k11, k12);
            uint32_t b2_01 = Blend50(k20, k21), b2_12 = Blend50(k21, k22);

            // Precompute corner blends (4-way)
            uint32_t c_01_01 = Blend50(b01_0, b01_1);
            uint32_t c_12_01 = Blend50(b12_0, b12_1);
            uint32_t c_01_12 = Blend50(b01_1, b01_2);
            uint32_t c_12_12 = Blend50(b12_1, b12_2);

            int dx = x * 9;

            // Row 0-2: top key row (k00, k10, k20) region
            // Row 0: pure top
            dstRows[0][dx+0] = k00; dstRows[0][dx+1] = k00; dstRows[0][dx+2] = b01_0;
            dstRows[0][dx+3] = k10; dstRows[0][dx+4] = k10; dstRows[0][dx+5] = b12_0;
            dstRows[0][dx+6] = k20; dstRows[0][dx+7] = k20; dstRows[0][dx+8] = k20;

            // Row 1: same as row 0
            dstRows[1][dx+0] = k00; dstRows[1][dx+1] = k00; dstRows[1][dx+2] = b01_0;
            dstRows[1][dx+3] = k10; dstRows[1][dx+4] = k10; dstRows[1][dx+5] = b12_0;
            dstRows[1][dx+6] = k20; dstRows[1][dx+7] = k20; dstRows[1][dx+8] = k20;

            // Row 2: blend toward middle row
            dstRows[2][dx+0] = b0_01; dstRows[2][dx+1] = b0_01; dstRows[2][dx+2] = c_01_01;
            dstRows[2][dx+3] = b1_01; dstRows[2][dx+4] = b1_01; dstRows[2][dx+5] = c_12_01;
            dstRows[2][dx+6] = b2_01; dstRows[2][dx+7] = b2_01; dstRows[2][dx+8] = b2_01;

            // Row 3-5: middle key row (k01, k11, k21) region
            // Row 3: pure middle
            dstRows[3][dx+0] = k01; dstRows[3][dx+1] = k01; dstRows[3][dx+2] = b01_1;
            dstRows[3][dx+3] = k11; dstRows[3][dx+4] = k11; dstRows[3][dx+5] = b12_1;
            dstRows[3][dx+6] = k21; dstRows[3][dx+7] = k21; dstRows[3][dx+8] = k21;

            // Row 4: same as row 3
            dstRows[4][dx+0] = k01; dstRows[4][dx+1] = k01; dstRows[4][dx+2] = b01_1;
            dstRows[4][dx+3] = k11; dstRows[4][dx+4] = k11; dstRows[4][dx+5] = b12_1;
            dstRows[4][dx+6] = k21; dstRows[4][dx+7] = k21; dstRows[4][dx+8] = k21;

            // Row 5: blend toward bottom row
            dstRows[5][dx+0] = b0_12; dstRows[5][dx+1] = b0_12; dstRows[5][dx+2] = c_01_12;
            dstRows[5][dx+3] = b1_12; dstRows[5][dx+4] = b1_12; dstRows[5][dx+5] = c_12_12;
            dstRows[5][dx+6] = b2_12; dstRows[5][dx+7] = b2_12; dstRows[5][dx+8] = b2_12;

            // Row 6-8: bottom key row (k02, k12, k22) region
            // Row 6: pure bottom
            dstRows[6][dx+0] = k02; dstRows[6][dx+1] = k02; dstRows[6][dx+2] = b01_2;
            dstRows[6][dx+3] = k12; dstRows[6][dx+4] = k12; dstRows[6][dx+5] = b12_2;
            dstRows[6][dx+6] = k22; dstRows[6][dx+7] = k22; dstRows[6][dx+8] = k22;

            // Row 7: same as row 6
            dstRows[7][dx+0] = k02; dstRows[7][dx+1] = k02; dstRows[7][dx+2] = b01_2;
            dstRows[7][dx+3] = k12; dstRows[7][dx+4] = k12; dstRows[7][dx+5] = b12_2;
            dstRows[7][dx+6] = k22; dstRows[7][dx+7] = k22; dstRows[7][dx+8] = k22;

            // Row 8: same as row 6
            dstRows[8][dx+0] = k02; dstRows[8][dx+1] = k02; dstRows[8][dx+2] = b01_2;
            dstRows[8][dx+3] = k12; dstRows[8][dx+4] = k12; dstRows[8][dx+5] = b12_2;
            dstRows[8][dx+6] = k22; dstRows[8][dx+7] = k22; dstRows[8][dx+8] = k22;
        }
    }
}

#endif // SCALEFX_USE_MMX

} // namespace scalefx
