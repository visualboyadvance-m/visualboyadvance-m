// ScaleFX Pass 2: Junction Conflict Resolution
// Resolves ambiguous configurations where multiple corners are active
// Input: Pass 0 metrics + Pass 1 strengths
// Output: Packed bit flags for corners, edges, and orientation
//
// Based on ScaleFX by Sp00kyFox, 2017-03-01
// CPU implementation: four pixels per lane group (SSE2 / NEON), or scalar

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "scalefx_internal.h"

namespace scalefx {

namespace {

constexpr float kInv15 = 1.0f / 15.0f;

// ---------------------------------------------------------------------------
// Scalar
// ---------------------------------------------------------------------------

// Boolean comparison functions (replacing float-based GLSL style)
inline bool LT(float x, float y) { return x < y; }
inline bool GE(float x, float y) { return x >= y; }
inline bool LEQ(float x, float y) { return x <= y; }

// Necessary but not sufficient junction condition for orthogonal edges
inline bool Clear(float crn_x, float crn_y, float a_x, float a_y, float b_x, float b_y) {
    bool cond1 = crn_x >= std::max(std::min(a_x, a_y), std::min(b_x, b_y));
    bool cond2 = crn_y >= std::max(std::min(a_x, b_y), std::min(b_x, a_y));
    return cond1 && cond2;
}

// Majority vote using boolean logic (unrolled)
inline Bool4 MajorityVote(const float jD[4]) {
    Bool4 result;
    result.x = GE(jD[0], 0.0f) && ((LEQ(jD[1], 0.0f) && LEQ(jD[3], 0.0f)) || GE(jD[0] + jD[2], jD[1] + jD[3]));
    result.y = GE(jD[1], 0.0f) && ((LEQ(jD[2], 0.0f) && LEQ(jD[0], 0.0f)) || GE(jD[1] + jD[3], jD[2] + jD[0]));
    result.z = GE(jD[2], 0.0f) && ((LEQ(jD[3], 0.0f) && LEQ(jD[1], 0.0f)) || GE(jD[2] + jD[0], jD[3] + jD[1]));
    result.w = GE(jD[3], 0.0f) && ((LEQ(jD[0], 0.0f) && LEQ(jD[2], 0.0f)) || GE(jD[3] + jD[1], jD[0] + jD[2]));
    return result;
}

void Pass2Scalar(const float* pass0, const float* pass1, float* dst, int width, int height) {
    /*  grid        metric      pattern
        A B C       x y z       x y
        D E F         o w       w z
        G H I
    */
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
            const size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            dst[idx + 0] = static_cast<float>(res.x + 2 * hori.x + 4 * vert.x + 8 * orient.x) * kInv15;
            dst[idx + 1] = static_cast<float>(res.y + 2 * hori.y + 4 * vert.y + 8 * orient.y) * kInv15;
            dst[idx + 2] = static_cast<float>(res.z + 2 * hori.z + 4 * vert.z + 8 * orient.z) * kInv15;
            dst[idx + 3] = static_cast<float>(res.w + 2 * hori.w + 4 * vert.w + 8 * orient.w) * kInv15;
        }
    }
}

// ---------------------------------------------------------------------------
// Vector: lane i = pixel x + i, every bool a lane mask
// ---------------------------------------------------------------------------

struct Mask4 {
    V x, y, z, w;
};

VBAM_SIMD_INLINE V GEv(VF a, VF b) { return fcmpge(a, b); }
VBAM_SIMD_INLINE V LTv(VF a, VF b) { return fcmplt(a, b); }
VBAM_SIMD_INLINE V LEQv(VF a, VF b) { return fcmple(a, b); }

VBAM_SIMD_INLINE V ClearV(VF crn_x, VF crn_y, VF a_x, VF a_y, VF b_x, VF b_y) {
    const V cond1 = fcmpge(crn_x, fmax(fmin(a_x, a_y), fmin(b_x, b_y)));
    const V cond2 = fcmpge(crn_y, fmax(fmin(a_x, b_y), fmin(b_x, a_y)));
    return and_(cond1, cond2);
}

VBAM_SIMD_INLINE Mask4 MajorityVoteV(const VF jD[4]) {
    const VF z = fzero();
    Mask4 r;
    r.x = and_(GEv(jD[0], z), or_(and_(LEQv(jD[1], z), LEQv(jD[3], z)), GEv(fadd(jD[0], jD[2]), fadd(jD[1], jD[3]))));
    r.y = and_(GEv(jD[1], z), or_(and_(LEQv(jD[2], z), LEQv(jD[0], z)), GEv(fadd(jD[1], jD[3]), fadd(jD[2], jD[0]))));
    r.z = and_(GEv(jD[2], z), or_(and_(LEQv(jD[3], z), LEQv(jD[1], z)), GEv(fadd(jD[2], jD[0]), fadd(jD[3], jD[1]))));
    r.w = and_(GEv(jD[3], z), or_(and_(LEQv(jD[0], z), LEQv(jD[2], z)), GEv(fadd(jD[3], jD[1]), fadd(jD[0], jD[2]))));
    return r;
}

// 2 * a - (b + c)
VBAM_SIMD_INLINE VF Dom(VF a, VF b, VF c) { return fsub(fmul(fset1(2.0f), a), fadd(b, c)); }

VBAM_SIMD_INLINE VF PackFlags(V res, V hori, V vert, V orient) {
    const V one = set1_u32(1);
    const V sum = or_(or_(and_(res, one), sll_u32<1>(and_(hori, one))), or_(sll_u32<2>(and_(vert, one)), sll_u32<3>(and_(orient, one))));
    return fmul(cvt_s32_f(sum), fset1(kInv15));
}

void Pass2Vector(const float* pass0, const float* pass1, float* dst, int width, int height) {
    PaddedF4Rows<1, 3> metric(pass0, width, height);
    PaddedF4Rows<1, 3> strength(pass1, width, height);
    const VF z = fzero();

    for (int y = 0; y < height; y++) {
        const float* m0 = metric.Row(y - 1);
        const float* m1 = metric.Row(y);
        const float* m2 = metric.Row(y + 1);
        const float* s0 = strength.Row(y - 1);
        const float* s1 = strength.Row(y);
        const float* s2 = strength.Row(y + 1);

        for (int x = 0; x < width; x += 4) {
            const Float4Lanes A = LoadLanes(m0 + 4 * (x - 1)), B = LoadLanes(m0 + 4 * x);
            const Float4Lanes D = LoadLanes(m1 + 4 * (x - 1)), E = LoadLanes(m1 + 4 * x), F = LoadLanes(m1 + 4 * (x + 1));
            const Float4Lanes G = LoadLanes(m2 + 4 * (x - 1)), H = LoadLanes(m2 + 4 * x), I = LoadLanes(m2 + 4 * (x + 1));

            const Float4Lanes As = LoadLanes(s0 + 4 * (x - 1)), Bs = LoadLanes(s0 + 4 * x), Cs = LoadLanes(s0 + 4 * (x + 1));
            const Float4Lanes Ds = LoadLanes(s1 + 4 * (x - 1)), Es = LoadLanes(s1 + 4 * x), Fs = LoadLanes(s1 + 4 * (x + 1));
            const Float4Lanes Gs = LoadLanes(s2 + 4 * (x - 1)), Hs = LoadLanes(s2 + 4 * x), Is = LoadLanes(s2 + 4 * (x + 1));

            // Strength junctions
            const VF jSx[4] = {As.z, Bs.w, Es.x, Ds.y};
            const VF jSy[4] = {Bs.z, Cs.w, Fs.x, Es.y};
            const VF jSz[4] = {Es.z, Fs.w, Is.x, Hs.y};
            const VF jSw[4] = {Ds.z, Es.w, Hs.x, Gs.y};

            // Dominance values
            const VF jDx[4] = {Dom(As.z, As.y, As.w), Dom(Bs.w, Bs.z, Bs.x), Dom(Es.x, Es.w, Es.y), Dom(Ds.y, Ds.x, Ds.z)};
            const VF jDy[4] = {Dom(Bs.z, Bs.y, Bs.w), Dom(Cs.w, Cs.z, Cs.x), Dom(Fs.x, Fs.w, Fs.y), Dom(Es.y, Es.x, Es.z)};
            const VF jDz[4] = {Dom(Es.z, Es.y, Es.w), Dom(Fs.w, Fs.z, Fs.x), Dom(Is.x, Is.w, Is.y), Dom(Hs.y, Hs.x, Hs.z)};
            const VF jDw[4] = {Dom(Ds.z, Ds.y, Ds.w), Dom(Es.w, Es.z, Es.x), Dom(Hs.x, Hs.w, Hs.y), Dom(Gs.y, Gs.x, Gs.z)};

            const Mask4 jx = MajorityVoteV(jDx);
            const Mask4 jy = MajorityVoteV(jDy);
            const Mask4 jz = MajorityVoteV(jDz);
            const Mask4 jw = MajorityVoteV(jDw);

            // Inject strength without creating new contradictions
            Mask4 res;
            res.x = or_(jx.z, and_(and_(and_(not_(jx.y), not_(jx.w)), GEv(jSx[2], z)),
                                   or_(jx.x, GEv(fadd(jSx[0], jSx[2]), fadd(jSx[1], jSx[3])))));
            res.y = or_(jy.w, and_(and_(and_(not_(jy.z), not_(jy.x)), GEv(jSy[3], z)),
                                   or_(jy.y, GEv(fadd(jSy[1], jSy[3]), fadd(jSy[0], jSy[2])))));
            res.z = or_(jz.x, and_(and_(and_(not_(jz.w), not_(jz.y)), GEv(jSz[0], z)),
                                   or_(jz.z, GEv(fadd(jSz[0], jSz[2]), fadd(jSz[1], jSz[3])))));
            res.w = or_(jw.y, and_(and_(and_(not_(jw.x), not_(jw.z)), GEv(jSw[1], z)),
                                   or_(jw.w, GEv(fadd(jSw[1], jSw[3]), fadd(jSw[0], jSw[2])))));

            // Single pixel & end of line detection (sequential, as in the scalar code)
            res.x = and_(res.x, or_(jx.z, not_(and_(res.w, res.y))));
            res.y = and_(res.y, or_(jy.w, not_(and_(res.x, res.z))));
            res.z = and_(res.z, or_(jz.x, not_(and_(res.y, res.w))));
            res.w = and_(res.w, or_(jw.y, not_(and_(res.z, res.x))));

            // Clear edge conditions
            Mask4 clr;
            clr.x = ClearV(D.z, E.x, D.w, E.y, A.w, D.y);
            clr.y = ClearV(F.x, E.z, E.w, E.y, B.w, F.y);
            clr.z = ClearV(H.z, I.x, E.w, H.y, H.w, I.y);
            clr.w = ClearV(H.x, G.z, D.w, H.y, G.w, G.y);

            // Horizontal and vertical edge calculations
            const VF hx = fmin(D.w, A.w), hy = fmin(E.w, B.w);
            const VF hz = fmin(E.w, H.w), hw = fmin(D.w, G.w);
            const VF vx = fmin(E.y, D.y), vy = fmin(E.y, F.y);
            const VF vz = fmin(H.y, I.y), vw = fmin(H.y, G.y);

            Mask4 orient;
            orient.x = GEv(fadd(hx, D.w), fadd(vx, E.y));
            orient.y = GEv(fadd(hy, E.w), fadd(vy, E.y));
            orient.z = GEv(fadd(hz, E.w), fadd(vz, H.y));
            orient.w = GEv(fadd(hw, D.w), fadd(vw, H.y));

            Mask4 hori;
            hori.x = and_(LTv(hx, vx), clr.x);
            hori.y = and_(LTv(hy, vy), clr.y);
            hori.z = and_(LTv(hz, vz), clr.z);
            hori.w = and_(LTv(hw, vw), clr.w);

            Mask4 vert;
            vert.x = and_(GEv(hx, vx), clr.x);
            vert.y = and_(GEv(hy, vy), clr.y);
            vert.z = and_(GEv(hz, vz), clr.z);
            vert.w = and_(GEv(hw, vw), clr.w);

            const VF ox = PackFlags(res.x, hori.x, vert.x, orient.x);
            const VF oy = PackFlags(res.y, hori.y, vert.y, orient.y);
            const VF oz = PackFlags(res.z, hori.z, vert.z, orient.z);
            const VF ow = PackFlags(res.w, hori.w, vert.w, orient.w);

            float* out = dst + (static_cast<size_t>(y) * width + x) * 4;
            if (x + 4 <= width) {
                StoreLanes(out, ox, oy, oz, ow);
            } else {
                float tmp[16];
                StoreLanes(tmp, ox, oy, oz, ow);
                std::memcpy(out, tmp, (width - x) * 16);
            }
        }
    }
}

}  // namespace

void Pass2(const float* pass0, const float* pass1, float* dst, int width, int height) {
    if (Enabled())
        Pass2Vector(pass0, pass1, dst, width, height);
    else
        Pass2Scalar(pass0, pass1, dst, width, height);
}

}  // namespace scalefx
