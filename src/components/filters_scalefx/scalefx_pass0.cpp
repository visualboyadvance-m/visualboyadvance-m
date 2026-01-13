// ScaleFX Pass 0: Metric Data Preparation
// Calculates CIE-weighted color distance between center pixel and neighbors
// Output: float4 per pixel containing distances (E,A), (E,B), (E,C), (E,F)
//
// Based on ScaleFX by Sp00kyFox, 2017-03-01
// CPU implementation with SSE3 optimization

#include "scalefx_simd.h"
#include <cstdint>
#include <cmath>

namespace scalefx {

// Get pixel with boundary clamping (scalar)
static inline uint32_t GetPixelClamped(const uint8_t* src, int srcPitch, int width, int height, int x, int y) {
    x = (x < 0) ? 0 : (x >= width) ? width - 1 : x;
    y = (y < 0) ? 0 : (y >= height) ? height - 1 : y;
    const uint32_t* row = reinterpret_cast<const uint32_t*>(src + y * srcPitch);
    return row[x];
}

// CIE color distance (scalar version for boundaries)
static inline float ColorDistScalar(uint32_t c1, uint32_t c2) {
    float r1 = static_cast<float>((c1 >> 16) & 0xFF) / 255.0f;
    float g1 = static_cast<float>((c1 >> 8) & 0xFF) / 255.0f;
    float b1 = static_cast<float>(c1 & 0xFF) / 255.0f;

    float r2 = static_cast<float>((c2 >> 16) & 0xFF) / 255.0f;
    float g2 = static_cast<float>((c2 >> 8) & 0xFF) / 255.0f;
    float b2 = static_cast<float>(c2 & 0xFF) / 255.0f;

    float r = 0.5f * (r1 + r2);
    float dr = r1 - r2;
    float dg = g1 - g2;
    float db = b1 - b2;

    float cr = 2.0f + r;
    float cg = 4.0f;
    float cb = 3.0f - r;

    return std::sqrt(cr * dr * dr + cg * dg * dg + cb * db * db) / 3.0f;
}

#ifdef SCALEFX_USE_SSE3

// SSE3 optimized Pass0 - processes 4 pixels at a time
static void Pass0_SSE3(const uint32_t* src, int srcPitch, float* dst, int width, int height) {
    const uint8_t* srcBytes = reinterpret_cast<const uint8_t*>(src);

    const __m128 inv255 = _mm_set1_ps(1.0f / 255.0f);
    const __m128 half = _mm_set1_ps(0.5f);
    const __m128 two = _mm_set1_ps(2.0f);
    const __m128 three = _mm_set1_ps(3.0f);
    const __m128 four = _mm_set1_ps(4.0f);
    const __m128 inv3 = _mm_set1_ps(1.0f / 3.0f);

    for (int y = 0; y < height; y++) {
        const uint32_t* rowE = reinterpret_cast<const uint32_t*>(srcBytes + y * srcPitch);
        const uint32_t* rowB = reinterpret_cast<const uint32_t*>(srcBytes + ((y > 0) ? y - 1 : 0) * srcPitch);

        int x = 0;

        // Process 4 pixels at a time in the interior
        for (; x + 4 <= width; x += 4) {
            // Load 4 center pixels E
            __m128i E_i = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&rowE[x]));

            // Load neighbors with boundary handling
            __m128i A_i, B_i, C_i, F_i;

            // B: top neighbor (same x)
            B_i = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&rowB[x]));

            // For A, C, F we need to handle boundaries carefully
            // A: top-left (x-1, y-1)
            if (x > 0) {
                A_i = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&rowB[x - 1]));
            } else {
                // First pixel needs special handling
                A_i = _mm_set_epi32(rowB[x + 2], rowB[x + 1], rowB[x], rowB[0]);
            }

            // C: top-right (x+1, y-1)
            if (x + 4 < width) {
                C_i = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&rowB[x + 1]));
            } else {
                C_i = _mm_set_epi32(
                    rowB[(x + 3 < width) ? x + 4 : width - 1],
                    rowB[(x + 2 < width) ? x + 3 : width - 1],
                    rowB[(x + 1 < width) ? x + 2 : width - 1],
                    rowB[x + 1]
                );
            }

            // F: right neighbor (x+1, y)
            if (x + 4 < width) {
                F_i = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&rowE[x + 1]));
            } else {
                F_i = _mm_set_epi32(
                    rowE[(x + 3 < width) ? x + 4 : width - 1],
                    rowE[(x + 2 < width) ? x + 3 : width - 1],
                    rowE[(x + 1 < width) ? x + 2 : width - 1],
                    rowE[x + 1 < width ? x + 1 : width - 1]
                );
            }

            // Unpack E to RGB floats
            __m128i mask_r = _mm_set1_epi32(0x00FF0000);
            __m128i mask_g = _mm_set1_epi32(0x0000FF00);
            __m128i mask_b = _mm_set1_epi32(0x000000FF);

            __m128 E_r = _mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(_mm_and_si128(E_i, mask_r), 16)), inv255);
            __m128 E_g = _mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(_mm_and_si128(E_i, mask_g), 8)), inv255);
            __m128 E_b = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(E_i, mask_b)), inv255);

            // Helper lambda for distance calculation
            auto calcDist = [&](__m128i neighbor) -> __m128 {
                __m128 N_r = _mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(_mm_and_si128(neighbor, mask_r), 16)), inv255);
                __m128 N_g = _mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(_mm_and_si128(neighbor, mask_g), 8)), inv255);
                __m128 N_b = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(neighbor, mask_b)), inv255);

                __m128 r_avg = _mm_mul_ps(half, _mm_add_ps(E_r, N_r));
                __m128 dr = _mm_sub_ps(E_r, N_r);
                __m128 dg = _mm_sub_ps(E_g, N_g);
                __m128 db = _mm_sub_ps(E_b, N_b);

                __m128 cr = _mm_add_ps(two, r_avg);
                __m128 cb = _mm_sub_ps(three, r_avg);

                __m128 sum = _mm_add_ps(
                    _mm_add_ps(_mm_mul_ps(cr, _mm_mul_ps(dr, dr)),
                               _mm_mul_ps(four, _mm_mul_ps(dg, dg))),
                    _mm_mul_ps(cb, _mm_mul_ps(db, db))
                );

                return _mm_mul_ps(_mm_sqrt_ps(sum), inv3);
            };

            __m128 dist_ea = calcDist(A_i);
            __m128 dist_eb = calcDist(B_i);
            __m128 dist_ec = calcDist(C_i);
            __m128 dist_ef = calcDist(F_i);

            // Store results - interleaved as float4 per pixel
            // Format: [ea0,eb0,ec0,ef0, ea1,eb1,ec1,ef1, ...]
            alignas(16) float ea[4], eb[4], ec[4], ef[4];
            _mm_store_ps(ea, dist_ea);
            _mm_store_ps(eb, dist_eb);
            _mm_store_ps(ec, dist_ec);
            _mm_store_ps(ef, dist_ef);

            for (int i = 0; i < 4; i++) {
                int idx = (y * width + x + i) * 4;
                dst[idx + 0] = ea[i];
                dst[idx + 1] = eb[i];
                dst[idx + 2] = ec[i];
                dst[idx + 3] = ef[i];
            }
        }

        // Handle remaining pixels with scalar code
        for (; x < width; x++) {
            uint32_t E = rowE[x];
            uint32_t A = GetPixelClamped(srcBytes, srcPitch, width, height, x - 1, y - 1);
            uint32_t B = GetPixelClamped(srcBytes, srcPitch, width, height, x, y - 1);
            uint32_t C = GetPixelClamped(srcBytes, srcPitch, width, height, x + 1, y - 1);
            uint32_t F = GetPixelClamped(srcBytes, srcPitch, width, height, x + 1, y);

            int idx = (y * width + x) * 4;
            dst[idx + 0] = ColorDistScalar(E, A);
            dst[idx + 1] = ColorDistScalar(E, B);
            dst[idx + 2] = ColorDistScalar(E, C);
            dst[idx + 3] = ColorDistScalar(E, F);
        }
    }
}

#else // !SCALEFX_USE_SSE3

// Scalar fallback
static void Pass0_Scalar(const uint32_t* src, int srcPitch, float* dst, int width, int height) {
    const uint8_t* srcBytes = reinterpret_cast<const uint8_t*>(src);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t E = GetPixelClamped(srcBytes, srcPitch, width, height, x, y);
            uint32_t A = GetPixelClamped(srcBytes, srcPitch, width, height, x - 1, y - 1);
            uint32_t B = GetPixelClamped(srcBytes, srcPitch, width, height, x, y - 1);
            uint32_t C = GetPixelClamped(srcBytes, srcPitch, width, height, x + 1, y - 1);
            uint32_t F = GetPixelClamped(srcBytes, srcPitch, width, height, x + 1, y);

            int idx = (y * width + x) * 4;
            dst[idx + 0] = ColorDistScalar(E, A);
            dst[idx + 1] = ColorDistScalar(E, B);
            dst[idx + 2] = ColorDistScalar(E, C);
            dst[idx + 3] = ColorDistScalar(E, F);
        }
    }
}

#endif // SCALEFX_USE_SSE3

// Dispatch function
void Pass0(const uint32_t* src, int srcPitch, float* dst, int width, int height) {
#ifdef SCALEFX_USE_SSE3
    Pass0_SSE3(src, srcPitch, dst, width, height);
#else
    Pass0_Scalar(src, srcPitch, dst, width, height);
#endif
}

} // namespace scalefx
