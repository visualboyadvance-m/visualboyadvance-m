// ****************************************************************************
// * This file is part of the xBRZ project. It is distributed under           *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0         *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved          *
// *                                                                          *
// * xBRZ 9x scaler - extends xBRZ algorithm to 9x scaling factor             *
// * Based on the standard xBRZ implementation, following the same patterns   *
// * as Scaler2x through Scaler6x but extended to 9x.                         *
// ****************************************************************************

#include "xbrz9x.h"
#include "xbrz.h"
#include "xbrz_tools.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

using namespace xbrz;

// some gcc versions lie about having this C++17 feature
#define static_assert(x) static_assert(x, "assertion failed")

namespace
{

// ============================================================================
// Basic infrastructure from xBRZ
// ============================================================================

inline double fastSqrt(double n)
{
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __asm__ ("fsqrt" : "+t" (n));
    return n;
#elif defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fld n
        fsqrt
    }
#elif defined(_MSC_VER)
    return sqrt(n);
#else
    return std::sqrt(n);
#endif
}

#ifdef _MSC_VER
    #define FORCE_INLINE __forceinline
#elif defined __GNUC__
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif

template <class T> inline
T square(T value) { return value * value; }

inline
double distYCbCr(uint32_t pix1, uint32_t pix2, double lumaWeight)
{
    const int r_diff = static_cast<int>(getRed  (pix1)) - getRed  (pix2);
    const int g_diff = static_cast<int>(getGreen(pix1)) - getGreen(pix2);
    const int b_diff = static_cast<int>(getBlue (pix1)) - getBlue (pix2);

    const double k_b = 0.0593; //ITU-R BT.2020 conversion
    const double k_r = 0.2627;
    const double k_g = 1 - k_b - k_r;

    const double scale_b = 0.5 / (1 - k_b);
    const double scale_r = 0.5 / (1 - k_r);

    const double y   = k_r * r_diff + k_g * g_diff + k_b * b_diff;
    const double c_b = scale_b * (b_diff - y);
    const double c_r = scale_r * (r_diff - y);

    return fastSqrt(square(lumaWeight * y) + square(c_b) + square(c_r));
}

// ============================================================================
// Rotation support (from xBRZ)
// ============================================================================

enum RotationDegree { ROT_0, ROT_90, ROT_180, ROT_270 };

template <RotationDegree rotDeg, size_t I, size_t J, size_t N>
struct MatrixRotation;

template <size_t I, size_t J, size_t N>
struct MatrixRotation<ROT_0, I, J, N>
{
    static const size_t I_old = I;
    static const size_t J_old = J;
};

template <RotationDegree rotDeg, size_t I, size_t J, size_t N>
struct MatrixRotation
{
    static const size_t I_old = N - 1 - MatrixRotation<static_cast<RotationDegree>(rotDeg - 1), I, J, N>::J_old;
    static const size_t J_old = MatrixRotation<static_cast<RotationDegree>(rotDeg - 1), I, J, N>::I_old;
};

template <size_t N, RotationDegree rotDeg>
class OutputMatrix
{
public:
    OutputMatrix(uint32_t* out, int outWidth) : out_(out), outWidth_(outWidth) {}

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

// ============================================================================
// Gradient blending
// ============================================================================

template <unsigned int M, unsigned int N> inline
uint32_t gradientRGB(uint32_t pixFront, uint32_t pixBack)
{
    static_assert(0 < M && M < N && N <= 1000);

    auto calcColor = [](unsigned char colFront, unsigned char colBack) -> unsigned char {
        return (colFront * M + colBack * (N - M)) / N;
    };

    return makePixel(calcColor(getRed  (pixFront), getRed  (pixBack)),
                     calcColor(getGreen(pixFront), getGreen(pixBack)),
                     calcColor(getBlue (pixFront), getBlue (pixBack)));
}

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
        return static_cast<unsigned char>((colFront * weightFront + colBack * weightBack) / weightSum);
    };

    return makePixel(static_cast<unsigned char>(weightSum / N),
                     calcColor(getRed  (pixFront), getRed  (pixBack)),
                     calcColor(getGreen(pixFront), getGreen(pixBack)),
                     calcColor(getBlue (pixFront), getBlue (pixBack)));
}

// ============================================================================
// Blend types and preprocessing
// ============================================================================

enum BlendType
{
    BLEND_NONE = 0,
    BLEND_NORMAL,
    BLEND_DOMINANT,
};

struct BlendResult
{
    BlendType blend_f, blend_g, blend_j, blend_k;
};

struct Kernel_4x4
{
    uint32_t a, b, c, d,
             e, f, g, h,
             i, j, k, l,
             m, n, o, p;
};

struct Kernel_3x3
{
    uint32_t a, b, c,
             d, e, f,
             g, h, i;
};

template <class ColorDistance>
FORCE_INLINE
BlendResult preProcessCorners(const Kernel_4x4& ker, const ScalerCfg& cfg)
{
    BlendResult result = {};

    if ((ker.f == ker.g && ker.j == ker.k) ||
        (ker.f == ker.j && ker.g == ker.k))
        return result;

    auto dist = [&](uint32_t pix1, uint32_t pix2) {
        return ColorDistance::dist(pix1, pix2, cfg.luminanceWeight);
    };

    double jg = dist(ker.i, ker.f) + dist(ker.f, ker.c) + dist(ker.n, ker.k) + dist(ker.k, ker.h) + cfg.centerDirectionBias * dist(ker.j, ker.g);
    double fk = dist(ker.e, ker.j) + dist(ker.j, ker.o) + dist(ker.b, ker.g) + dist(ker.g, ker.l) + cfg.centerDirectionBias * dist(ker.f, ker.k);

    if (jg < fk)
    {
        const bool dominantGradient = cfg.dominantDirectionThreshold * jg < fk;
        if (ker.f != ker.g && ker.f != ker.j)
            result.blend_f = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;
        if (ker.k != ker.j && ker.k != ker.g)
            result.blend_k = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;
    }
    else if (fk < jg)
    {
        const bool dominantGradient = cfg.dominantDirectionThreshold * fk < jg;
        if (ker.j != ker.f && ker.j != ker.k)
            result.blend_j = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;
        if (ker.g != ker.f && ker.g != ker.k)
            result.blend_g = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;
    }
    return result;
}

// ============================================================================
// Blend info compression/extraction
// ============================================================================

inline BlendType getTopR   (unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 2)); }
inline BlendType getBottomR(unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 4)); }
inline BlendType getBottomL(unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 6)); }

inline void setTopL   (unsigned char& b, BlendType bt) { b |= bt; }
inline void setTopR   (unsigned char& b, BlendType bt) { b |= (bt << 2); }
inline void setBottomR(unsigned char& b, BlendType bt) { b |= (bt << 4); }
inline void setBottomL(unsigned char& b, BlendType bt) { b |= (bt << 6); }

inline bool blendingNeeded(unsigned char b) { return b != 0; }

template <RotationDegree rotDeg> inline
unsigned char rotateBlendInfo(unsigned char b) { return b; }
template <> inline unsigned char rotateBlendInfo<ROT_90 >(unsigned char b) { return ((b << 2) | (b >> 6)) & 0xff; }
template <> inline unsigned char rotateBlendInfo<ROT_180>(unsigned char b) { return ((b << 4) | (b >> 4)) & 0xff; }
template <> inline unsigned char rotateBlendInfo<ROT_270>(unsigned char b) { return ((b << 6) | (b >> 2)) & 0xff; }

// ============================================================================
// Kernel getters with rotation
// ============================================================================

#define DEF_GETTER(x) template <RotationDegree rotDeg> uint32_t inline get_##x(const Kernel_3x3& ker) { return ker.x; }
DEF_GETTER(a) DEF_GETTER(b) DEF_GETTER(c)
DEF_GETTER(d) DEF_GETTER(e) DEF_GETTER(f)
DEF_GETTER(g) DEF_GETTER(h) DEF_GETTER(i)
#undef DEF_GETTER

#define DEF_GETTER(x, y) template <> inline uint32_t get_##x<ROT_90>(const Kernel_3x3& ker) { return ker.y; }
DEF_GETTER(a, g) DEF_GETTER(b, d) DEF_GETTER(c, a)
DEF_GETTER(d, h) DEF_GETTER(e, e) DEF_GETTER(f, b)
DEF_GETTER(g, i) DEF_GETTER(h, f) DEF_GETTER(i, c)
#undef DEF_GETTER

#define DEF_GETTER(x, y) template <> inline uint32_t get_##x<ROT_180>(const Kernel_3x3& ker) { return ker.y; }
DEF_GETTER(a, i) DEF_GETTER(b, h) DEF_GETTER(c, g)
DEF_GETTER(d, f) DEF_GETTER(e, e) DEF_GETTER(f, d)
DEF_GETTER(g, c) DEF_GETTER(h, b) DEF_GETTER(i, a)
#undef DEF_GETTER

#define DEF_GETTER(x, y) template <> inline uint32_t get_##x<ROT_270>(const Kernel_3x3& ker) { return ker.y; }
DEF_GETTER(a, c) DEF_GETTER(b, f) DEF_GETTER(c, i)
DEF_GETTER(d, b) DEF_GETTER(e, e) DEF_GETTER(f, h)
DEF_GETTER(g, a) DEF_GETTER(h, d) DEF_GETTER(i, g)
#undef DEF_GETTER

// ============================================================================
// Scaler9x structure
// ============================================================================

template <class ColorGradient>
struct Scaler9x : public ColorGradient
{
    static const int scale = 9;

    template <unsigned int M, unsigned int N>
    static void alphaGrad(uint32_t& pixBack, uint32_t pixFront)
    {
        ColorGradient::template alphaGrad<M, N>(pixBack, pixFront);
    }

    template <class OutputMatrix>
    static void blendLineShallow(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<scale - 1, 0>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 2, 2>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 3, 4>(), col);
        alphaGrad<1, 4>(out.template ref<scale - 4, 6>(), col);

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
    }

    template <class OutputMatrix>
    static void blendLineSteep(uint32_t col, OutputMatrix& out)
    {
        alphaGrad<1, 4>(out.template ref<0, scale - 1>(), col);
        alphaGrad<1, 4>(out.template ref<2, scale - 2>(), col);
        alphaGrad<1, 4>(out.template ref<4, scale - 3>(), col);
        alphaGrad<1, 4>(out.template ref<6, scale - 4>(), col);

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
        alphaGrad<1, 2>(out.template ref<scale - 1, scale / 2    >(), col);
        alphaGrad<1, 2>(out.template ref<scale - 2, scale / 2 + 1>(), col);
        alphaGrad<1, 2>(out.template ref<scale - 3, scale / 2 + 2>(), col);
        alphaGrad<1, 2>(out.template ref<scale - 4, scale / 2 + 3>(), col);

        out.template ref<scale - 2, scale - 1>() = col;
        out.template ref<scale - 1, scale - 1>() = col;
        out.template ref<scale - 1, scale - 2>() = col;
        out.template ref<scale - 1, scale - 3>() = col;
        out.template ref<scale - 3, scale - 1>() = col;
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

// ============================================================================
// Color distance policies
// ============================================================================

struct ColorDistanceRGB
{
    static double dist(uint32_t pix1, uint32_t pix2, double luminanceWeight)
    {
        return distYCbCr(pix1, pix2, luminanceWeight);
    }
};

struct ColorDistanceARGB
{
    static double dist(uint32_t pix1, uint32_t pix2, double luminanceWeight)
    {
        const double a1 = getAlpha(pix1) / 255.0;
        const double a2 = getAlpha(pix2) / 255.0;

        const double d = distYCbCr(pix1, pix2, luminanceWeight);
        if (a1 < a2)
            return a1 * d + 255 * (a2 - a1);
        else
            return a2 * d + 255 * (a1 - a2);
    }
};

// ============================================================================
// Color gradient policies
// ============================================================================

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

// ============================================================================
// Pixel blending function
// ============================================================================

template <class Scaler, class ColorDistance, RotationDegree rotDeg>
FORCE_INLINE
void blendPixel(const Kernel_3x3& ker,
                uint32_t* target, int trgWidth,
                unsigned char blendInfo,
                const ScalerCfg& cfg)
{
#define b get_b<rotDeg>(ker)
#define c get_c<rotDeg>(ker)
#define d get_d<rotDeg>(ker)
#define e get_e<rotDeg>(ker)
#define f get_f<rotDeg>(ker)
#define g get_g<rotDeg>(ker)
#define h get_h<rotDeg>(ker)
#define i get_i<rotDeg>(ker)

    const unsigned char blend = rotateBlendInfo<rotDeg>(blendInfo);

    if (getBottomR(blend) >= BLEND_NORMAL)
    {
        auto eq = [&](uint32_t pix1, uint32_t pix2) {
            return ColorDistance::dist(pix1, pix2, cfg.luminanceWeight) < cfg.equalColorTolerance;
        };
        auto dist = [&](uint32_t pix1, uint32_t pix2) {
            return ColorDistance::dist(pix1, pix2, cfg.luminanceWeight);
        };

        const bool doLineBlend = [&]() -> bool
        {
            if (getBottomR(blend) >= BLEND_DOMINANT)
                return true;

            if (getTopR(blend) != BLEND_NONE && !eq(e, g))
                return false;
            if (getBottomL(blend) != BLEND_NONE && !eq(e, c))
                return false;

            if (!eq(e, i) && eq(g, h) && eq(h, i) && eq(i, f) && eq(f, c))
                return false;

            return true;
        }();

        const uint32_t px = dist(e, f) <= dist(e, h) ? f : h;

        OutputMatrix<Scaler::scale, rotDeg> out(target, trgWidth);

        if (doLineBlend)
        {
            const double fg = dist(f, g);
            const double hc = dist(h, c);

            const bool haveShallowLine = cfg.steepDirectionThreshold * fg <= hc && e != g && d != g;
            const bool haveSteepLine   = cfg.steepDirectionThreshold * hc <= fg && e != c && b != c;

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

#undef b
#undef c
#undef d
#undef e
#undef f
#undef g
#undef h
#undef i
}

// ============================================================================
// Main scaling function (based on xbrz.cpp scaleImage)
// ============================================================================

template <class Scaler, class ColorDistance>
void scaleImage(const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, int srcPitch, int trgPitch, const ScalerCfg& cfg, int yFirst, int yLast)
{
    yFirst = std::max(yFirst, 0);
    yLast  = std::min(yLast, srcHeight);
    if (yFirst >= yLast || srcWidth <= 0)
        return;

    int trgWidth = trgPitch;

    const int bufferSize = srcWidth;
    unsigned char* preProcBuffer = reinterpret_cast<unsigned char*>(trg + yLast * Scaler::scale * trgWidth) - bufferSize;
    std::fill(preProcBuffer, preProcBuffer + bufferSize, '\0');
    static_assert(BLEND_NONE == 0);

    if (yFirst > 0)
    {
        const int y = yFirst - 1;

        const uint32_t* s_m1 = src + srcPitch * std::max(y - 1, 0);
        const uint32_t* s_0  = src + srcPitch * y;
        const uint32_t* s_p1 = src + srcPitch * std::min(y + 1, srcHeight - 1);
        const uint32_t* s_p2 = src + srcPitch * std::min(y + 2, srcHeight - 1);

        for (int x = 0; x < srcWidth; ++x)
        {
            const int x_m1 = std::max(x - 1, 0);
            const int x_p1 = std::min(x + 1, srcWidth - 1);
            const int x_p2 = std::min(x + 2, srcWidth - 1);

            Kernel_4x4 ker = {};
            ker.a = s_m1[x_m1]; ker.b = s_m1[x]; ker.c = s_m1[x_p1]; ker.d = s_m1[x_p2];
            ker.e = s_0[x_m1];  ker.f = s_0[x];  ker.g = s_0[x_p1];  ker.h = s_0[x_p2];
            ker.i = s_p1[x_m1]; ker.j = s_p1[x]; ker.k = s_p1[x_p1]; ker.l = s_p1[x_p2];
            ker.m = s_p2[x_m1]; ker.n = s_p2[x]; ker.o = s_p2[x_p1]; ker.p = s_p2[x_p2];

            const BlendResult res = preProcessCorners<ColorDistance>(ker, cfg);
            setTopR(preProcBuffer[x], res.blend_j);

            if (x + 1 < bufferSize)
                setTopL(preProcBuffer[x + 1], res.blend_k);
        }
    }

    for (int y = yFirst; y < yLast; ++y)
    {
        uint32_t* out = trg + Scaler::scale * y * trgWidth;

        const uint32_t* s_m1 = src + srcPitch * std::max(y - 1, 0);
        const uint32_t* s_0  = src + srcPitch * y;
        const uint32_t* s_p1 = src + srcPitch * std::min(y + 1, srcHeight - 1);
        const uint32_t* s_p2 = src + srcPitch * std::min(y + 2, srcHeight - 1);

        unsigned char blend_xy1 = 0;

        for (int x = 0; x < srcWidth; ++x, out += Scaler::scale)
        {
            const int x_m1 = std::max(x - 1, 0);
            const int x_p1 = std::min(x + 1, srcWidth - 1);
            const int x_p2 = std::min(x + 2, srcWidth - 1);

            Kernel_4x4 ker4 = {};
            ker4.a = s_m1[x_m1]; ker4.b = s_m1[x]; ker4.c = s_m1[x_p1]; ker4.d = s_m1[x_p2];
            ker4.e = s_0[x_m1];  ker4.f = s_0[x];  ker4.g = s_0[x_p1];  ker4.h = s_0[x_p2];
            ker4.i = s_p1[x_m1]; ker4.j = s_p1[x]; ker4.k = s_p1[x_p1]; ker4.l = s_p1[x_p2];
            ker4.m = s_p2[x_m1]; ker4.n = s_p2[x]; ker4.o = s_p2[x_p1]; ker4.p = s_p2[x_p2];

            unsigned char blend_xy = 0;
            {
                const BlendResult res = preProcessCorners<ColorDistance>(ker4, cfg);

                blend_xy = preProcBuffer[x];
                setBottomR(blend_xy, res.blend_f);

                setTopR(blend_xy1, res.blend_j);
                preProcBuffer[x] = blend_xy1;

                blend_xy1 = 0;
                setTopL(blend_xy1, res.blend_k);

                if (x + 1 < bufferSize)
                    setBottomL(preProcBuffer[x + 1], res.blend_g);
            }

            fillBlock(out, trgWidth * sizeof(uint32_t), ker4.f, Scaler::scale, Scaler::scale);

            if (blendingNeeded(blend_xy))
            {
                Kernel_3x3 ker3 = {};
                ker3.a = ker4.a; ker3.b = ker4.b; ker3.c = ker4.c;
                ker3.d = ker4.e; ker3.e = ker4.f; ker3.f = ker4.g;
                ker3.g = ker4.i; ker3.h = ker4.j; ker3.i = ker4.k;

                blendPixel<Scaler, ColorDistance, ROT_0  >(ker3, out, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ColorDistance, ROT_90 >(ker3, out, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ColorDistance, ROT_180>(ker3, out, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ColorDistance, ROT_270>(ker3, out, trgWidth, blend_xy, cfg);
            }
        }
    }
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

namespace xbrz9x
{

void scale9x(const uint32_t* src, uint32_t* trg,
             int srcWidth, int srcHeight,
             int srcPitch, int trgPitch,
             bool useAlpha)
{
    if (srcWidth <= 0 || srcHeight <= 0)
        return;

    int srcPPitch = srcPitch / static_cast<int>(sizeof(uint32_t));
    int trgWidth = trgPitch / static_cast<int>(sizeof(uint32_t));

    ScalerCfg cfg;

    if (useAlpha)
        scaleImage<Scaler9x<ColorGradientARGB>, ColorDistanceARGB>(src, trg, srcWidth, srcHeight, srcPPitch, trgWidth, cfg, 0, srcHeight);
    else
        scaleImage<Scaler9x<ColorGradientRGB>, ColorDistanceRGB>(src, trg, srcWidth, srcHeight, srcPPitch, trgWidth, cfg, 0, srcHeight);
}

} // namespace xbrz9x
