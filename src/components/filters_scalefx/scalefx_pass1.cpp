// ScaleFX Pass 1: Corner Strength Calculation
// Calculates interpolation strength for each of the 4 corners
// Input: Pass 0 metric data
// Output: float4 per pixel containing corner strengths
//
// Based on ScaleFX by Sp00kyFox, 2017-03-01
// CPU implementation with SSE3 optimization

#include "scalefx_simd.h"
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace scalefx {

// Helper struct for float4 values
struct Float4 {
    float x, y, z, w;
};

// Get metric data at position with boundary clamping (scalar)
static inline Float4 GetMetric(const float* pass0, int width, int height, int px, int py) {
    px = (px < 0) ? 0 : (px >= width) ? width - 1 : px;
    py = (py < 0) ? 0 : (py >= height) ? height - 1 : py;
    int idx = (py * width + px) * 4;
    return { pass0[idx + 0], pass0[idx + 1], pass0[idx + 2], pass0[idx + 3] };
}

#ifdef SCALEFX_USE_SSE3

// SSE helper: min of 4 floats
static inline __m128 sse_min(__m128 a, __m128 b) {
    return _mm_min_ps(a, b);
}

// SSE helper: max of 4 floats
static inline __m128 sse_max(__m128 a, __m128 b) {
    return _mm_max_ps(a, b);
}

// SSE3 optimized Pass1
static void Pass1_SSE3(const float* pass0, float* dst, int width, int height, float sfx_clr, int sfx_saa) {
    const __m128 zero = _mm_setzero_ps();
    const __m128 one = _mm_set1_ps(1.0f);
    const __m128 two = _mm_set1_ps(2.0f);
    const __m128 clr = _mm_set1_ps(sfx_clr);
    const __m128 inv_clr = _mm_set1_ps(1.0f / sfx_clr);
    const bool saa_enabled = (sfx_saa == 1);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Load metric data for 3x3 neighborhood
            // Each metric is (dist_EA, dist_EB, dist_EC, dist_EF) = (x, y, z, w)
            Float4 A = GetMetric(pass0, width, height, x - 1, y - 1);
            Float4 B = GetMetric(pass0, width, height, x,     y - 1);
            Float4 D = GetMetric(pass0, width, height, x - 1, y);
            Float4 E = GetMetric(pass0, width, height, x,     y);
            Float4 F = GetMetric(pass0, width, height, x + 1, y);
            Float4 G = GetMetric(pass0, width, height, x - 1, y + 1);
            Float4 H = GetMetric(pass0, width, height, x,     y + 1);
            Float4 I = GetMetric(pass0, width, height, x + 1, y + 1);

            // Load all 4 corner calculations into SSE vectors
            // Corner x: str(D.z, D.w, E.y, A.w, D.y)
            // Corner y: str(F.x, E.w, E.y, B.w, F.y)
            // Corner z: str(H.z, E.w, H.y, H.w, I.y)
            // Corner w: str(H.x, D.w, H.y, G.w, G.y)

            __m128 d_vec = _mm_set_ps(H.x, H.z, F.x, D.z);
            __m128 a_x_vec = _mm_set_ps(D.w, E.w, E.w, D.w);
            __m128 a_y_vec = _mm_set_ps(H.y, H.y, E.y, E.y);
            __m128 b_x_vec = _mm_set_ps(G.w, H.w, B.w, A.w);
            __m128 b_y_vec = _mm_set_ps(G.y, I.y, F.y, D.y);

            // Calculate diff = a_x - a_y
            __m128 diff = _mm_sub_ps(a_x_vec, a_y_vec);

            // wght1 = max(sfx_clr - d, 0) / sfx_clr
            __m128 wght1 = _mm_mul_ps(sse_max(_mm_sub_ps(clr, d_vec), zero), inv_clr);

            // min_ax_bx = min(a_x, b_x)
            // min_ay_by = min(a_y, b_y)
            __m128 min_ax_bx = sse_min(a_x_vec, b_x_vec);
            __m128 min_ay_by = sse_min(a_y_vec, b_y_vec);

            // cond_val = (min_ax_bx + a_x > min_ay_by + a_y) ? diff : -diff
            __m128 sum1 = _mm_add_ps(min_ax_bx, a_x_vec);
            __m128 sum2 = _mm_add_ps(min_ay_by, a_y_vec);
            __m128 cmp_mask = _mm_cmpgt_ps(sum1, sum2);
            __m128 neg_diff = _mm_sub_ps(zero, diff);
            __m128 cond_val = _mm_or_ps(_mm_and_ps(cmp_mask, diff), _mm_andnot_ps(cmp_mask, neg_diff));

            // wght2 = clamp((1 - d) + cond_val, 0, 1)
            __m128 wght2_raw = _mm_add_ps(_mm_sub_ps(one, d_vec), cond_val);
            __m128 wght2 = sse_min(sse_max(wght2_raw, zero), one);

            // result = wght1 * wght2 * a_x * a_y (conditional on saa check)
            __m128 strength = _mm_mul_ps(_mm_mul_ps(wght1, wght2), _mm_mul_ps(a_x_vec, a_y_vec));

            // SAA check: if (sfx_saa == 1 || 2*d < a_x + a_y)
            if (!saa_enabled) {
                __m128 a_sum = _mm_add_ps(a_x_vec, a_y_vec);
                __m128 d2 = _mm_mul_ps(two, d_vec);
                __m128 saa_mask = _mm_cmplt_ps(d2, a_sum);
                strength = _mm_and_ps(strength, saa_mask);
            }

            // Store results
            alignas(16) float results[4];
            _mm_store_ps(results, strength);

            int idx = (y * width + x) * 4;
            dst[idx + 0] = results[0];  // Corner x (top-left)
            dst[idx + 1] = results[1];  // Corner y (top-right)
            dst[idx + 2] = results[2];  // Corner z (bottom-right)
            dst[idx + 3] = results[3];  // Corner w (bottom-left)
        }
    }
}

#else // !SCALEFX_USE_SSE3

// Corner strength function (scalar)
static inline float StrScalar(float d, float a_x, float a_y, float b_x, float b_y,
                              float sfx_clr, int sfx_saa) {
    float diff = a_x - a_y;
    float wght1 = std::max(sfx_clr - d, 0.0f) / sfx_clr;

    float min_ax_bx = std::min(a_x, b_x);
    float min_ay_by = std::min(a_y, b_y);
    float cond = (min_ax_bx + a_x > min_ay_by + a_y) ? diff : -diff;
    float wght2 = std::max(0.0f, std::min(1.0f, (1.0f - d) + cond));

    if (sfx_saa == 1 || 2.0f * d < a_x + a_y) {
        return (wght1 * wght2) * (a_x * a_y);
    }
    return 0.0f;
}

// Scalar fallback
static void Pass1_Scalar(const float* pass0, float* dst, int width, int height, float sfx_clr, int sfx_saa) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Float4 A = GetMetric(pass0, width, height, x - 1, y - 1);
            Float4 B = GetMetric(pass0, width, height, x,     y - 1);
            Float4 D = GetMetric(pass0, width, height, x - 1, y);
            Float4 E = GetMetric(pass0, width, height, x,     y);
            Float4 F = GetMetric(pass0, width, height, x + 1, y);
            Float4 G = GetMetric(pass0, width, height, x - 1, y + 1);
            Float4 H = GetMetric(pass0, width, height, x,     y + 1);
            Float4 I = GetMetric(pass0, width, height, x + 1, y + 1);

            float res_x = StrScalar(D.z, D.w, E.y, A.w, D.y, sfx_clr, sfx_saa);
            float res_y = StrScalar(F.x, E.w, E.y, B.w, F.y, sfx_clr, sfx_saa);
            float res_z = StrScalar(H.z, E.w, H.y, H.w, I.y, sfx_clr, sfx_saa);
            float res_w = StrScalar(H.x, D.w, H.y, G.w, G.y, sfx_clr, sfx_saa);

            int idx = (y * width + x) * 4;
            dst[idx + 0] = res_x;
            dst[idx + 1] = res_y;
            dst[idx + 2] = res_z;
            dst[idx + 3] = res_w;
        }
    }
}

#endif // SCALEFX_USE_SSE3

// Dispatch function
void Pass1(const float* pass0, float* dst, int width, int height, float sfx_clr, int sfx_saa) {
#ifdef SCALEFX_USE_SSE3
    Pass1_SSE3(pass0, dst, width, height, sfx_clr, sfx_saa);
#else
    Pass1_Scalar(pass0, dst, width, height, sfx_clr, sfx_saa);
#endif
}

} // namespace scalefx
