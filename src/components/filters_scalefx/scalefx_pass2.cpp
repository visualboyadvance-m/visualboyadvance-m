// ScaleFX Pass 2: Junction Conflict Resolution
// Resolves ambiguous configurations where multiple corners are active
// Input: Pass 0 metrics + Pass 1 strengths
// Output: Packed bit flags for corners, edges, and orientation
//
// Based on ScaleFX by Sp00kyFox, 2017-03-01
// CPU implementation

#include "scalefx_simd.h"
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace scalefx {

// Helper struct for float4 values
struct Float4 {
    float x, y, z, w;
};

// Get data at position with boundary clamping
static inline Float4 GetData(const float* buf, int width, int height, int px, int py) {
    px = (px < 0) ? 0 : (px >= width) ? width - 1 : px;
    py = (py < 0) ? 0 : (py >= height) ? height - 1 : py;
    int idx = (py * width + px) * 4;
    return { buf[idx + 0], buf[idx + 1], buf[idx + 2], buf[idx + 3] };
}

// Boolean comparison functions (replacing float-based GLSL style)
static inline bool LT(float x, float y) { return x < y; }
static inline bool GE(float x, float y) { return x >= y; }
static inline bool LEQ(float x, float y) { return x <= y; }

// Necessary but not sufficient junction condition for orthogonal edges
static inline bool Clear(float crn_x, float crn_y, float a_x, float a_y, float b_x, float b_y) {
    bool cond1 = crn_x >= std::max(std::min(a_x, a_y), std::min(b_x, b_y));
    bool cond2 = crn_y >= std::max(std::min(a_x, b_y), std::min(b_x, a_y));
    return cond1 && cond2;
}

// Helper struct for bool4 values
struct Bool4 {
    bool x, y, z, w;
};

void Pass2(const float* pass0, const float* pass1, float* dst, int width, int height) {
    /*  grid        metric      pattern
        A B C       x y z       x y
        D E F         o w       w z
        G H I
    */

    // Precompute 1/15 for output packing
    const float inv15 = 1.0f / 15.0f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Get metric data (Pass 0 output)
            Float4 A = GetData(pass0, width, height, x - 1, y - 1);
            Float4 B = GetData(pass0, width, height, x,     y - 1);
            Float4 D = GetData(pass0, width, height, x - 1, y);
            Float4 E = GetData(pass0, width, height, x,     y);
            Float4 F = GetData(pass0, width, height, x + 1, y);
            Float4 G = GetData(pass0, width, height, x - 1, y + 1);
            Float4 H = GetData(pass0, width, height, x,     y + 1);
            Float4 I = GetData(pass0, width, height, x + 1, y + 1);

            // Get strength data (Pass 1 output)
            Float4 As = GetData(pass1, width, height, x - 1, y - 1);
            Float4 Bs = GetData(pass1, width, height, x,     y - 1);
            Float4 Cs = GetData(pass1, width, height, x + 1, y - 1);
            Float4 Ds = GetData(pass1, width, height, x - 1, y);
            Float4 Es = GetData(pass1, width, height, x,     y);
            Float4 Fs = GetData(pass1, width, height, x + 1, y);
            Float4 Gs = GetData(pass1, width, height, x - 1, y + 1);
            Float4 Hs = GetData(pass1, width, height, x,     y + 1);
            Float4 Is = GetData(pass1, width, height, x + 1, y + 1);

            // Strength junctions
            Float4 jSx = { As.z, Bs.w, Es.x, Ds.y };
            Float4 jSy = { Bs.z, Cs.w, Fs.x, Es.y };
            Float4 jSz = { Es.z, Fs.w, Is.x, Hs.y };
            Float4 jSw = { Ds.z, Es.w, Hs.x, Gs.y };

            // Dominance values
            float jDx[4] = {
                2.0f * As.z - (As.y + As.w),
                2.0f * Bs.w - (Bs.z + Bs.x),
                2.0f * Es.x - (Es.w + Es.y),
                2.0f * Ds.y - (Ds.x + Ds.z)
            };
            float jDy[4] = {
                2.0f * Bs.z - (Bs.y + Bs.w),
                2.0f * Cs.w - (Cs.z + Cs.x),
                2.0f * Fs.x - (Fs.w + Fs.y),
                2.0f * Es.y - (Es.x + Es.z)
            };
            float jDz[4] = {
                2.0f * Es.z - (Es.y + Es.w),
                2.0f * Fs.w - (Fs.z + Fs.x),
                2.0f * Is.x - (Is.w + Is.y),
                2.0f * Hs.y - (Hs.x + Hs.z)
            };
            float jDw[4] = {
                2.0f * Ds.z - (Ds.y + Ds.w),
                2.0f * Es.w - (Es.z + Es.x),
                2.0f * Hs.x - (Hs.w + Hs.y),
                2.0f * Gs.y - (Gs.x + Gs.z)
            };

            // Majority vote using boolean logic (unrolled)
            auto MajorityVote = [](float jD[4]) -> Bool4 {
                // jD_yzwx = {jD[1], jD[2], jD[3], jD[0]}
                // jD_wxyz = {jD[3], jD[0], jD[1], jD[2]}
                // jD_zwxy = {jD[2], jD[3], jD[0], jD[1]}
                Bool4 result;
                result.x = GE(jD[0], 0.0f) && (LEQ(jD[1], 0.0f) && LEQ(jD[3], 0.0f) || GE(jD[0] + jD[2], jD[1] + jD[3]));
                result.y = GE(jD[1], 0.0f) && (LEQ(jD[2], 0.0f) && LEQ(jD[0], 0.0f) || GE(jD[1] + jD[3], jD[2] + jD[0]));
                result.z = GE(jD[2], 0.0f) && (LEQ(jD[3], 0.0f) && LEQ(jD[1], 0.0f) || GE(jD[2] + jD[0], jD[3] + jD[1]));
                result.w = GE(jD[3], 0.0f) && (LEQ(jD[0], 0.0f) && LEQ(jD[2], 0.0f) || GE(jD[3] + jD[1], jD[0] + jD[2]));
                return result;
            };

            Bool4 jx = MajorityVote(jDx);
            Bool4 jy = MajorityVote(jDy);
            Bool4 jz = MajorityVote(jDz);
            Bool4 jw = MajorityVote(jDw);

            // Inject strength without creating new contradictions
            Bool4 res;
            res.x = jx.z || (!jx.y && !jx.w && GE(jSx.z, 0.0f) && (jx.x || GE(jSx.x + jSx.z, jSx.y + jSx.w)));
            res.y = jy.w || (!jy.z && !jy.x && GE(jSy.w, 0.0f) && (jy.y || GE(jSy.y + jSy.w, jSy.x + jSy.z)));
            res.z = jz.x || (!jz.w && !jz.y && GE(jSz.x, 0.0f) && (jz.z || GE(jSz.x + jSz.z, jSz.y + jSz.w)));
            res.w = jw.y || (!jw.x && !jw.z && GE(jSw.y, 0.0f) && (jw.w || GE(jSw.y + jSw.w, jSw.x + jSw.z)));

            // Single pixel & end of line detection
            res.x = res.x && (jx.z || !(res.w && res.y));
            res.y = res.y && (jy.w || !(res.x && res.z));
            res.z = res.z && (jz.x || !(res.y && res.w));
            res.w = res.w && (jw.y || !(res.z && res.x));

            // Clear edge conditions
            Bool4 clr;
            clr.x = Clear(D.z, E.x, D.w, E.y, A.w, D.y);
            clr.y = Clear(F.x, E.z, E.w, E.y, B.w, F.y);
            clr.z = Clear(H.z, I.x, E.w, H.y, H.w, I.y);
            clr.w = Clear(H.x, G.z, D.w, H.y, G.w, G.y);

            // Horizontal and vertical edge calculations
            float hx = std::min(D.w, A.w), hy = std::min(E.w, B.w);
            float hz = std::min(E.w, H.w), hw = std::min(D.w, G.w);
            float vx = std::min(E.y, D.y), vy = std::min(E.y, F.y);
            float vz = std::min(H.y, I.y), vw = std::min(H.y, G.y);

            // Orientation: horizontal vs vertical (boolean)
            Bool4 orient;
            orient.x = GE(hx + D.w, vx + E.y);
            orient.y = GE(hy + E.w, vy + E.y);
            orient.z = GE(hz + E.w, vz + H.y);
            orient.w = GE(hw + D.w, vw + H.y);

            // Horizontal edges (where h < v and clear)
            Bool4 hori;
            hori.x = LT(hx, vx) && clr.x;
            hori.y = LT(hy, vy) && clr.y;
            hori.z = LT(hz, vz) && clr.z;
            hori.w = LT(hw, vw) && clr.w;

            // Vertical edges (where h >= v and clear)
            Bool4 vert;
            vert.x = GE(hx, vx) && clr.x;
            vert.y = GE(hy, vy) && clr.y;
            vert.z = GE(hz, vz) && clr.z;
            vert.w = GE(hw, vw) && clr.w;

            // Pack output: (res + 2*hori + 4*vert + 8*or) / 15
            int idx = (y * width + x) * 4;
            dst[idx + 0] = static_cast<float>(res.x + 2 * hori.x + 4 * vert.x + 8 * orient.x) * inv15;
            dst[idx + 1] = static_cast<float>(res.y + 2 * hori.y + 4 * vert.y + 8 * orient.y) * inv15;
            dst[idx + 2] = static_cast<float>(res.z + 2 * hori.z + 4 * vert.z + 8 * orient.z) * inv15;
            dst[idx + 3] = static_cast<float>(res.w + 2 * hori.w + 4 * vert.w + 8 * orient.w) * inv15;
        }
    }
}

} // namespace scalefx
