// ScaleFX Pass 3: Edge Level Determination
// Determines which edge level (1-6) is present and prepares subpixel tags
// Input: Pass 2 junction data
// Output: Subpixel tags packed as (crn + 9*mid) / 80
//
// Based on ScaleFX by Sp00kyFox, 2017-03-01
// CPU implementation: four pixels per lane group (SSE2 / NEON), or scalar

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "scalefx_internal.h"

namespace scalefx {

namespace {

// ---------------------------------------------------------------------------
// Scalar
// ---------------------------------------------------------------------------

// Helper struct for packed int4 values (bit flags)
struct Int4 {
    int x, y, z, w;
};

// Unpack Pass 2 data to integer bit flags
// Pass 2 packs as: (res + 2*hori + 4*vert + 8*or) / 15
// So we multiply by 15 and extract bits
inline Int4 UnpackPass2(Float4 v) {
    return {
        static_cast<int>(v.x * 15.0f + 0.5f),
        static_cast<int>(v.y * 15.0f + 0.5f),
        static_cast<int>(v.z * 15.0f + 0.5f),
        static_cast<int>(v.w * 15.0f + 0.5f)
    };
}

inline Bool4 GetCorn(Int4 p) { return { (p.x & 1) != 0, (p.y & 1) != 0, (p.z & 1) != 0, (p.w & 1) != 0 }; }
inline Bool4 GetHori(Int4 p) { return { (p.x & 2) != 0, (p.y & 2) != 0, (p.z & 2) != 0, (p.w & 2) != 0 }; }
inline Bool4 GetVert(Int4 p) { return { (p.x & 4) != 0, (p.y & 4) != 0, (p.z & 4) != 0, (p.w & 4) != 0 }; }
inline Bool4 GetOr(Int4 p) { return { (p.x & 8) != 0, (p.y & 8) != 0, (p.z & 8) != 0, (p.w & 8) != 0 }; }

void Pass3Scalar(const float* pass2, float* dst, int width, int height, int sfx_scn) {
    /*  grid        corners     mids
          B         x   y         x
        D E F                   w   y
          H         w   z         z
    */

    // Subpixel indices: 0=E, 1=D, 2=D0, 3=F, 4=F0, 5=B, 6=B0, 7=H, 8=H0

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Read Pass 2 data for extended neighborhood
            Float4 E_raw  = GetData(pass2, width, height, x,     y);
            Float4 D_raw  = GetData(pass2, width, height, x - 1, y);
            Float4 D0_raw = GetData(pass2, width, height, x - 2, y);
            Float4 D1_raw = GetData(pass2, width, height, x - 3, y);
            Float4 F_raw  = GetData(pass2, width, height, x + 1, y);
            Float4 F0_raw = GetData(pass2, width, height, x + 2, y);
            Float4 F1_raw = GetData(pass2, width, height, x + 3, y);
            Float4 B_raw  = GetData(pass2, width, height, x,     y - 1);
            Float4 B0_raw = GetData(pass2, width, height, x,     y - 2);
            Float4 B1_raw = GetData(pass2, width, height, x,     y - 3);
            Float4 H_raw  = GetData(pass2, width, height, x,     y + 1);
            Float4 H0_raw = GetData(pass2, width, height, x,     y + 2);
            Float4 H1_raw = GetData(pass2, width, height, x,     y + 3);

            // Unpack to integers once, then extract bits
            Int4 Ep = UnpackPass2(E_raw), Dp = UnpackPass2(D_raw), D0p = UnpackPass2(D0_raw), D1p = UnpackPass2(D1_raw);
            Int4 Fp = UnpackPass2(F_raw), F0p = UnpackPass2(F0_raw), F1p = UnpackPass2(F1_raw);
            Int4 Bp = UnpackPass2(B_raw), B0p = UnpackPass2(B0_raw), B1p = UnpackPass2(B1_raw);
            Int4 Hp = UnpackPass2(H_raw), H0p = UnpackPass2(H0_raw), H1p = UnpackPass2(H1_raw);

            // Extract flags from packed integers
            Bool4 Ec = GetCorn(Ep), Eh = GetHori(Ep), Ev = GetVert(Ep), Eo = GetOr(Ep);
            Bool4 Dc = GetCorn(Dp), Dh = GetHori(Dp), Do = GetOr(Dp);
            Bool4 D0c = GetCorn(D0p), D0h = GetHori(D0p), D1h = GetHori(D1p);
            Bool4 Fc = GetCorn(Fp), Fh = GetHori(Fp), Fo = GetOr(Fp);
            Bool4 F0c = GetCorn(F0p), F0h = GetHori(F0p), F1h = GetHori(F1p);
            Bool4 Bc = GetCorn(Bp), Bv = GetVert(Bp), Bo = GetOr(Bp);
            Bool4 B0c = GetCorn(B0p), B0v = GetVert(B0p), B1v = GetVert(B1p);
            Bool4 Hc = GetCorn(Hp), Hv = GetVert(Hp), Ho = GetOr(Hp);
            Bool4 H0c = GetCorn(H0p), H0v = GetVert(H0p), H1v = GetVert(H1p);

            // Level 1 corners (hori, vert)
            bool lvl1x = Ec.x && (Dc.z || Bc.z || sfx_scn == 1);
            bool lvl1y = Ec.y && (Fc.w || Bc.w || sfx_scn == 1);
            bool lvl1z = Ec.z && (Fc.x || Hc.x || sfx_scn == 1);
            bool lvl1w = Ec.w && (Dc.y || Hc.y || sfx_scn == 1);

            // Level 2 mid (left, right / up, down)
            bool lvl2x_0 = (Ec.x && Eh.y) && Dc.z;
            bool lvl2x_1 = (Ec.y && Eh.x) && Fc.w;
            bool lvl2y_0 = (Ec.y && Ev.z) && Bc.w;
            bool lvl2y_1 = (Ec.z && Ev.y) && Hc.x;
            bool lvl2z_0 = (Ec.w && Eh.z) && Dc.y;
            bool lvl2z_1 = (Ec.z && Eh.w) && Fc.x;
            bool lvl2w_0 = (Ec.x && Ev.w) && Bc.z;
            bool lvl2w_1 = (Ec.w && Ev.x) && Hc.y;

            // Level 3 corners (hori, vert)
            bool lvl3x_0 = lvl2x_1 && (Dh.y && Dh.x) && Fh.z;
            bool lvl3x_1 = lvl2w_1 && (Bv.w && Bv.x) && Hv.z;
            bool lvl3y_0 = lvl2x_0 && (Fh.x && Fh.y) && Dh.w;
            bool lvl3y_1 = lvl2y_1 && (Bv.z && Bv.y) && Hv.w;
            bool lvl3z_0 = lvl2z_0 && (Fh.w && Fh.z) && Dh.x;
            bool lvl3z_1 = lvl2y_0 && (Hv.y && Hv.z) && Bv.x;
            bool lvl3w_0 = lvl2z_1 && (Dh.z && Dh.w) && Fh.y;
            bool lvl3w_1 = lvl2w_0 && (Hv.x && Hv.w) && Bv.y;

            // Level 4 corners (hori, vert)
            bool lvl4x_0 = (Dc.x && Dh.y && Eh.x && Eh.y && Fh.x && Fh.y) && (D0c.z && D0h.w);
            bool lvl4x_1 = (Bc.x && Bv.w && Ev.x && Ev.w && Hv.x && Hv.w) && (B0c.z && B0v.y);
            bool lvl4y_0 = (Fc.y && Fh.x && Eh.y && Eh.x && Dh.y && Dh.x) && (F0c.w && F0h.z);
            bool lvl4y_1 = (Bc.y && Bv.z && Ev.y && Ev.z && Hv.y && Hv.z) && (B0c.w && B0v.x);
            bool lvl4z_0 = (Fc.z && Fh.w && Eh.z && Eh.w && Dh.z && Dh.w) && (F0c.x && F0h.y);
            bool lvl4z_1 = (Hc.z && Hv.y && Ev.z && Ev.y && Bv.z && Bv.y) && (H0c.x && H0v.w);
            bool lvl4w_0 = (Dc.w && Dh.z && Eh.w && Eh.z && Fh.w && Fh.z) && (D0c.y && D0h.x);
            bool lvl4w_1 = (Hc.w && Hv.x && Ev.w && Ev.x && Bv.w && Bv.x) && (H0c.y && H0v.z);

            // Level 5 mid (left, right / up, down)
            bool lvl5x_0 = lvl4x_0 && (F0h.x && F0h.y) && (D1h.z && D1h.w);
            bool lvl5x_1 = lvl4y_0 && (D0h.y && D0h.x) && (F1h.w && F1h.z);
            bool lvl5y_0 = lvl4y_1 && (H0v.y && H0v.z) && (B1v.w && B1v.x);
            bool lvl5y_1 = lvl4z_1 && (B0v.z && B0v.y) && (H1v.x && H1v.w);
            bool lvl5z_0 = lvl4w_0 && (F0h.w && F0h.z) && (D1h.y && D1h.x);
            bool lvl5z_1 = lvl4z_0 && (D0h.z && D0h.w) && (F1h.x && F1h.y);
            bool lvl5w_0 = lvl4x_1 && (H0v.x && H0v.w) && (B1v.z && B1v.y);
            bool lvl5w_1 = lvl4w_1 && (B0v.w && B0v.x) && (H1v.y && H1v.z);

            // Level 6 corners (hori, vert)
            bool lvl6x_0 = lvl5x_1 && (D1h.y && D1h.x);
            bool lvl6x_1 = lvl5w_1 && (B1v.w && B1v.x);
            bool lvl6y_0 = lvl5x_0 && (F1h.x && F1h.y);
            bool lvl6y_1 = lvl5y_1 && (B1v.z && B1v.y);
            bool lvl6z_0 = lvl5z_0 && (F1h.w && F1h.z);
            bool lvl6z_1 = lvl5y_0 && (H1v.y && H1v.z);
            bool lvl6w_0 = lvl5z_1 && (D1h.z && D1h.w);
            bool lvl6w_1 = lvl5w_0 && (H1v.x && H1v.w);

            // Calculate subpixel indices
            // Subpixels: 0=E, 1=D, 2=D0, 3=F, 4=F0, 5=B, 6=B0, 7=H, 8=H0
            Float4 crn, mid;

            // Corner x (top-left)
            crn.x = ((lvl1x && Eo.x) || (lvl3x_0 && Eo.y) || (lvl4x_0 && Do.x) || (lvl6x_0 && Fo.y)) ? 5.0f :
                    (lvl1x || (lvl3x_1 && !Eo.w) || (lvl4x_1 && !Bo.x) || (lvl6x_1 && !Ho.w)) ? 1.0f :
                    lvl3x_0 ? 3.0f : lvl3x_1 ? 7.0f : lvl4x_0 ? 2.0f : lvl4x_1 ? 6.0f : lvl6x_0 ? 4.0f : lvl6x_1 ? 8.0f : 0.0f;

            // Corner y (top-right)
            crn.y = ((lvl1y && Eo.y) || (lvl3y_0 && Eo.x) || (lvl4y_0 && Fo.y) || (lvl6y_0 && Do.x)) ? 5.0f :
                    (lvl1y || (lvl3y_1 && !Eo.z) || (lvl4y_1 && !Bo.y) || (lvl6y_1 && !Ho.z)) ? 3.0f :
                    lvl3y_0 ? 1.0f : lvl3y_1 ? 7.0f : lvl4y_0 ? 4.0f : lvl4y_1 ? 6.0f : lvl6y_0 ? 2.0f : lvl6y_1 ? 8.0f : 0.0f;

            // Corner z (bottom-right)
            crn.z = ((lvl1z && Eo.z) || (lvl3z_0 && Eo.w) || (lvl4z_0 && Fo.z) || (lvl6z_0 && Do.w)) ? 7.0f :
                    (lvl1z || (lvl3z_1 && !Eo.y) || (lvl4z_1 && !Ho.z) || (lvl6z_1 && !Bo.y)) ? 3.0f :
                    lvl3z_0 ? 1.0f : lvl3z_1 ? 5.0f : lvl4z_0 ? 4.0f : lvl4z_1 ? 8.0f : lvl6z_0 ? 2.0f : lvl6z_1 ? 6.0f : 0.0f;

            // Corner w (bottom-left)
            crn.w = ((lvl1w && Eo.w) || (lvl3w_0 && Eo.z) || (lvl4w_0 && Do.w) || (lvl6w_0 && Fo.z)) ? 7.0f :
                    (lvl1w || (lvl3w_1 && !Eo.x) || (lvl4w_1 && !Ho.w) || (lvl6w_1 && !Bo.x)) ? 1.0f :
                    lvl3w_0 ? 3.0f : lvl3w_1 ? 5.0f : lvl4w_0 ? 2.0f : lvl4w_1 ? 8.0f : lvl6w_0 ? 4.0f : lvl6w_1 ? 6.0f : 0.0f;

            // Mid x (top)
            mid.x = ((lvl2x_0 && Eo.x) || (lvl2x_1 && Eo.y) || (lvl5x_0 && Do.x) || (lvl5x_1 && Fo.y)) ? 5.0f :
                    lvl2x_0 ? 1.0f : lvl2x_1 ? 3.0f : lvl5x_0 ? 2.0f : lvl5x_1 ? 4.0f :
                    (Ec.x && Dc.z && Ec.y && Fc.w) ? (Eo.x ? (Eo.y ? 5.0f : 3.0f) : 1.0f) : 0.0f;

            // Mid y (right)
            mid.y = ((lvl2y_0 && !Eo.y) || (lvl2y_1 && !Eo.z) || (lvl5y_0 && !Bo.y) || (lvl5y_1 && !Ho.z)) ? 3.0f :
                    lvl2y_0 ? 5.0f : lvl2y_1 ? 7.0f : lvl5y_0 ? 6.0f : lvl5y_1 ? 8.0f :
                    (Ec.y && Bc.w && Ec.z && Hc.x) ? (!Eo.y ? (!Eo.z ? 3.0f : 7.0f) : 5.0f) : 0.0f;

            // Mid z (bottom)
            mid.z = ((lvl2z_0 && Eo.w) || (lvl2z_1 && Eo.z) || (lvl5z_0 && Do.w) || (lvl5z_1 && Fo.z)) ? 7.0f :
                    lvl2z_0 ? 1.0f : lvl2z_1 ? 3.0f : lvl5z_0 ? 2.0f : lvl5z_1 ? 4.0f :
                    (Ec.z && Fc.x && Ec.w && Dc.y) ? (Eo.z ? (Eo.w ? 7.0f : 1.0f) : 3.0f) : 0.0f;

            // Mid w (left)
            mid.w = ((lvl2w_0 && !Eo.x) || (lvl2w_1 && !Eo.w) || (lvl5w_0 && !Bo.x) || (lvl5w_1 && !Ho.w)) ? 1.0f :
                    lvl2w_0 ? 5.0f : lvl2w_1 ? 7.0f : lvl5w_0 ? 6.0f : lvl5w_1 ? 8.0f :
                    (Ec.w && Hc.y && Ec.x && Bc.z) ? (!Eo.w ? (!Eo.x ? 1.0f : 5.0f) : 7.0f) : 0.0f;

            // Pack output: (crn + 9 * mid) / 80
            const size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            dst[idx + 0] = (crn.x + 9.0f * mid.w) / 80.0f;  // TL corner + L mid
            dst[idx + 1] = (crn.y + 9.0f * mid.x) / 80.0f;  // TR corner + T mid
            dst[idx + 2] = (crn.z + 9.0f * mid.y) / 80.0f;  // BR corner + R mid
            dst[idx + 3] = (crn.w + 9.0f * mid.z) / 80.0f;  // BL corner + B mid
        }
    }
}

// ---------------------------------------------------------------------------
// Vector: lane i = pixel x + i, every bool a lane mask
// ---------------------------------------------------------------------------

struct Mask4 {
    V x, y, z, w;
};

// the packed Pass 2 flags of four pixels as integer lanes per field
struct Int4Lanes {
    V x, y, z, w;
};

VBAM_SIMD_INLINE V UnpackField(VF v) { return cvt_f_s32(fadd(fmul(v, fset1(15.0f)), fset1(0.5f))); }

VBAM_SIMD_INLINE Int4Lanes UnpackPass2V(const Float4Lanes& v) {
    return {UnpackField(v.x), UnpackField(v.y), UnpackField(v.z), UnpackField(v.w)};
}

VBAM_SIMD_INLINE Mask4 Bit(const Int4Lanes& p, uint32_t bit) {
    const V b = set1_u32(bit);
    return {NonZero(and_(p.x, b)), NonZero(and_(p.y, b)), NonZero(and_(p.z, b)), NonZero(and_(p.w, b))};
}

#define AND and_
#define OR or_
#define NOT not_

// nested  cond ? a : b  on floats
VBAM_SIMD_INLINE VF Sel(V cond, VF a, VF b) { return fselect(cond, a, b); }
VBAM_SIMD_INLINE VF Sel(V cond, float a, VF b) { return fselect(cond, fset1(a), b); }

void Pass3Vector(const float* pass2, float* dst, int width, int height, int sfx_scn) {
    PaddedF4Rows<3, 7> rows(pass2, width, height);
    const V scn = sfx_scn == 1 ? cmpeq_u32(zero(), zero()) : zero();
    const VF f0 = fzero();

    for (int y = 0; y < height; y++) {
        const float* r0 = rows.Row(y);
        const float* rB = rows.Row(y - 1);
        const float* rB0 = rows.Row(y - 2);
        const float* rB1 = rows.Row(y - 3);
        const float* rH = rows.Row(y + 1);
        const float* rH0 = rows.Row(y + 2);
        const float* rH1 = rows.Row(y + 3);

        for (int x = 0; x < width; x += 4) {
            const Int4Lanes Ep = UnpackPass2V(LoadLanes(r0 + 4 * x));
            const Int4Lanes Dp = UnpackPass2V(LoadLanes(r0 + 4 * (x - 1)));
            const Int4Lanes D0p = UnpackPass2V(LoadLanes(r0 + 4 * (x - 2)));
            const Int4Lanes D1p = UnpackPass2V(LoadLanes(r0 + 4 * (x - 3)));
            const Int4Lanes Fp = UnpackPass2V(LoadLanes(r0 + 4 * (x + 1)));
            const Int4Lanes F0p = UnpackPass2V(LoadLanes(r0 + 4 * (x + 2)));
            const Int4Lanes F1p = UnpackPass2V(LoadLanes(r0 + 4 * (x + 3)));
            const Int4Lanes Bp = UnpackPass2V(LoadLanes(rB + 4 * x));
            const Int4Lanes B0p = UnpackPass2V(LoadLanes(rB0 + 4 * x));
            const Int4Lanes B1p = UnpackPass2V(LoadLanes(rB1 + 4 * x));
            const Int4Lanes Hp = UnpackPass2V(LoadLanes(rH + 4 * x));
            const Int4Lanes H0p = UnpackPass2V(LoadLanes(rH0 + 4 * x));
            const Int4Lanes H1p = UnpackPass2V(LoadLanes(rH1 + 4 * x));

            const Mask4 Ec = Bit(Ep, 1), Eh = Bit(Ep, 2), Ev = Bit(Ep, 4), Eo = Bit(Ep, 8);
            const Mask4 Dc = Bit(Dp, 1), Dh = Bit(Dp, 2), Do = Bit(Dp, 8);
            const Mask4 D0c = Bit(D0p, 1), D0h = Bit(D0p, 2), D1h = Bit(D1p, 2);
            const Mask4 Fc = Bit(Fp, 1), Fh = Bit(Fp, 2), Fo = Bit(Fp, 8);
            const Mask4 F0c = Bit(F0p, 1), F0h = Bit(F0p, 2), F1h = Bit(F1p, 2);
            const Mask4 Bc = Bit(Bp, 1), Bv = Bit(Bp, 4), Bo = Bit(Bp, 8);
            const Mask4 B0c = Bit(B0p, 1), B0v = Bit(B0p, 4), B1v = Bit(B1p, 4);
            const Mask4 Hc = Bit(Hp, 1), Hv = Bit(Hp, 4), Ho = Bit(Hp, 8);
            const Mask4 H0c = Bit(H0p, 1), H0v = Bit(H0p, 4), H1v = Bit(H1p, 4);

            // Level 1 corners (hori, vert)
            const V lvl1x = AND(Ec.x, OR(OR(Dc.z, Bc.z), scn));
            const V lvl1y = AND(Ec.y, OR(OR(Fc.w, Bc.w), scn));
            const V lvl1z = AND(Ec.z, OR(OR(Fc.x, Hc.x), scn));
            const V lvl1w = AND(Ec.w, OR(OR(Dc.y, Hc.y), scn));

            // Level 2 mid (left, right / up, down)
            const V lvl2x_0 = AND(AND(Ec.x, Eh.y), Dc.z);
            const V lvl2x_1 = AND(AND(Ec.y, Eh.x), Fc.w);
            const V lvl2y_0 = AND(AND(Ec.y, Ev.z), Bc.w);
            const V lvl2y_1 = AND(AND(Ec.z, Ev.y), Hc.x);
            const V lvl2z_0 = AND(AND(Ec.w, Eh.z), Dc.y);
            const V lvl2z_1 = AND(AND(Ec.z, Eh.w), Fc.x);
            const V lvl2w_0 = AND(AND(Ec.x, Ev.w), Bc.z);
            const V lvl2w_1 = AND(AND(Ec.w, Ev.x), Hc.y);

            // Level 3 corners (hori, vert)
            const V lvl3x_0 = AND(AND(lvl2x_1, AND(Dh.y, Dh.x)), Fh.z);
            const V lvl3x_1 = AND(AND(lvl2w_1, AND(Bv.w, Bv.x)), Hv.z);
            const V lvl3y_0 = AND(AND(lvl2x_0, AND(Fh.x, Fh.y)), Dh.w);
            const V lvl3y_1 = AND(AND(lvl2y_1, AND(Bv.z, Bv.y)), Hv.w);
            const V lvl3z_0 = AND(AND(lvl2z_0, AND(Fh.w, Fh.z)), Dh.x);
            const V lvl3z_1 = AND(AND(lvl2y_0, AND(Hv.y, Hv.z)), Bv.x);
            const V lvl3w_0 = AND(AND(lvl2z_1, AND(Dh.z, Dh.w)), Fh.y);
            const V lvl3w_1 = AND(AND(lvl2w_0, AND(Hv.x, Hv.w)), Bv.y);

            // Level 4 corners (hori, vert)
            auto and6 = [](V a, V b, V c, V d, V e, V f) { return AND(AND(AND(a, b), AND(c, d)), AND(e, f)); };
            const V lvl4x_0 = AND(and6(Dc.x, Dh.y, Eh.x, Eh.y, Fh.x, Fh.y), AND(D0c.z, D0h.w));
            const V lvl4x_1 = AND(and6(Bc.x, Bv.w, Ev.x, Ev.w, Hv.x, Hv.w), AND(B0c.z, B0v.y));
            const V lvl4y_0 = AND(and6(Fc.y, Fh.x, Eh.y, Eh.x, Dh.y, Dh.x), AND(F0c.w, F0h.z));
            const V lvl4y_1 = AND(and6(Bc.y, Bv.z, Ev.y, Ev.z, Hv.y, Hv.z), AND(B0c.w, B0v.x));
            const V lvl4z_0 = AND(and6(Fc.z, Fh.w, Eh.z, Eh.w, Dh.z, Dh.w), AND(F0c.x, F0h.y));
            const V lvl4z_1 = AND(and6(Hc.z, Hv.y, Ev.z, Ev.y, Bv.z, Bv.y), AND(H0c.x, H0v.w));
            const V lvl4w_0 = AND(and6(Dc.w, Dh.z, Eh.w, Eh.z, Fh.w, Fh.z), AND(D0c.y, D0h.x));
            const V lvl4w_1 = AND(and6(Hc.w, Hv.x, Ev.w, Ev.x, Bv.w, Bv.x), AND(H0c.y, H0v.z));

            // Level 5 mid (left, right / up, down)
            const V lvl5x_0 = AND(AND(lvl4x_0, AND(F0h.x, F0h.y)), AND(D1h.z, D1h.w));
            const V lvl5x_1 = AND(AND(lvl4y_0, AND(D0h.y, D0h.x)), AND(F1h.w, F1h.z));
            const V lvl5y_0 = AND(AND(lvl4y_1, AND(H0v.y, H0v.z)), AND(B1v.w, B1v.x));
            const V lvl5y_1 = AND(AND(lvl4z_1, AND(B0v.z, B0v.y)), AND(H1v.x, H1v.w));
            const V lvl5z_0 = AND(AND(lvl4w_0, AND(F0h.w, F0h.z)), AND(D1h.y, D1h.x));
            const V lvl5z_1 = AND(AND(lvl4z_0, AND(D0h.z, D0h.w)), AND(F1h.x, F1h.y));
            const V lvl5w_0 = AND(AND(lvl4x_1, AND(H0v.x, H0v.w)), AND(B1v.z, B1v.y));
            const V lvl5w_1 = AND(AND(lvl4w_1, AND(B0v.w, B0v.x)), AND(H1v.y, H1v.z));

            // Level 6 corners (hori, vert)
            const V lvl6x_0 = AND(lvl5x_1, AND(D1h.y, D1h.x));
            const V lvl6x_1 = AND(lvl5w_1, AND(B1v.w, B1v.x));
            const V lvl6y_0 = AND(lvl5x_0, AND(F1h.x, F1h.y));
            const V lvl6y_1 = AND(lvl5y_1, AND(B1v.z, B1v.y));
            const V lvl6z_0 = AND(lvl5z_0, AND(F1h.w, F1h.z));
            const V lvl6z_1 = AND(lvl5y_0, AND(H1v.y, H1v.z));
            const V lvl6w_0 = AND(lvl5z_1, AND(D1h.z, D1h.w));
            const V lvl6w_1 = AND(lvl5w_0, AND(H1v.x, H1v.w));

            auto or4 = [](V a, V b, V c, V d) { return OR(OR(a, b), OR(c, d)); };

            // Corner x (top-left)
            const VF crnx = Sel(or4(AND(lvl1x, Eo.x), AND(lvl3x_0, Eo.y), AND(lvl4x_0, Do.x), AND(lvl6x_0, Fo.y)), 5.0f,
                            Sel(or4(lvl1x, AND(lvl3x_1, NOT(Eo.w)), AND(lvl4x_1, NOT(Bo.x)), AND(lvl6x_1, NOT(Ho.w))), 1.0f,
                            Sel(lvl3x_0, 3.0f, Sel(lvl3x_1, 7.0f, Sel(lvl4x_0, 2.0f, Sel(lvl4x_1, 6.0f,
                            Sel(lvl6x_0, 4.0f, Sel(lvl6x_1, 8.0f, f0))))))));

            // Corner y (top-right)
            const VF crny = Sel(or4(AND(lvl1y, Eo.y), AND(lvl3y_0, Eo.x), AND(lvl4y_0, Fo.y), AND(lvl6y_0, Do.x)), 5.0f,
                            Sel(or4(lvl1y, AND(lvl3y_1, NOT(Eo.z)), AND(lvl4y_1, NOT(Bo.y)), AND(lvl6y_1, NOT(Ho.z))), 3.0f,
                            Sel(lvl3y_0, 1.0f, Sel(lvl3y_1, 7.0f, Sel(lvl4y_0, 4.0f, Sel(lvl4y_1, 6.0f,
                            Sel(lvl6y_0, 2.0f, Sel(lvl6y_1, 8.0f, f0))))))));

            // Corner z (bottom-right)
            const VF crnz = Sel(or4(AND(lvl1z, Eo.z), AND(lvl3z_0, Eo.w), AND(lvl4z_0, Fo.z), AND(lvl6z_0, Do.w)), 7.0f,
                            Sel(or4(lvl1z, AND(lvl3z_1, NOT(Eo.y)), AND(lvl4z_1, NOT(Ho.z)), AND(lvl6z_1, NOT(Bo.y))), 3.0f,
                            Sel(lvl3z_0, 1.0f, Sel(lvl3z_1, 5.0f, Sel(lvl4z_0, 4.0f, Sel(lvl4z_1, 8.0f,
                            Sel(lvl6z_0, 2.0f, Sel(lvl6z_1, 6.0f, f0))))))));

            // Corner w (bottom-left)
            const VF crnw = Sel(or4(AND(lvl1w, Eo.w), AND(lvl3w_0, Eo.z), AND(lvl4w_0, Do.w), AND(lvl6w_0, Fo.z)), 7.0f,
                            Sel(or4(lvl1w, AND(lvl3w_1, NOT(Eo.x)), AND(lvl4w_1, NOT(Ho.w)), AND(lvl6w_1, NOT(Bo.x))), 1.0f,
                            Sel(lvl3w_0, 3.0f, Sel(lvl3w_1, 5.0f, Sel(lvl4w_0, 2.0f, Sel(lvl4w_1, 8.0f,
                            Sel(lvl6w_0, 4.0f, Sel(lvl6w_1, 6.0f, f0))))))));

            // Mid x (top)
            const VF midx = Sel(or4(AND(lvl2x_0, Eo.x), AND(lvl2x_1, Eo.y), AND(lvl5x_0, Do.x), AND(lvl5x_1, Fo.y)), 5.0f,
                            Sel(lvl2x_0, 1.0f, Sel(lvl2x_1, 3.0f, Sel(lvl5x_0, 2.0f, Sel(lvl5x_1, 4.0f,
                            Sel(AND(AND(Ec.x, Dc.z), AND(Ec.y, Fc.w)), Sel(Eo.x, Sel(Eo.y, 5.0f, fset1(3.0f)), fset1(1.0f)), f0))))));

            // Mid y (right)
            const VF midy = Sel(or4(AND(lvl2y_0, NOT(Eo.y)), AND(lvl2y_1, NOT(Eo.z)), AND(lvl5y_0, NOT(Bo.y)), AND(lvl5y_1, NOT(Ho.z))), 3.0f,
                            Sel(lvl2y_0, 5.0f, Sel(lvl2y_1, 7.0f, Sel(lvl5y_0, 6.0f, Sel(lvl5y_1, 8.0f,
                            Sel(AND(AND(Ec.y, Bc.w), AND(Ec.z, Hc.x)), Sel(NOT(Eo.y), Sel(NOT(Eo.z), 3.0f, fset1(7.0f)), fset1(5.0f)), f0))))));

            // Mid z (bottom)
            const VF midz = Sel(or4(AND(lvl2z_0, Eo.w), AND(lvl2z_1, Eo.z), AND(lvl5z_0, Do.w), AND(lvl5z_1, Fo.z)), 7.0f,
                            Sel(lvl2z_0, 1.0f, Sel(lvl2z_1, 3.0f, Sel(lvl5z_0, 2.0f, Sel(lvl5z_1, 4.0f,
                            Sel(AND(AND(Ec.z, Fc.x), AND(Ec.w, Dc.y)), Sel(Eo.z, Sel(Eo.w, 7.0f, fset1(1.0f)), fset1(3.0f)), f0))))));

            // Mid w (left)
            const VF midw = Sel(or4(AND(lvl2w_0, NOT(Eo.x)), AND(lvl2w_1, NOT(Eo.w)), AND(lvl5w_0, NOT(Bo.x)), AND(lvl5w_1, NOT(Ho.w))), 1.0f,
                            Sel(lvl2w_0, 5.0f, Sel(lvl2w_1, 7.0f, Sel(lvl5w_0, 6.0f, Sel(lvl5w_1, 8.0f,
                            Sel(AND(AND(Ec.w, Hc.y), AND(Ec.x, Bc.z)), Sel(NOT(Eo.w), Sel(NOT(Eo.x), 1.0f, fset1(5.0f)), fset1(7.0f)), f0))))));

            // Pack output: (crn + 9 * mid) / 80
            const VF nine = fset1(9.0f), eighty = fset1(80.0f);
            const VF o0 = fdiv(fadd(crnx, fmul(nine, midw)), eighty);
            const VF o1 = fdiv(fadd(crny, fmul(nine, midx)), eighty);
            const VF o2 = fdiv(fadd(crnz, fmul(nine, midy)), eighty);
            const VF o3 = fdiv(fadd(crnw, fmul(nine, midz)), eighty);

            float* out = dst + (static_cast<size_t>(y) * width + x) * 4;
            if (x + 4 <= width) {
                StoreLanes(out, o0, o1, o2, o3);
            } else {
                float tmp[16];
                StoreLanes(tmp, o0, o1, o2, o3);
                std::memcpy(out, tmp, (width - x) * 16);
            }
        }
    }
}

#undef AND
#undef OR
#undef NOT

}  // namespace

void Pass3(const float* pass2, float* dst, int width, int height, int sfx_scn) {
    if (Enabled())
        Pass3Vector(pass2, dst, width, height, sfx_scn);
    else
        Pass3Scalar(pass2, dst, width, height, sfx_scn);
}

}  // namespace scalefx
