// ****************************************************************************
// * This file is part of the xBRZ project. It is distributed under           *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0         *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved          *
// *                                                                          *
// * Additionally and as a special exception, the author gives permission     *
// * to link the code of this program with the following libraries            *
// * (or with modified versions that use the same licenses), and distribute   *
// * linked combinations including the two: MAME, FreeFileSync, Snes9x, ePSXe *
// *                                                                          *
// * You must obey the GNU General Public License in all respects for all of  *
// * the code used other than MAME, FreeFileSync, Snes9x, ePSXe.              *
// * If you modify this file, you may extend this exception to your version   *
// * of the file, but you are not obligated to do so. If you do not wish to   *
// * do so, delete this exception statement from your version.                *
// ****************************************************************************

// VBA-M: vectorized rework of xBRZ 1.9 (SSE2 / NEON / scalar, see xbrz_simd.h).
//
// What changed against the upstream scaler, and why:
//
// - Colour distances are computed, four pixel pairs at a time, instead of
//   looked up in the 64 MB float table that upstream builds on first use
//   (distYCbCrBuffered). The table halved the precision of every channel
//   difference to fit; the vector path evaluates the same BT.2020 Y'CbCr
//   formula exactly, in single precision, as upstream's own
//   "argbUnbuffered" mode and the former 9x scaler already did. So blend
//   decisions on a few borderline pixels differ from the table-driven build,
//   and the three ColorFormat values now share one distance.
//
// - Corner preprocessing runs over a whole source row at once with one pixel
//   per lane: the 4x4 kernel entries are unaligned loads from four padded
//   copies of the source rows, so the ten distances per corner become ten
//   vector distances per four corners. The flat-area early-out of upstream
//   survives as a lane mask.
//
// - For a pixel that blends, the 24 distances any of the four rotations can
//   ask for are computed once (six vector distances) into a table that the
//   rotated blend step indexes at compile time.
//
// - Gradient blends run on the four channels as 16-bit lanes; block fills
//   use vector stores.
//
// - The former separate 9x scaler (xbrz9x.cpp) is folded in as Scaler9x; it
//   had drifted on details such as truncating instead of rounding gradients.

#include "xbrz.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>
#include <vector>
#include "xbrz_simd.h"
#include "xbrz_tools.h"

using namespace xbrz;


namespace
{
#ifdef _MSC_VER
    #define FORCE_INLINE __forceinline
#elif defined __GNUC__
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif

using simd::U32x4;
using simd::F32x4;


//blend front color with opacity M / N over opaque background: https://en.wikipedia.org/wiki/Alpha_compositing
//Limitation: alpha should be applied in gamma-decoded linear RGB space: https://ssp.impulsetrain.com/gamma-premult.html
template <unsigned int M, unsigned int N> FORCE_INLINE
uint32_t gradientRGB(uint32_t pixFront, uint32_t pixBack)
{
    //upstream: makePixel(r, g, b) => the unused upper byte comes out as zero
    return simd::packPixel(simd::gradient<M, N>(simd::unpackPixel(pixFront), simd::unpackPixel(pixBack))) & 0x00ffffff;
}


//find intermediate color between two colors with alpha channels (=> NO alpha blending!!!)
//Limitation: alpha should be applied in gamma-decoded linear RGB space: https://ssp.impulsetrain.com/gamma-premult.html
template <unsigned int M, unsigned int N> inline
uint32_t gradientARGB(uint32_t pixFront, uint32_t pixBack)
{
    static_assert(0 < M && M < N && N <= 1000);

    const unsigned int weightFront = getAlpha(pixFront) * M;
    const unsigned int weightBack  = getAlpha(pixBack) * (N - M);
    const unsigned int weightSum   = weightFront + weightBack;
    if (weightSum == 0)
        return 0;

    auto calcColor = [=](unsigned char colFront, unsigned char colBack)
    {
        return static_cast<unsigned char>(uintDivRound(colFront * weightFront + colBack * weightBack, weightSum));
    };

    return makePixel(static_cast<unsigned char>(uintDivRound(weightSum, N)),
                     calcColor(getRed  (pixFront), getRed  (pixBack)),
                     calcColor(getGreen(pixFront), getGreen(pixBack)),
                     calcColor(getBlue (pixFront), getBlue (pixBack)));
}


enum RotationDegree //clock-wise
{
    ROT_0,
    ROT_90,
    ROT_180,
    ROT_270
};

//calculate input matrix coordinates after rotation at compile time
template <RotationDegree rotDeg, size_t I, size_t J, size_t N>
struct MatrixRotation;

template <size_t I, size_t J, size_t N>
struct MatrixRotation<ROT_0, I, J, N>
{
    static const size_t I_old = I;
    static const size_t J_old = J;
};

template <RotationDegree rotDeg, size_t I, size_t J, size_t N> //(i, j) = (row, col) indices, N = size of (square) matrix
struct MatrixRotation
{
    static const size_t I_old = N - 1 - MatrixRotation<static_cast<RotationDegree>(rotDeg - 1), I, J, N>::J_old; //old coordinates before rotation!
    static const size_t J_old =         MatrixRotation<static_cast<RotationDegree>(rotDeg - 1), I, J, N>::I_old; //
};


template <size_t N, RotationDegree rotDeg>
class OutputMatrix
{
public:
    OutputMatrix(uint32_t* out, int outWidth) : //access matrix area, top-left at position "out" for image with given width
        out_(out),
        outWidth_(outWidth) {}

    template <size_t I, size_t J>
    uint32_t& ref() const
    {
        static const size_t I_old = MatrixRotation<rotDeg, I, J, N>::I_old;
        static const size_t J_old = MatrixRotation<rotDeg, I, J, N>::J_old;
        return *(out_ + J_old + I_old * outWidth_);
    }

private:
    uint32_t* out_;
    const int outWidth_;
};


//------------------------------------------------------------------------------------
// colour distance, four pixel pairs per call
//------------------------------------------------------------------------------------

//https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
//Y'CbCr conversion is a matrix multiplication => take advantage of linearity by subtracting first!
//NOTE: input is gamma-encoded RGB! => what does this mean for the output distance!??
//
//const double k_b = 0.0722; //ITU-R BT.709 conversion
//const double k_r = 0.2126; //
constexpr double k_b_d = 0.0593; //ITU-R BT.2020 conversion
constexpr double k_r_d = 0.2627; //
constexpr double k_g_d = 1 - k_b_d - k_r_d;

constexpr float k_b = static_cast<float>(k_b_d);
constexpr float k_r = static_cast<float>(k_r_d);
constexpr float k_g = static_cast<float>(k_g_d);
constexpr float scale_b = static_cast<float>(0.5 / (1 - k_b_d));
constexpr float scale_r = static_cast<float>(0.5 / (1 - k_r_d));


//a vector of pixels split into float channels
struct Channels
{
    F32x4 r, g, b, a;
};

template <bool withAlpha> FORCE_INLINE
Channels unpackChannels(U32x4 pix)
{
    using namespace simd;
    const U32x4 byteMask = set1(0xff);
    Channels c;
    c.r = toFloat(bitAnd(shr<16>(pix), byteMask));
    c.g = toFloat(bitAnd(shr< 8>(pix), byteMask));
    c.b = toFloat(bitAnd(        pix,  byteMask));
    c.a = withAlpha ? toFloat(shr<24>(pix)) : fset1(0); //alpha in [0, 255]
    return c;
}

template <bool withAlpha> FORCE_INLINE
F32x4 distYCbCr4(const Channels& pix1, const Channels& pix2)
{
    using namespace simd;

    //defer division by 255 to after matrix multiplication
    const F32x4 r_diff = fsub(pix1.r, pix2.r);
    const F32x4 g_diff = fsub(pix1.g, pix2.g);
    const F32x4 b_diff = fsub(pix1.b, pix2.b);

    const F32x4 y   = fadd(fadd(fmul(fset1(k_r), r_diff), fmul(fset1(k_g), g_diff)), fmul(fset1(k_b), b_diff)); //[!], analog YCbCr!
    const F32x4 c_b = fmul(fset1(scale_b), fsub(b_diff, y));
    const F32x4 c_r = fmul(fset1(scale_r), fsub(r_diff, y));

    //we skip division by 255 to have similar range like other distance functions
    F32x4 d = fsqrt(fadd(fadd(fmul(y, y), fmul(c_b, c_b)), fmul(c_r, c_r)));

    if constexpr (withAlpha)
    {
        /*  Requirements for a color distance handling alpha channel: with a1, a2 in [0, 1]

            1. if a1 = a2, distance should be: a1 * distYCbCr()
            2. if a1 = 0,  distance should be: a2 * distYCbCr(black, white) = a2 * 255
            3. if a1 = 1,  ??? maybe: 255 * (1 - a2) + a2 * distYCbCr()

            std::min(a1, a2) * distYCbCr(pix1, pix2) + 255 * abs(a1 - a2);   */
        d = fmul(fadd(fmul(fmin(pix1.a, pix2.a), d), fmul(fabs(fsub(pix1.a, pix2.a)), fset1(255.0f))), fset1(1.0f / 255));
    }
    return d;
}

template <bool withAlpha> FORCE_INLINE
F32x4 distYCbCr4(U32x4 pix1, U32x4 pix2)
{
    return distYCbCr4<withAlpha>(unpackChannels<withAlpha>(pix1), unpackChannels<withAlpha>(pix2));
}

//ScalerCfg narrowed to the precision the lanes work in
struct ScalerCfgF
{
    explicit ScalerCfgF(const xbrz::ScalerCfg& cfg) :
        equalColorTolerance       (static_cast<float>(cfg.equalColorTolerance)),
        centerDirectionBias       (static_cast<float>(cfg.centerDirectionBias)),
        dominantDirectionThreshold(static_cast<float>(cfg.dominantDirectionThreshold)),
        steepDirectionThreshold   (static_cast<float>(cfg.steepDirectionThreshold)) {}

    float equalColorTolerance;
    float centerDirectionBias;
    float dominantDirectionThreshold;
    float steepDirectionThreshold;
};


enum BlendType
{
    BLEND_NONE = 0,
    BLEND_NORMAL,   //a normal indication to blend
    BLEND_DOMINANT, //a strong indication to blend
    //attention: BlendType must fit into the value range of 2 bit!!!
};

/* input kernel for preprocessing step:

    -----------------
    | A | B | C | P |
    |---|---|---|---|
    | D | E | F | O |   evaluate the four corners between E, F, H, I
    |---|---|---|---|   input pixel is at position E
    | G | H | I | N |
    |---|---|---|---|
    | J | K | L | M |
    -----------------                                                         */

//distances between the pixels of one source row and their right, lower and two
//diagonal neighbours, one float per column (see computeDistRow4): every pair the
//preprocessing sums and most pairs the blend step asks for are loads from these
enum DistMap
{
    MAP_H,  //h[x]  = dist(P(x, y),     P(x + 1, y))
    MAP_V,  //v[x]  = dist(P(x, y),     P(x, y + 1))
    MAP_D1, //d1[x] = dist(P(x, y),     P(x + 1, y + 1)), diagonal "\\"
    MAP_D2, //d2[x] = dist(P(x + 1, y), P(x, y + 1)),     diagonal "/"
};

struct DistRow
{
    float* m[4]; //indexed by DistMap
};

//fill the distance rows for the four pixels at x0 .. x0 + 3 of the padded pixel rows y (s_0) and y + 1 (s_p1)
template <bool withAlpha> FORCE_INLINE
void computeDistRow4(const uint32_t* s_0, const uint32_t* s_p1, int x0, const DistRow& out)
{
    using namespace simd;

    const U32x4 a = loadu(s_0  + x0);     //P(x, y)
    const U32x4 b = loadu(s_0  + x0 + 1); //P(x + 1, y)
    const U32x4 c = loadu(s_p1 + x0);     //P(x, y + 1)
    const U32x4 d = loadu(s_p1 + x0 + 1); //P(x + 1, y + 1)

    //flat area (the common case in pixel art): all four distances are zero
    const U32x4 eq_ab = cmpEq(a, b);
    const U32x4 eq_ac = cmpEq(a, c);
    const U32x4 eq_bd = cmpEq(b, d);
    if (maskAll(bitAnd(bitAnd(eq_ab, eq_ac), eq_bd)))
    {
        const F32x4 zero = fset1(0);
        for (float* map : out.m)
            storeLanes(map + x0, zero);
        return;
    }

    const Channels ca = unpackChannels<withAlpha>(a);
    const Channels cb = unpackChannels<withAlpha>(b);
    const Channels cc = unpackChannels<withAlpha>(c);
    const Channels cd = unpackChannels<withAlpha>(d);

    //along a horizontal or vertical edge one pair is still equal in all four lanes
    auto pair = [&](float* map, U32x4 eq, const Channels& pix1, const Channels& pix2)
    {
        if (maskAll(eq))
            storeLanes(map + x0, fset1(0));
        else
            storeLanes(map + x0, distYCbCr4<withAlpha>(pix1, pix2));
    };
    pair(out.m[MAP_H ], eq_ab,       ca, cb);
    pair(out.m[MAP_V ], eq_ac,       ca, cc);
    pair(out.m[MAP_D1], cmpEq(a, d), ca, cd);
    pair(out.m[MAP_D2], cmpEq(b, c), cb, cc);
}


//detect blend direction for the corners at four consecutive x positions (one per lane):
//s_0 and s_p1 are the padded pixel rows y and y + 1, dm1 / d0 / dp1 the distance rows y - 1 .. y + 1; x0 may be -1
//result per lane, packed into a byte: blend_e | blend_f << 2 | blend_h << 4 | blend_i << 6
FORCE_INLINE
void preProcessCorners4(const uint32_t* s_0, const uint32_t* s_p1,
                        const DistRow& dm1, const DistRow& d0, const DistRow& dp1,
                        int x0, const ScalerCfgF& cfg, unsigned char* result)
{
    using namespace simd;

    const U32x4 e = loadu(s_0 + x0);
    const U32x4 f = loadu(s_0 + x0 + 1);
    const U32x4 h = loadu(s_p1 + x0);
    const U32x4 i = loadu(s_p1 + x0 + 1);

    const U32x4 eq_ef = cmpEq(e, f);
    const U32x4 eq_hi = cmpEq(h, i);
    const U32x4 eq_eh = cmpEq(e, h);
    const U32x4 eq_fi = cmpEq(f, i);

    const U32x4 skip = bitOr(bitAnd(eq_ef, eq_hi), bitAnd(eq_eh, eq_fi));
    if (maskAll(skip))
    {
        std::memset(result, 0, 4);
        return;
    }

    const F32x4 bias = fset1(cfg.centerDirectionBias);

    //hf: the "/" pairs of the 4x4 kernel: g-e, e-c, k-i, i-o and the centre pair h-f
    const float* d2_m1 = dm1.m[MAP_D2];
    const float* d2_0  = d0 .m[MAP_D2];
    const float* d2_p1 = dp1.m[MAP_D2];
    const F32x4 hf = fadd(fadd(fadd(fadd(floadu(d2_0 + x0 - 1), floadu(d2_m1 + x0)), floadu(d2_p1 + x0)), floadu(d2_0 + x0 + 1)),
                          fmul(bias, floadu(d2_0 + x0)));
    //ei: the "\\" pairs: d-h, h-l, b-f, f-n and the centre pair e-i
    const float* d1_m1 = dm1.m[MAP_D1];
    const float* d1_0  = d0 .m[MAP_D1];
    const float* d1_p1 = dp1.m[MAP_D1];
    const F32x4 ei = fadd(fadd(fadd(fadd(floadu(d1_0 + x0 - 1), floadu(d1_p1 + x0)), floadu(d1_m1 + x0)), floadu(d1_0 + x0 + 1)),
                          fmul(bias, floadu(d1_0 + x0)));

    //test sample: 70% of values max(hf, ei) / min(hf, ei) are between 1.1 and 3.7 with median being 1.8
    const U32x4 hfLess = fcmpLt(hf, ei);
    const U32x4 eiLess = fcmpLt(ei, hf);

    const F32x4 domThr = fset1(cfg.dominantDirectionThreshold);
    const U32x4 one = set1(1);
    const U32x4 valHf = add(one, bitAnd(fcmpLt(fmul(domThr, hf), ei), one)); //BLEND_DOMINANT or BLEND_NORMAL
    const U32x4 valEi = add(one, bitAnd(fcmpLt(fmul(domThr, ei), hf), one));

    const U32x4 blend_e = bitAnd(hfLess, bitAndNot(bitOr(eq_ef, eq_eh), valHf)); //e != f && e != h
    const U32x4 blend_i = bitAnd(hfLess, bitAndNot(bitOr(eq_hi, eq_fi), valHf)); //i != h && i != f
    const U32x4 blend_h = bitAnd(eiLess, bitAndNot(bitOr(eq_eh, eq_hi), valEi)); //h != e && h != i
    const U32x4 blend_f = bitAnd(eiLess, bitAndNot(bitOr(eq_ef, eq_fi), valEi)); //f != e && f != i

    const U32x4 packed = bitAndNot(skip, bitOr(bitOr(blend_e, shl<2>(blend_f)), bitOr(shl<4>(blend_h), shl<6>(blend_i))));

    const uint32_t bytes = packLowBytes(packed);
    for (int j = 0; j < 4; ++j)
        result[j] = static_cast<unsigned char>(bytes >> (8 * j));
}


//compress four blend types into a single byte
//inline BlendType getTopL   (unsigned char b) { return static_cast<BlendType>(0x3 & b); }
inline BlendType getTopR   (unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 2)); }
inline BlendType getBottomR(unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 4)); }
inline BlendType getBottomL(unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 6)); }

inline bool blendingNeeded(unsigned char b)
{
    static_assert(BLEND_NONE == 0);
    return b != 0;
}

template <RotationDegree rotDeg> inline
unsigned char rotateBlendInfo(unsigned char b) { return b; }
template <> inline unsigned char rotateBlendInfo<ROT_90 >(unsigned char b) { return ((b << 2) | (b >> 6)) & 0xff; }
template <> inline unsigned char rotateBlendInfo<ROT_180>(unsigned char b) { return ((b << 4) | (b >> 4)) & 0xff; }
template <> inline unsigned char rotateBlendInfo<ROT_270>(unsigned char b) { return ((b << 6) | (b >> 2)) & 0xff; }


/* input kernel area naming convention:
-------------
| A | B | C |
|---|---|---|
| D | E | F | input pixel is at position E
|---|---|---|
| G | H | I |
-------------                                  */
enum KernelPos { POS_A, POS_B, POS_C, POS_D, POS_E, POS_F, POS_G, POS_H, POS_I };

struct Kernel_3x3
{
    uint32_t pix[9]; //A, B, C, D, E, F, G, H, I
};

//position in the unrotated kernel that the given rotation sees at position "pos"
constexpr KernelPos rotatedPos[4][9] =
{
    {POS_A, POS_B, POS_C, POS_D, POS_E, POS_F, POS_G, POS_H, POS_I}, //ROT_0
    {POS_G, POS_D, POS_A, POS_H, POS_E, POS_B, POS_I, POS_F, POS_C}, //ROT_90
    {POS_I, POS_H, POS_G, POS_F, POS_E, POS_D, POS_C, POS_B, POS_A}, //ROT_180
    {POS_C, POS_F, POS_I, POS_B, POS_E, POS_H, POS_A, POS_D, POS_G}, //ROT_270
};

//the 24 pixel pairs blendPixel() can ask the distance of, over all four rotations:
//E against its eight neighbours and the eight adjacent pairs along the ring are
//entries of the distance rows, the eight "knight" pairs (F-G, H-C and their
//rotations) are computed per pixel, two vector distances
constexpr KernelPos distPairs[24][2] =
{
    {POS_E, POS_A}, {POS_E, POS_B}, {POS_E, POS_C}, {POS_E, POS_D},
    {POS_E, POS_F}, {POS_E, POS_G}, {POS_E, POS_H}, {POS_E, POS_I},
    {POS_A, POS_B}, {POS_B, POS_C}, {POS_C, POS_F}, {POS_F, POS_I},
    {POS_I, POS_H}, {POS_H, POS_G}, {POS_G, POS_D}, {POS_D, POS_A},
    {POS_F, POS_G}, {POS_H, POS_C}, {POS_B, POS_I}, {POS_F, POS_A},
    {POS_D, POS_C}, {POS_B, POS_G}, {POS_H, POS_A}, {POS_D, POS_I},
};

//where the first 16 pairs sit in the distance rows, relative to E at (x, y)
struct RowPairLoc
{
    signed char row; //-1, 0, 1 = y - 1, y, y + 1
    signed char map; //DistMap
    signed char dx;
};
constexpr RowPairLoc rowPairLoc[16] =
{
    {-1, MAP_D1, -1}, //E-A
    {-1, MAP_V,   0}, //E-B
    {-1, MAP_D2,  0}, //E-C
    { 0, MAP_H,  -1}, //E-D
    { 0, MAP_H,   0}, //E-F
    { 0, MAP_D2, -1}, //E-G
    { 0, MAP_V,   0}, //E-H
    { 0, MAP_D1,  0}, //E-I
    {-1, MAP_H,  -1}, //A-B
    {-1, MAP_H,   0}, //B-C
    {-1, MAP_V,   1}, //C-F
    { 0, MAP_V,   1}, //F-I
    { 1, MAP_H,   0}, //I-H
    { 1, MAP_H,  -1}, //H-G
    { 0, MAP_V,  -1}, //G-D
    {-1, MAP_V,  -1}, //D-A
};

struct DistPairIndex
{
    constexpr DistPairIndex() : idx()
    {
        for (auto& row : idx)
            for (int& v : row)
                v = -1;
        for (int p = 0; p < 24; ++p)
        {
            idx[distPairs[p][0]][distPairs[p][1]] = p;
            idx[distPairs[p][1]][distPairs[p][0]] = p;
        }
    }
    int idx[9][9];
};
constexpr DistPairIndex distPairIndex;

//the distances blendPixel() can ask for, for the pixel E at (x, y): entries of the
//distance rows y - 1 .. y + 1 are read when asked (the pair index is a compile-time
//constant at every call site, so each lookup is one load), the knight pairs are kept
struct DistSource
{
    const DistRow* rows[3];
    int x;
    float knight[8];

    FORCE_INLINE float get(int pair) const
    {
        if (pair < 16)
        {
            const RowPairLoc& loc = rowPairLoc[pair];
            return rows[loc.row + 1]->m[loc.map][x + loc.dx];
        }
        return knight[pair - 16];
    }
};

template <bool withAlpha> FORCE_INLINE
DistSource makeDistSource(const Kernel_3x3& ker, const DistRow& dm1, const DistRow& d0, const DistRow& dp1, int x)
{
    using namespace simd;
    DistSource ds;
    ds.rows[0] = &dm1;
    ds.rows[1] = &d0;
    ds.rows[2] = &dp1;
    ds.x = x;
    for (int p = 16; p < 24; p += 4)
    {
        const U32x4 pix1 = set4(ker.pix[distPairs[p][0]], ker.pix[distPairs[p + 1][0]], ker.pix[distPairs[p + 2][0]], ker.pix[distPairs[p + 3][0]]);
        const U32x4 pix2 = set4(ker.pix[distPairs[p][1]], ker.pix[distPairs[p + 1][1]], ker.pix[distPairs[p + 2][1]], ker.pix[distPairs[p + 3][1]]);
        storeLanes(ds.knight + p - 16, distYCbCr4<withAlpha>(pix1, pix2));
    }
    return ds;
}


template <class Scaler, RotationDegree rotDeg>
FORCE_INLINE //perf: quite worth it!
void blendPixel(const Kernel_3x3& ker,
                const DistSource& ds,
                uint32_t* target, int trgWidth,
                unsigned char blendInfo, //result of preprocessing all four corners of pixel "E"
                const ScalerCfgF& cfg)
{
    const unsigned char blend = rotateBlendInfo<rotDeg>(blendInfo);

    if (getBottomR(blend) >= BLEND_NORMAL)
    {
        auto pix  = [&](KernelPos pos) { return ker.pix[rotatedPos[rotDeg][pos]]; };
        auto dist = [&](KernelPos pos1, KernelPos pos2)
        {
            const int i = distPairIndex.idx[rotatedPos[rotDeg][pos1]][rotatedPos[rotDeg][pos2]];
            assert(i >= 0);
            return ds.get(i);
        };
        auto eq = [&](KernelPos pos1, KernelPos pos2) { return dist(pos1, pos2) < cfg.equalColorTolerance; };

        //short-circuiting on purpose: evaluating every test unconditionally was measured slower
        //than the branches, each test being a distance row load
        const bool doLineBlend = [&]  // MSVC workaround: remove -> bool
        {
            if (getBottomR(blend) >= BLEND_DOMINANT)
                return true;

            //make sure there is no second blending in an adjacent rotation for this pixel: handles insular pixels, mario eyes
            if (getTopR(blend) != BLEND_NONE && !eq(POS_E, POS_G)) //but support double-blending for 90° corners
                return false;
            if (getBottomL(blend) != BLEND_NONE && !eq(POS_E, POS_C))
                return false;

            //no full blending for L-shapes; blend corner only (handles "mario mushroom eyes")
            if (!eq(POS_E, POS_I) && eq(POS_G, POS_H) && eq(POS_H, POS_I) && eq(POS_I, POS_F) && eq(POS_F, POS_C))
                return false;

            return true;
        }();

        const uint32_t px = dist(POS_E, POS_F) <= dist(POS_E, POS_H) ? pix(POS_F) : pix(POS_H); //choose most similar color

        OutputMatrix<Scaler::scale, rotDeg> out(target, trgWidth);

        if (doLineBlend)
        {
            const float fg = dist(POS_F, POS_G); //test sample: 70% of values max(fg, hc) / min(fg, hc) are between 1.1 and 3.7 with median being 1.9
            const float hc = dist(POS_H, POS_C); //

            const bool haveShallowLine = cfg.steepDirectionThreshold * fg <= hc && pix(POS_E) != pix(POS_G) && pix(POS_D) != pix(POS_G);
            const bool haveSteepLine   = cfg.steepDirectionThreshold * hc <= fg && pix(POS_E) != pix(POS_C) && pix(POS_B) != pix(POS_C);

            if (haveShallowLine)
            {
                if (haveSteepLine)
                    Scaler::blendLineSteepAndShallow(px, out);
                else
                    Scaler::blendLineShallow(px, out);
            }
            else
            {
                if (haveSteepLine)
                    Scaler::blendLineSteep(px, out);
                else
                    Scaler::blendLineDiagonal(px, out);
            }
        }
        else
            Scaler::blendCorner(px, out);
    }
}


//------------------------------------------------------------------------------------
// source rows with the out-of-bounds policy applied as padding
//------------------------------------------------------------------------------------

//pixels available left of x = 0 and right of x = srcWidth - 1 in a padded row:
//the distance rows cover columns -2 .. srcWidth + 3 (the corner lanes run up to
//srcWidth + 2 and read one column to either side), computed four at a time
//from pixel columns up to srcWidth + 7
const int PAD_LEFT  = 4;
const int PAD_RIGHT = 8;

struct OobReaderTransparent
{
    static void padRow(const uint32_t* src, int srcWidth, int srcHeight, int srcPitch, int y, uint32_t* row /*column 0*/)
    {
        if (0 <= y && y < srcHeight)
        {
            std::memset(row - PAD_LEFT, 0, PAD_LEFT * sizeof(uint32_t));
            std::memcpy(row, src + srcPitch * y, srcWidth * sizeof(uint32_t));
            std::memset(row + srcWidth, 0, PAD_RIGHT * sizeof(uint32_t));
        }
        else
            std::memset(row - PAD_LEFT, 0, (PAD_LEFT + srcWidth + PAD_RIGHT) * sizeof(uint32_t));
    }
};

struct OobReaderDuplicate
{
    static void padRow(const uint32_t* src, int srcWidth, int srcHeight, int srcPitch, int y, uint32_t* row /*column 0*/)
    {
        const uint32_t* srcRow = src + srcPitch * std::clamp(y, 0, srcHeight - 1);
        std::memcpy(row, srcRow, srcWidth * sizeof(uint32_t));
        std::fill(row - PAD_LEFT, row, srcRow[0]);
        std::fill(row + srcWidth, row + srcWidth + PAD_RIGHT, srcRow[srcWidth - 1]);
    }
};


template <int blockSize> FORCE_INLINE
void fillBlock(uint32_t* trg, int trgWidth, uint32_t col)
{
    using namespace simd;
    const U32x4 col4 = set1(col);
    for (int y = 0; y < blockSize; ++y, trg += trgWidth)
    {
        int x = 0;
        for (; x + 4 <= blockSize; x += 4)
            storeu(trg + x, col4);
        if constexpr (blockSize % 4 >= 2)
        {
            storeu2(trg + x, col4);
            x += 2;
        }
        if constexpr (blockSize % 2 == 1)
            trg[x] = col;
    }
}

//one output row of the blocks of four consecutive source pixels is 4 * blockSize pixels,
//blockSize vectors: vector j holds output pixels 4j .. 4j + 3, source lanes (4j + k) / blockSize
template <int blockSize, int... J> FORCE_INLINE
void blockRowVectors(U32x4* row, U32x4 pix, std::integer_sequence<int, J...>)
{
    ((row[J] = simd::lanes<(4 * J) / blockSize, (4 * J + 1) / blockSize, (4 * J + 2) / blockSize, (4 * J + 3) / blockSize>(pix)), ...);
}

//fill the blocks of four consecutive source pixels
template <int blockSize> FORCE_INLINE
void fillBlock4(uint32_t* trg, int trgWidth, const uint32_t* pix)
{
    using namespace simd;
    U32x4 row[blockSize];
    blockRowVectors<blockSize>(row, loadu(pix), std::make_integer_sequence<int, blockSize>{});
    for (int y = 0; y < blockSize; ++y, trg += trgWidth)
        for (int j = 0; j < blockSize; ++j)
            storeu(trg + 4 * j, row[j]);
}


inline uint32_t load4(const unsigned char* p) //four bytes, the first in the low byte
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}


template <class Scaler, bool withAlpha, class OobReader> //scaler policy: see "Scaler2x" reference implementation
void scaleImage(const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, int srcPitch, int trgPitch, const xbrz::ScalerCfg& cfgIn, int yFirst, int yLast)
{
    yFirst = std::max(yFirst, 0);
    yLast  = std::min(yLast, srcHeight);
    if (yFirst >= yLast || srcWidth <= 0)
        return;

    const ScalerCfgF cfg(cfgIn);
    const int trgWidth = trgPitch;
    const int rowLen = PAD_LEFT + srcWidth + PAD_RIGHT;

    //ring of the four padded source rows y - 1 .. y + 2 that the kernels of row y read
    std::vector<uint32_t> rowBuf(4 * rowLen);
    auto paddedRow = [&](int y) { return rowBuf.data() + ((y + 8) & 3) * rowLen + PAD_LEFT; }; //y >= -2

    //ring of the distance rows y - 2 .. y + 1; row r holds the distances between source rows r and r + 1
    std::vector<float> distBuf(4 * 4 * rowLen);
    auto distRow = [&](int r) //r >= -2
    {
        float* base = distBuf.data() + ((r + 8) & 3) * 4 * rowLen + PAD_LEFT;
        return DistRow{{base, base + rowLen, base + 2 * rowLen, base + 3 * rowLen}};
    };
    auto computeDistRow = [&](int r)
    {
        const uint32_t* s_0  = paddedRow(r);
        const uint32_t* s_p1 = paddedRow(r + 1);
        const DistRow out = distRow(r);
        for (int x = -2; x < srcWidth + 4; x += 4) //columns -2 .. srcWidth + 3 are read: corner lanes up to srcWidth + 2, one more to either side
            computeDistRow4<withAlpha>(s_0, s_p1, x, out);
    };

    for (int y = yFirst - 2; y <= yFirst + 1; ++y)
        OobReader::padRow(src, srcWidth, srcHeight, srcPitch, y, paddedRow(y));
    for (int r = yFirst - 2; r <= yFirst; ++r)
        computeDistRow(r);

    /*  preprocessing blend result:
        ---------
        | E | F |   evaluate corner between E, F, H, I
        |---+---|   current input pixel is at position E
        | H | I |
        ---------                                        */
    //corner results of the rows y - 1 (prev) and y (cur): byte x + 1 holds the corner at (x, y), x = -1 .. srcWidth - 1;
    //the last lane group may write up to three bytes past that
    const int cornerLen = srcWidth + 4;
    std::vector<unsigned char> cornerBuf(2 * cornerLen);
    unsigned char* cornersPrev = cornerBuf.data();
    unsigned char* cornersCur  = cornerBuf.data() + cornerLen;

    auto preProcessRow = [&](int y, unsigned char* corners)
    {
        const uint32_t* s_0  = paddedRow(y);
        const uint32_t* s_p1 = paddedRow(y + 1);
        const DistRow dm1 = distRow(y - 1);
        const DistRow d0  = distRow(y);
        const DistRow dp1 = distRow(y + 1);

        for (int x = -1; x < srcWidth; x += 4)
            preProcessCorners4(s_0, s_p1, dm1, d0, dp1, x, cfg, corners + x + 1);
    };

    //initialize for first row of current stripe: detect upper left and right corner blending
    //this cannot be optimized for adjacent processing stripes; we must not allow for a memory race condition!
    preProcessRow(yFirst - 1, cornersPrev);

    for (int y = yFirst; y < yLast; ++y)
    {
        uint32_t* out = trg + Scaler::scale * y * trgWidth; //consider MT "striped" access

        OobReader::padRow(src, srcWidth, srcHeight, srcPitch, y + 2, paddedRow(y + 2));
        computeDistRow(y + 1);
        preProcessRow(y, cornersCur);

        const uint32_t* s_m1 = paddedRow(y - 1);
        const uint32_t* s_0  = paddedRow(y);
        const uint32_t* s_p1 = paddedRow(y + 1);
        const DistRow dm1 = distRow(y - 1);
        const DistRow d0  = distRow(y);
        const DistRow dp1 = distRow(y + 1);

        //the four corners of pixel (x, y): top-left is corner (x - 1, y - 1)'s blend_i, top-right is (x, y - 1)'s blend_h,
        //bottom-right is (x, y)'s blend_e, bottom-left is (x - 1, y)'s blend_f
        auto blendInfo = [&](int x)
        {
            return static_cast<unsigned char>((cornersPrev[x] >> 6) |
                                              (((cornersPrev[x + 1] >> 4) & 0x3) << 2) |
                                              ((cornersCur[x + 1] & 0x3) << 4) |
                                              (((cornersCur[x] >> 2) & 0x3) << 6));
        };

        auto scalePixel = [&](int x, uint32_t* outBlock, unsigned char blend_xy)
        {
            //fill block of size scale * scale with the given color
            fillBlock<Scaler::scale>(outBlock, trgWidth, s_0[x]);

            //blend all four corners of current pixel
            if (blendingNeeded(blend_xy))
            {
                const Kernel_3x3 ker3 = {{s_m1[x - 1], s_m1[x], s_m1[x + 1],
                                          s_0 [x - 1], s_0 [x], s_0 [x + 1],
                                          s_p1[x - 1], s_p1[x], s_p1[x + 1]}};
                const DistSource ds = makeDistSource<withAlpha>(ker3, dm1, d0, dp1, x);

                blendPixel<Scaler, ROT_0  >(ker3, ds, outBlock, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ROT_90 >(ker3, ds, outBlock, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ROT_180>(ker3, ds, outBlock, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ROT_270>(ker3, ds, outBlock, trgWidth, blend_xy, cfg);
            }
        };

        //pixels are taken four at a time: when none of the four has a corner to blend (the
        //common case) their blocks are filled in one go, otherwise the four blend bytes are
        //consumed one pixel per iteration. One call site for scalePixel(), so the (large)
        //blend code is inlined once
        uint32_t blend4 = 0; //blend bytes of the pending pixels, the next one in the low byte
        int pending = 0;
        for (int x = 0; x < srcWidth; ++x, out += Scaler::scale)
        {
            if (pending == 0)
            {
                if (x + 4 <= srcWidth)
                {
                    //blendInfo() for the pixels x .. x + 3 at once, one per byte
                    blend4 = ((load4(cornersPrev + x) >> 6) & 0x03030303u) |
                             (((load4(cornersPrev + x + 1) >> 4) & 0x03030303u) << 2) |
                             ((load4(cornersCur + x + 1) & 0x03030303u) << 4) |
                             (((load4(cornersCur + x) >> 2) & 0x03030303u) << 6);
                    if (blend4 == 0)
                    {
                        fillBlock4<Scaler::scale>(out, trgWidth, s_0 + x);
                        x += 3;
                        out += 3 * Scaler::scale;
                        continue;
                    }
                    pending = 4;
                }
                else
                {
                    blend4 = blendInfo(x);
                    pending = 1;
                }
            }
            scalePixel(x, out, static_cast<unsigned char>(blend4));
            blend4 >>= 8;
            --pending;
        }

        std::swap(cornersPrev, cornersCur);
    }
}

//------------------------------------------------------------------------------------

template <class ColorGradient>
struct Scaler2x : public ColorGradient
{
    static const int scale = 2;

    template <unsigned int M, unsigned int N> //bring template function into scope for GCC
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront) { ColorGradient::template alphaGrad<M, N>(pixBack, pixFront); }


    template <class OutputMatrix>
    static void blendLineShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);
    }

    template <class OutputMatrix>
    static void blendLineSteep(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);
    }

    template <class OutputMatrix>
    static void blendLineSteepAndShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<0, 1>(), col);
        alphaGrad<5, 6>(out.template ref<1, 1>(), col); //[!] fixes 7/8 used in xBR
    }

    template <class OutputMatrix>
    static void blendLineDiagonal(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 2>(out.template ref<1, 1>(), col);
    }

    template <class OutputMatrix>
    static void blendCorner(uint32_t col, OutputMatrix& out)
    {
        //model a round corner
        alphaGrad<21, 100>(out.template ref<1, 1>(), col); //exact: 1 - pi/4 = 0.2146018366
    }
};


template <class ColorGradient>
struct Scaler3x : public ColorGradient
{
    static const int scale = 3;

    template <unsigned int M, unsigned int N> //bring template function into scope for GCC
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront) { ColorGradient::template alphaGrad<M, N>(pixBack, pixFront); }


    template <class OutputMatrix>
    static void blendLineShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);

        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);
        out.template ref<scale - 1, 2>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteep(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);

        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);
        out.template ref<2, scale - 1>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteepAndShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<2, 0>(), col);
        alphaGrad<1, 4>(out.template ref<0, 2>(), col);
        alphaGrad<3, 4>(out.template ref<2, 1>(), col);
        alphaGrad<3, 4>(out.template ref<1, 2>(), col);
        out.template ref<2, 2>() = col;
    }

    template <class OutputMatrix>
    static void blendLineDiagonal(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 8>(out.template ref<1, 2>(), col); //conflict with other rotations for this odd scale
        alphaGrad<1, 8>(out.template ref<2, 1>(), col);
        alphaGrad<7, 8>(out.template ref<2, 2>(), col); //
    }

    template <class OutputMatrix>
    static void blendCorner(uint32_t col, OutputMatrix& out)
    {
        //model a round corner
        alphaGrad<45, 100>(out.template ref<2, 2>(), col); //exact: 0.4545939598
        //alphaGrad<3, 100>(out.template ref<2, 1>(), col); //0.02826017254 -> negligible + avoid overlap with other rotations at this scale
        //alphaGrad<3, 100>(out.template ref<1, 2>(), col); //0.02826017254
    }
};


template <class ColorGradient>
struct Scaler4x : public ColorGradient
{
    static const int scale = 4;

    template <unsigned int M, unsigned int N> //bring template function into scope for GCC
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront) { ColorGradient::template alphaGrad<M, N>(pixBack, pixFront); }


    template <class OutputMatrix>
    static void blendLineShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);

        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 2, 3>(), col);

        out.template ref<scale - 1, 2>() = col;
        out.template ref<scale - 1, 3>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteep(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);

        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);
        alphaGrad<3, 4>(out.template ref<3, scale - 2>(), col);

        out.template ref<2, scale - 1>() = col;
        out.template ref<3, scale - 1>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteepAndShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<3, 4>(out.template ref<3, 1>(), col);
        alphaGrad<3, 4>(out.template ref<1, 3>(), col);
        alphaGrad<1, 4>(out.template ref<3, 0>(), col);
        alphaGrad<1, 4>(out.template ref<0, 3>(), col);

        alphaGrad<1, 3>(out.template ref<2, 2>(), col); //[!] fixes 1/4 used in xBR

        out.template ref<3, 3>() = col;
        out.template ref<3, 2>() = col;
        out.template ref<2, 3>() = col;
    }

    template <class OutputMatrix>
    static void blendLineDiagonal(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 2>(out.template ref<scale - 1, scale / 2    >(), col);
        alphaGrad<1, 2>(out.template ref<scale - 2, scale / 2 + 1>(), col);
        out.template ref<scale - 1, scale - 1>() = col;
    }

    template <class OutputMatrix>
    static void blendCorner(uint32_t col, OutputMatrix& out)
    {
        //model a round corner
        alphaGrad<68, 100>(out.template ref<3, 3>(), col); //exact: 0.6848532563
        alphaGrad< 9, 100>(out.template ref<3, 2>(), col); //0.08677704501
        alphaGrad< 9, 100>(out.template ref<2, 3>(), col); //0.08677704501
    }
};


template <class ColorGradient>
struct Scaler5x : public ColorGradient
{
    static const int scale = 5;

    template <unsigned int M, unsigned int N> //bring template function into scope for GCC
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront) { ColorGradient::template alphaGrad<M, N>(pixBack, pixFront); }


    template <class OutputMatrix>
    static void blendLineShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 3, 4>(), col);

        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 2, 3>(), col);

        out.template ref<scale - 1, 2>() = col;
        out.template ref<scale - 1, 3>() = col;
        out.template ref<scale - 1, 4>() = col;
        out.template ref<scale - 2, 4>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteep(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);
        alphaGrad<1, 4>(out.template ref<4, scale - 3>(), col);

        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);
        alphaGrad<3, 4>(out.template ref<3, scale - 2>(), col);

        out.template ref<2, scale - 1>() = col;
        out.template ref<3, scale - 1>() = col;
        out.template ref<4, scale - 1>() = col;
        out.template ref<4, scale - 2>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteepAndShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);
        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);

        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);

        alphaGrad<2, 3>(out.template ref<3, 3>(), col);

        out.template ref<2, scale - 1>() = col;
        out.template ref<3, scale - 1>() = col;
        out.template ref<4, scale - 1>() = col;

        out.template ref<scale - 1, 2>() = col;
        out.template ref<scale - 1, 3>() = col;
    }

    template <class OutputMatrix>
    static void blendLineDiagonal(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 8>(out.template ref<scale - 1, scale / 2    >(), col); //conflict with other rotations for this odd scale
        alphaGrad<1, 8>(out.template ref<scale - 2, scale / 2 + 1>(), col);
        alphaGrad<1, 8>(out.template ref<scale - 3, scale / 2 + 2>(), col); //

        alphaGrad<7, 8>(out.template ref<4, 3>(), col);
        alphaGrad<7, 8>(out.template ref<3, 4>(), col);

        out.template ref<4, 4>() = col;
    }

    template <class OutputMatrix>
    static void blendCorner(uint32_t col, OutputMatrix& out)
    {
        //model a round corner
        alphaGrad<86, 100>(out.template ref<4, 4>(), col); //exact: 0.8631434088
        alphaGrad<23, 100>(out.template ref<4, 3>(), col); //0.2306749731
        alphaGrad<23, 100>(out.template ref<3, 4>(), col); //0.2306749731
        //alphaGrad<2, 100>(out.template ref<4, 2>(), col); //0.01676812367 -> negligible + avoid overlap with other rotations at this scale
        //alphaGrad<2, 100>(out.template ref<2, 4>(), col); //0.01676812367
    }
};


template <class ColorGradient>
struct Scaler6x : public ColorGradient
{
    static const int scale = 6;

    template <unsigned int M, unsigned int N> //bring template function into scope for GCC
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront) { ColorGradient::template alphaGrad<M, N>(pixBack, pixFront); }


    template <class OutputMatrix>
    static void blendLineShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 3, 4>(), col);

        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 2, 3>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 3, 5>(), col);

        out.template ref<scale - 1, 2>() = col;
        out.template ref<scale - 1, 3>() = col;
        out.template ref<scale - 1, 4>() = col;
        out.template ref<scale - 1, 5>() = col;

        out.template ref<scale - 2, 4>() = col;
        out.template ref<scale - 2, 5>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteep(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);
        alphaGrad<1, 4>(out.template ref<4, scale - 3>(), col);

        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);
        alphaGrad<3, 4>(out.template ref<3, scale - 2>(), col);
        alphaGrad<3, 4>(out.template ref<5, scale - 3>(), col);

        out.template ref<2, scale - 1>() = col;
        out.template ref<3, scale - 1>() = col;
        out.template ref<4, scale - 1>() = col;
        out.template ref<5, scale - 1>() = col;

        out.template ref<4, scale - 2>() = col;
        out.template ref<5, scale - 2>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteepAndShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);
        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);
        alphaGrad<3, 4>(out.template ref<3, scale - 2>(), col);

        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 2, 3>(), col);

        out.template ref<2, scale - 1>() = col;
        out.template ref<3, scale - 1>() = col;
        out.template ref<4, scale - 1>() = col;
        out.template ref<5, scale - 1>() = col;

        out.template ref<4, scale - 2>() = col;
        out.template ref<5, scale - 2>() = col;

        out.template ref<scale - 1, 2>() = col;
        out.template ref<scale - 1, 3>() = col;
    }

    template <class OutputMatrix>
    static void blendLineDiagonal(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 2>(out.template ref<scale - 1, scale / 2    >(), col);
        alphaGrad<1, 2>(out.template ref<scale - 2, scale / 2 + 1>(), col);
        alphaGrad<1, 2>(out.template ref<scale - 3, scale / 2 + 2>(), col);

        out.template ref<scale - 2, scale - 1>() = col;
        out.template ref<scale - 1, scale - 1>() = col;
        out.template ref<scale - 1, scale - 2>() = col;
    }

    template <class OutputMatrix>
    static void blendCorner(uint32_t col, OutputMatrix& out)
    {
        //model a round corner
        alphaGrad<97, 100>(out.template ref<5, 5>(), col); //exact: 0.9711013910
        alphaGrad<42, 100>(out.template ref<4, 5>(), col); //0.4236372243
        alphaGrad<42, 100>(out.template ref<5, 4>(), col); //0.4236372243
        alphaGrad< 6, 100>(out.template ref<5, 3>(), col); //0.05652034508
        alphaGrad< 6, 100>(out.template ref<3, 5>(), col); //0.05652034508
    }
};


//VBA-M: 9x, extending the pattern of the even and odd scalers above
template <class ColorGradient>
struct Scaler9x : public ColorGradient
{
    static const int scale = 9;

    template <unsigned int M, unsigned int N> //bring template function into scope for GCC
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront) { ColorGradient::template alphaGrad<M, N>(pixBack, pixFront); }


    template <class OutputMatrix>
    static void blendLineShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 3, 4>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 4, 6>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 5, 8>(), col); //5th step for odd scale (no 3/4 since col 9 is out of bounds)

        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 2, 3>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 3, 5>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 4, 7>(), col);

        out.template ref<scale - 1, 2>() = col;
        out.template ref<scale - 1, 3>() = col;
        out.template ref<scale - 1, 4>() = col;
        out.template ref<scale - 1, 5>() = col;
        out.template ref<scale - 1, 6>() = col;
        out.template ref<scale - 1, 7>() = col;
        out.template ref<scale - 1, 8>() = col;

        out.template ref<scale - 2, 4>() = col;
        out.template ref<scale - 2, 5>() = col;
        out.template ref<scale - 2, 6>() = col;
        out.template ref<scale - 2, 7>() = col;
        out.template ref<scale - 2, 8>() = col;

        out.template ref<scale - 3, 6>() = col;
        out.template ref<scale - 3, 7>() = col;
        out.template ref<scale - 3, 8>() = col;

        out.template ref<scale - 4, 8>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteep(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);
        alphaGrad<1, 4>(out.template ref<4, scale - 3>(), col);
        alphaGrad<1, 4>(out.template ref<6, scale - 4>(), col);
        alphaGrad<1, 4>(out.template ref<8, scale - 5>(), col); //5th step for odd scale

        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);
        alphaGrad<3, 4>(out.template ref<3, scale - 2>(), col);
        alphaGrad<3, 4>(out.template ref<5, scale - 3>(), col);
        alphaGrad<3, 4>(out.template ref<7, scale - 4>(), col);

        out.template ref<2, scale - 1>() = col;
        out.template ref<3, scale - 1>() = col;
        out.template ref<4, scale - 1>() = col;
        out.template ref<5, scale - 1>() = col;
        out.template ref<6, scale - 1>() = col;
        out.template ref<7, scale - 1>() = col;
        out.template ref<8, scale - 1>() = col;

        out.template ref<4, scale - 2>() = col;
        out.template ref<5, scale - 2>() = col;
        out.template ref<6, scale - 2>() = col;
        out.template ref<7, scale - 2>() = col;
        out.template ref<8, scale - 2>() = col;

        out.template ref<6, scale - 3>() = col;
        out.template ref<7, scale - 3>() = col;
        out.template ref<8, scale - 3>() = col;

        out.template ref<8, scale - 4>() = col;
    }

    template <class OutputMatrix>
    static void blendLineSteepAndShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);
        alphaGrad<1, 4>(out.template ref<4, scale - 3>(), col);
        alphaGrad<3, 4>(out.template ref<1, scale - 1>(), col);
        alphaGrad<3, 4>(out.template ref<3, scale - 2>(), col);
        alphaGrad<3, 4>(out.template ref<5, scale - 3>(), col);

        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 3, 4>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 1, 1>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 2, 3>(), col);
        alphaGrad<3, 4>(out.template ref<scale - 3, 5>(), col);

        alphaGrad<2, 3>(out.template ref<5, 5>(), col); //center blend to connect steep and shallow (like Scaler5x)

        out.template ref<2, scale - 1>() = col;
        out.template ref<3, scale - 1>() = col;
        out.template ref<4, scale - 1>() = col;
        out.template ref<5, scale - 1>() = col;
        out.template ref<6, scale - 1>() = col;
        out.template ref<7, scale - 1>() = col;
        out.template ref<8, scale - 1>() = col;

        out.template ref<4, scale - 2>() = col;
        out.template ref<5, scale - 2>() = col;
        out.template ref<6, scale - 2>() = col;
        out.template ref<7, scale - 2>() = col;
        out.template ref<8, scale - 2>() = col;

        out.template ref<6, scale - 3>() = col; //close gap between steep and shallow

        out.template ref<scale - 1, 2>() = col;
        out.template ref<scale - 1, 3>() = col;
        out.template ref<scale - 1, 4>() = col;
        out.template ref<scale - 1, 5>() = col;
        out.template ref<scale - 1, 6>() = col;
        out.template ref<scale - 1, 7>() = col;
        out.template ref<scale - 1, 8>() = col;

        out.template ref<scale - 2, 4>() = col;
        out.template ref<scale - 2, 5>() = col;
        out.template ref<scale - 2, 6>() = col;
        out.template ref<scale - 2, 7>() = col;
        out.template ref<scale - 2, 8>() = col;
    }

    template <class OutputMatrix>
    static void blendLineDiagonal(uint32_t col, OutputMatrix& out)
    {
        //odd scale: use 1/8 for diagonal trace to avoid conflicts with other rotations (like Scaler5x)
        alphaGrad<1, 8>(out.template ref<scale - 1, scale / 2    >(), col); //[8, 4]
        alphaGrad<1, 8>(out.template ref<scale - 2, scale / 2 + 1>(), col); //[7, 5]
        alphaGrad<1, 8>(out.template ref<scale - 3, scale / 2 + 2>(), col); //[6, 6]
        alphaGrad<1, 8>(out.template ref<scale - 4, scale / 2 + 3>(), col); //[5, 7]
        alphaGrad<1, 8>(out.template ref<scale - 5, scale / 2 + 4>(), col); //[4, 8]

        alphaGrad<7, 8>(out.template ref<scale - 1, scale / 2 + 1>(), col); //[8, 5]
        alphaGrad<7, 8>(out.template ref<scale - 2, scale / 2 + 2>(), col); //[7, 6]
        alphaGrad<7, 8>(out.template ref<scale - 3, scale / 2 + 3>(), col); //[6, 7]
        alphaGrad<7, 8>(out.template ref<scale - 4, scale / 2 + 4>(), col); //[5, 8]

        out.template ref<scale - 1, scale - 1>() = col; //[8, 8]
        out.template ref<scale - 1, scale - 2>() = col; //[8, 7]
        out.template ref<scale - 1, scale - 3>() = col; //[8, 6]
        out.template ref<scale - 2, scale - 1>() = col; //[7, 8]
        out.template ref<scale - 2, scale - 2>() = col; //[7, 7]
        out.template ref<scale - 3, scale - 1>() = col; //[6, 8]
    }

    template <class OutputMatrix>
    static void blendCorner(uint32_t col, OutputMatrix& out)
    {
        //model a round corner
        alphaGrad<99, 100>(out.template ref<8, 8>(), col);
        alphaGrad<56, 100>(out.template ref<7, 8>(), col);
        alphaGrad<56, 100>(out.template ref<8, 7>(), col);
        alphaGrad<16, 100>(out.template ref<8, 6>(), col);
        alphaGrad<16, 100>(out.template ref<6, 8>(), col);
        alphaGrad< 3, 100>(out.template ref<8, 5>(), col);
        alphaGrad< 3, 100>(out.template ref<5, 8>(), col);
    }
};

//------------------------------------------------------------------------------------

struct ColorGradientRGB
{
    template <unsigned int M, unsigned int N>
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront)
    {
        pixBack = gradientRGB<M, N>(pixFront, pixBack);
    }
};

struct ColorGradientARGB
{
    template <unsigned int M, unsigned int N>
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront)
    {
        pixBack = gradientARGB<M, N>(pixFront, pixBack);
    }
};


template <template <class> class Scaler>
void scaleImageFmt(ColorFormat colFmt, const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, int srcPitch, int trgPitch, const xbrz::ScalerCfg& cfg, int yFirst, int yLast)
{
    switch (colFmt)
    {
        case ColorFormat::rgb:
            return scaleImage<Scaler<ColorGradientRGB>, false, OobReaderDuplicate>(src, trg, srcWidth, srcHeight, srcPitch, trgPitch, cfg, yFirst, yLast);
        case ColorFormat::argb:
        case ColorFormat::argbUnbuffered:
            return scaleImage<Scaler<ColorGradientARGB>, true, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitch, trgPitch, cfg, yFirst, yLast);
    }
    assert(false);
}
}


void xbrz::scale(size_t factor, const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, ColorFormat colFmt, int srcPitch, int trgPitch, const xbrz::ScalerCfg& cfg, int yFirst, int yLast)
{
    // VBA-M: Convert byte pitches to pixel pitches
    // VBA-M passes pitch in bytes, but we need pitch in pixels (uint32_t units)
    const int srcPitchPixels = srcPitch / static_cast<int>(sizeof(uint32_t));
    const int trgPitchPixels = trgPitch / static_cast<int>(sizeof(uint32_t));

    if (factor == 1)
    {
        // Copy with pitch support
        yFirst = std::max(yFirst, 0);
        yLast  = std::min(yLast, srcHeight);
        for (int y = yFirst; y < yLast; ++y)
        {
            const uint32_t* srcLine = src + y * srcPitchPixels;
            uint32_t* trgLine = trg + y * trgPitchPixels;
            std::copy(srcLine, srcLine + srcWidth, trgLine);
        }
        return;
    }

    static_assert(SCALE_FACTOR_MAX == 9);
    switch (factor)
    {
        case 2: return scaleImageFmt<Scaler2x>(colFmt, src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
        case 3: return scaleImageFmt<Scaler3x>(colFmt, src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
        case 4: return scaleImageFmt<Scaler4x>(colFmt, src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
        case 5: return scaleImageFmt<Scaler5x>(colFmt, src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
        case 6: return scaleImageFmt<Scaler6x>(colFmt, src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
        case 9: return scaleImageFmt<Scaler9x>(colFmt, src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
    }
    assert(false);
}


bool xbrz::equalColorTest2(uint32_t col1, uint32_t col2, ColorFormat colFmt, double equalColorTolerance, double /*testAttribute*/)
{
    float d[4];
    switch (colFmt)
    {
        case ColorFormat::rgb:
            simd::storeLanes(d, distYCbCr4<false>(simd::set1(col1), simd::set1(col2)));
            return d[0] < static_cast<float>(equalColorTolerance);
        case ColorFormat::argb:
        case ColorFormat::argbUnbuffered:
            simd::storeLanes(d, distYCbCr4<true>(simd::set1(col1), simd::set1(col2)));
            return d[0] < static_cast<float>(equalColorTolerance);
    }
    assert(false);
    return false;
}


void xbrz::bilinearScale(const uint32_t* src, int srcWidth, int srcHeight,
                         /**/  uint32_t* trg, int trgWidth, int trgHeight)
{
    const auto pixRead = [src, srcWidth](int x, int y)
    {
        const uint32_t pixSrc = src[y * srcWidth + x];

        return [pixSrc, a = int(getAlpha(pixSrc))](int channel)
        {
            if (channel == 3)
                return a;

            //Limitation: alpha should be applied in gamma-decoded linear RGB space: https://ssp.impulsetrain.com/gamma-premult.html
            return getByte(pixSrc, channel) * a;
        };
    };

    const auto pixWrite = [trg](const auto& interpolate) mutable
    {
        const double a = interpolate(3);
        if (a <= 0.0)
            * trg++ = 0;
        else
            * trg++ = makePixel(byteRound(a),
                                byteRound(interpolate(2) / a),  //r
                                byteRound(interpolate(1) / a),  //g
                                byteRound(interpolate(0) / a)); //b
    };

    bilinearScale(pixRead, srcWidth, srcHeight,
                  pixWrite, trgWidth, trgHeight, 0, trgHeight);
}


void xbrz::nearestNeighborScale(const uint32_t* src, int srcWidth, int srcHeight,
                                /**/  uint32_t* trg, int trgWidth, int trgHeight)
{
    const auto pixRead = [src, srcWidth](int x, int y) { return src[y * srcWidth + x]; };

    const auto pixWrite = [trg](uint32_t pix) mutable { *trg++ = pix; };

    nearestNeighborScale(pixRead, srcWidth, srcHeight,
                         pixWrite, trgWidth, trgHeight, 0, trgHeight);
}
