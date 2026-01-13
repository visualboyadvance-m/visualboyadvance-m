// ScaleFX Pass 3: Edge Level Determination
// Determines which edge level (1-6) is present and prepares subpixel tags
// Input: Pass 2 junction data
// Output: Subpixel tags packed as (crn + 9*mid) / 80
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

// Helper struct for bool4 values
struct Bool4 {
    bool x, y, z, w;
};

// Get data at position with boundary clamping
static inline Float4 GetData(const float* buf, int width, int height, int px, int py) {
    px = (px < 0) ? 0 : (px >= width) ? width - 1 : px;
    py = (py < 0) ? 0 : (py >= height) ? height - 1 : py;
    int idx = (py * width + px) * 4;
    return { buf[idx + 0], buf[idx + 1], buf[idx + 2], buf[idx + 3] };
}

// Helper struct for packed int4 values (bit flags)
struct Int4 {
    int x, y, z, w;
};

// Unpack Pass 2 data to integer bit flags
// Pass 2 packs as: (res + 2*hori + 4*vert + 8*or) / 15
// So we multiply by 15 and extract bits
static inline Int4 UnpackPass2(Float4 v) {
    return {
        static_cast<int>(v.x * 15.0f + 0.5f),
        static_cast<int>(v.y * 15.0f + 0.5f),
        static_cast<int>(v.z * 15.0f + 0.5f),
        static_cast<int>(v.w * 15.0f + 0.5f)
    };
}

// Extract corner flags from packed int (bit 0)
static inline Bool4 GetCorn(Int4 p) {
    return { (p.x & 1) != 0, (p.y & 1) != 0, (p.z & 1) != 0, (p.w & 1) != 0 };
}

// Extract horizontal edge flags from packed int (bit 1)
static inline Bool4 GetHori(Int4 p) {
    return { (p.x & 2) != 0, (p.y & 2) != 0, (p.z & 2) != 0, (p.w & 2) != 0 };
}

// Extract vertical edge flags from packed int (bit 2)
static inline Bool4 GetVert(Int4 p) {
    return { (p.x & 4) != 0, (p.y & 4) != 0, (p.z & 4) != 0, (p.w & 4) != 0 };
}

// Extract orientation flags from packed int (bit 3)
static inline Bool4 GetOr(Int4 p) {
    return { (p.x & 8) != 0, (p.y & 8) != 0, (p.z & 8) != 0, (p.w & 8) != 0 };
}

void Pass3(const float* pass2, float* dst, int width, int height, int sfx_scn) {
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
            crn.x = (lvl1x && Eo.x || lvl3x_0 && Eo.y || lvl4x_0 && Do.x || lvl6x_0 && Fo.y) ? 5.0f :
                    (lvl1x || lvl3x_1 && !Eo.w || lvl4x_1 && !Bo.x || lvl6x_1 && !Ho.w) ? 1.0f :
                    lvl3x_0 ? 3.0f : lvl3x_1 ? 7.0f : lvl4x_0 ? 2.0f : lvl4x_1 ? 6.0f : lvl6x_0 ? 4.0f : lvl6x_1 ? 8.0f : 0.0f;

            // Corner y (top-right)
            crn.y = (lvl1y && Eo.y || lvl3y_0 && Eo.x || lvl4y_0 && Fo.y || lvl6y_0 && Do.x) ? 5.0f :
                    (lvl1y || lvl3y_1 && !Eo.z || lvl4y_1 && !Bo.y || lvl6y_1 && !Ho.z) ? 3.0f :
                    lvl3y_0 ? 1.0f : lvl3y_1 ? 7.0f : lvl4y_0 ? 4.0f : lvl4y_1 ? 6.0f : lvl6y_0 ? 2.0f : lvl6y_1 ? 8.0f : 0.0f;

            // Corner z (bottom-right)
            crn.z = (lvl1z && Eo.z || lvl3z_0 && Eo.w || lvl4z_0 && Fo.z || lvl6z_0 && Do.w) ? 7.0f :
                    (lvl1z || lvl3z_1 && !Eo.y || lvl4z_1 && !Ho.z || lvl6z_1 && !Bo.y) ? 3.0f :
                    lvl3z_0 ? 1.0f : lvl3z_1 ? 5.0f : lvl4z_0 ? 4.0f : lvl4z_1 ? 8.0f : lvl6z_0 ? 2.0f : lvl6z_1 ? 6.0f : 0.0f;

            // Corner w (bottom-left)
            crn.w = (lvl1w && Eo.w || lvl3w_0 && Eo.z || lvl4w_0 && Do.w || lvl6w_0 && Fo.z) ? 7.0f :
                    (lvl1w || lvl3w_1 && !Eo.x || lvl4w_1 && !Ho.w || lvl6w_1 && !Bo.x) ? 1.0f :
                    lvl3w_0 ? 3.0f : lvl3w_1 ? 5.0f : lvl4w_0 ? 2.0f : lvl4w_1 ? 8.0f : lvl6w_0 ? 4.0f : lvl6w_1 ? 6.0f : 0.0f;

            // Mid x (top)
            mid.x = (lvl2x_0 && Eo.x || lvl2x_1 && Eo.y || lvl5x_0 && Do.x || lvl5x_1 && Fo.y) ? 5.0f :
                    lvl2x_0 ? 1.0f : lvl2x_1 ? 3.0f : lvl5x_0 ? 2.0f : lvl5x_1 ? 4.0f :
                    (Ec.x && Dc.z && Ec.y && Fc.w) ? (Eo.x ? (Eo.y ? 5.0f : 3.0f) : 1.0f) : 0.0f;

            // Mid y (right)
            mid.y = (lvl2y_0 && !Eo.y || lvl2y_1 && !Eo.z || lvl5y_0 && !Bo.y || lvl5y_1 && !Ho.z) ? 3.0f :
                    lvl2y_0 ? 5.0f : lvl2y_1 ? 7.0f : lvl5y_0 ? 6.0f : lvl5y_1 ? 8.0f :
                    (Ec.y && Bc.w && Ec.z && Hc.x) ? (!Eo.y ? (!Eo.z ? 3.0f : 7.0f) : 5.0f) : 0.0f;

            // Mid z (bottom)
            mid.z = (lvl2z_0 && Eo.w || lvl2z_1 && Eo.z || lvl5z_0 && Do.w || lvl5z_1 && Fo.z) ? 7.0f :
                    lvl2z_0 ? 1.0f : lvl2z_1 ? 3.0f : lvl5z_0 ? 2.0f : lvl5z_1 ? 4.0f :
                    (Ec.z && Fc.x && Ec.w && Dc.y) ? (Eo.z ? (Eo.w ? 7.0f : 1.0f) : 3.0f) : 0.0f;

            // Mid w (left)
            mid.w = (lvl2w_0 && !Eo.x || lvl2w_1 && !Eo.w || lvl5w_0 && !Bo.x || lvl5w_1 && !Ho.w) ? 1.0f :
                    lvl2w_0 ? 5.0f : lvl2w_1 ? 7.0f : lvl5w_0 ? 6.0f : lvl5w_1 ? 8.0f :
                    (Ec.w && Hc.y && Ec.x && Bc.z) ? (!Eo.w ? (!Eo.x ? 1.0f : 5.0f) : 7.0f) : 0.0f;

            // Pack output: (crn + 9 * mid) / 80
            int idx = (y * width + x) * 4;
            dst[idx + 0] = (crn.x + 9.0f * mid.w) / 80.0f;  // TL corner + L mid
            dst[idx + 1] = (crn.y + 9.0f * mid.x) / 80.0f;  // TR corner + T mid
            dst[idx + 2] = (crn.z + 9.0f * mid.y) / 80.0f;  // BR corner + R mid
            dst[idx + 3] = (crn.w + 9.0f * mid.z) / 80.0f;  // BL corner + B mid
        }
    }
}

} // namespace scalefx
