// ScaleFX Pass 1: Corner Strength Calculation
// Calculates interpolation strength for each of the 4 corners
// Input: Pass 0 metric data
// Output: float4 per pixel containing corner strengths
//
// Based on ScaleFX by Sp00kyFox, 2017-03-01
// CPU implementation: the four corners of a pixel are the four lanes
// (SSE2 / NEON), or scalar

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "scalefx_internal.h"

namespace scalefx {

namespace {

// Corner strength function (scalar); the operations in the order of the vector kernel
inline float StrScalar(float d, float a_x, float a_y, float b_x, float b_y, float sfx_clr, float inv_clr,
                       int sfx_saa) {
    const float diff = a_x - a_y;
    const float wght1 = std::max(sfx_clr - d, 0.0f) * inv_clr;

    const float min_ax_bx = std::min(a_x, b_x);
    const float min_ay_by = std::min(a_y, b_y);
    const float cond = (min_ax_bx + a_x > min_ay_by + a_y) ? diff : (0.0f - diff);
    const float wght2 = std::min(std::max((1.0f - d) + cond, 0.0f), 1.0f);

    const float strength = (wght1 * wght2) * (a_x * a_y);
    if (sfx_saa == 1 || 2.0f * d < a_x + a_y)
        return strength;
    return 0.0f;
}

void Pass1Scalar(const float* pass0, float* dst, int width, int height, float sfx_clr, int sfx_saa) {
    const float inv_clr = 1.0f / sfx_clr;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Float4 A = GetData(pass0, width, height, x - 1, y - 1);
            Float4 B = GetData(pass0, width, height, x, y - 1);
            Float4 D = GetData(pass0, width, height, x - 1, y);
            Float4 E = GetData(pass0, width, height, x, y);
            Float4 F = GetData(pass0, width, height, x + 1, y);
            Float4 G = GetData(pass0, width, height, x - 1, y + 1);
            Float4 H = GetData(pass0, width, height, x, y + 1);
            Float4 I = GetData(pass0, width, height, x + 1, y + 1);

            const size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            dst[idx + 0] = StrScalar(D.z, D.w, E.y, A.w, D.y, sfx_clr, inv_clr, sfx_saa);
            dst[idx + 1] = StrScalar(F.x, E.w, E.y, B.w, F.y, sfx_clr, inv_clr, sfx_saa);
            dst[idx + 2] = StrScalar(H.z, E.w, H.y, H.w, I.y, sfx_clr, inv_clr, sfx_saa);
            dst[idx + 3] = StrScalar(H.x, D.w, H.y, G.w, G.y, sfx_clr, inv_clr, sfx_saa);
        }
    }
}

void Pass1Vector(const float* pass0, float* dst, int width, int height, float sfx_clr, int sfx_saa) {
    const VF zero = fzero();
    const VF one = fset1(1.0f);
    const VF two = fset1(2.0f);
    const VF clr = fset1(sfx_clr);
    const VF inv_clr = fset1(1.0f / sfx_clr);
    const bool saa_enabled = (sfx_saa == 1);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Load metric data for 3x3 neighborhood
            // Each metric is (dist_EA, dist_EB, dist_EC, dist_EF) = (x, y, z, w)
            Float4 A = GetData(pass0, width, height, x - 1, y - 1);
            Float4 B = GetData(pass0, width, height, x, y - 1);
            Float4 D = GetData(pass0, width, height, x - 1, y);
            Float4 E = GetData(pass0, width, height, x, y);
            Float4 F = GetData(pass0, width, height, x + 1, y);
            Float4 G = GetData(pass0, width, height, x - 1, y + 1);
            Float4 H = GetData(pass0, width, height, x, y + 1);
            Float4 I = GetData(pass0, width, height, x + 1, y + 1);

            // Corner x: str(D.z, D.w, E.y, A.w, D.y)
            // Corner y: str(F.x, E.w, E.y, B.w, F.y)
            // Corner z: str(H.z, E.w, H.y, H.w, I.y)
            // Corner w: str(H.x, D.w, H.y, G.w, G.y)
            const VF d_vec = fset(D.z, F.x, H.z, H.x);
            const VF a_x_vec = fset(D.w, E.w, E.w, D.w);
            const VF a_y_vec = fset(E.y, E.y, H.y, H.y);
            const VF b_x_vec = fset(A.w, B.w, H.w, G.w);
            const VF b_y_vec = fset(D.y, F.y, I.y, G.y);

            const VF diff = fsub(a_x_vec, a_y_vec);
            const VF wght1 = fmul(fmax(fsub(clr, d_vec), zero), inv_clr);

            const VF min_ax_bx = fmin(a_x_vec, b_x_vec);
            const VF min_ay_by = fmin(a_y_vec, b_y_vec);
            const VF sum1 = fadd(min_ax_bx, a_x_vec);
            const VF sum2 = fadd(min_ay_by, a_y_vec);
            const V cmp_mask = fcmpgt(sum1, sum2);
            const VF neg_diff = fsub(zero, diff);
            const VF cond_val = fselect(cmp_mask, diff, neg_diff);

            const VF wght2 = fmin(fmax(fadd(fsub(one, d_vec), cond_val), zero), one);

            VF strength = fmul(fmul(wght1, wght2), fmul(a_x_vec, a_y_vec));

            if (!saa_enabled) {
                const VF a_sum = fadd(a_x_vec, a_y_vec);
                const VF d2 = fmul(two, d_vec);
                strength = fand(strength, fcmplt(d2, a_sum));
            }

            fstore(dst + (static_cast<size_t>(y) * width + x) * 4, strength);
        }
    }
}

}  // namespace

void Pass1(const float* pass0, float* dst, int width, int height, float sfx_clr, int sfx_saa) {
    if (Enabled())
        Pass1Vector(pass0, dst, width, height, sfx_clr, sfx_saa);
    else
        Pass1Scalar(pass0, dst, width, height, sfx_clr, sfx_saa);
}

}  // namespace scalefx
