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

#include "xbrz.h"
#include <algorithm>
#include <cassert>
#include <cmath> //std::sqrt
#include <vector>
#include "xbrz_tools.h"

using namespace xbrz;


namespace
{
//blend front color with opacity M / N over opaque background: https://en.wikipedia.org/wiki/Alpha_compositing
//Limitation: alpha should be applied in gamma-decoded linear RGB space: https://ssp.impulsetrain.com/gamma-premult.html
template <unsigned int M, unsigned int N> inline
uint32_t gradientRGB(uint32_t pixFront, uint32_t pixBack)
{
    static_assert(0 < M && M < N && N <= 1000);

    auto calcColor = [](unsigned char colFront, unsigned char colBack)
    {
        return static_cast<unsigned char>(uintDivRound(colFront * M + colBack * (N - M), N));
    };

    return makePixel(calcColor(getRed  (pixFront), getRed  (pixBack)),
                     calcColor(getGreen(pixFront), getGreen(pixBack)),
                     calcColor(getBlue (pixFront), getBlue (pixBack)));
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


//inline
//double fastSqrt(double n)
//{
//    __asm //speeds up xBRZ by about 9% compared to std::sqrt which internally uses the same assembler instructions but adds some "fluff"
//    {
//        fld n
//        fsqrt
//    }
//}
//


#ifdef _MSC_VER
    #define FORCE_INLINE __forceinline
#elif defined __GNUC__
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif


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


template <class T> inline
T square(T value) { return value * value; }


#if 0
inline
double distRGB(uint32_t pix1, uint32_t pix2)
{
    const double r_diff = static_cast<int>(getRed  (pix1)) - getRed  (pix2);
    const double g_diff = static_cast<int>(getGreen(pix1)) - getGreen(pix2);
    const double b_diff = static_cast<int>(getBlue (pix1)) - getBlue (pix2);

    //euklidean RGB distance
    return std::sqrt(square(r_diff) + square(g_diff) + square(b_diff));
}
#endif


inline
double distYCbCr(uint32_t pix1, uint32_t pix2, double /*testAttribute*/)
{
    //https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
    //Y'CbCr conversion is a matrix multiplication => take advantage of linearity by subtracting first!
    //NOTE: input is gamma-encoded RGB! => what does this mean for the output distance!??
    const int r_diff = static_cast<int>(getRed  (pix1)) - getRed  (pix2); //defer division by 255 to after matrix multiplication
    const int g_diff = static_cast<int>(getGreen(pix1)) - getGreen(pix2); //
    const int b_diff = static_cast<int>(getBlue (pix1)) - getBlue (pix2); //substraction for int is noticeable faster than for double!

    //const double k_b = 0.0722; //ITU-R BT.709 conversion
    //const double k_r = 0.2126; //
    const double k_b = 0.0593; //ITU-R BT.2020 conversion
    const double k_r = 0.2627; //
    const double k_g = 1 - k_b - k_r;

    const double scale_b = 0.5 / (1 - k_b);
    const double scale_r = 0.5 / (1 - k_r);

    const double y   = k_r * r_diff + k_g * g_diff + k_b * b_diff; //[!], analog YCbCr!
    const double c_b = scale_b * (b_diff - y);
    const double c_r = scale_r * (r_diff - y);

    //we skip division by 255 to have similar range like other distance functions
    return std::sqrt(square(y) + square(c_b) + square(c_r));
}


inline
double distYCbCrBuffered(uint32_t pix1, uint32_t pix2, double /*testAttribute*/)
{
    //30% perf boost compared to plain distYCbCr()!
    //consumes 64 MB memory; using double is only 2% faster, but takes 128 MB
    static const std::vector<float> diffToDist = []
    {
        std::vector<float> tmp;

        for (uint32_t i = 0; i < 256 * 256 * 256; ++i) //startup time: 114 ms on Intel Core i5 (four cores)
        {
            const int r_diff = static_cast<signed char>(getByte<2>(i)) * 2;
            const int g_diff = static_cast<signed char>(getByte<1>(i)) * 2;
            const int b_diff = static_cast<signed char>(getByte<0>(i)) * 2;

            const double k_b = 0.0593; //ITU-R BT.2020 conversion
            const double k_r = 0.2627; //
            const double k_g = 1 - k_b - k_r;

            const double scale_b = 0.5 / (1 - k_b);
            const double scale_r = 0.5 / (1 - k_r);

            const double y   = k_r * r_diff + k_g * g_diff + k_b * b_diff; //[!], analog YCbCr!
            const double c_b = scale_b * (b_diff - y);
            const double c_r = scale_r * (r_diff - y);

            tmp.push_back(static_cast<float>(std::sqrt(square(y) + square(c_b) + square(c_r))));
        }
        return tmp;
    }();

    //if (pix1 == pix2) -> 8% perf degradation!
    //    return 0;
    //if (pix1 < pix2)
    //    std::swap(pix1, pix2); -> 30% perf degradation!!!

    const int r_diff = static_cast<int>(getRed  (pix1)) - getRed  (pix2);
    const int g_diff = static_cast<int>(getGreen(pix1)) - getGreen(pix2);
    const int b_diff = static_cast<int>(getBlue (pix1)) - getBlue (pix2);

    const size_t index = (static_cast<unsigned char>(r_diff / 2) << 16) | //slightly reduce precision (division by 2) to squeeze value into single byte
                         (static_cast<unsigned char>(g_diff / 2) <<  8) |
                         (static_cast<unsigned char>(b_diff / 2));

#if 0 //attention: the following calculation creates an asymmetric color distance!!! (e.g. r_diff=46 will be unpacked as 45, but r_diff=-46 unpacks to -47
    const size_t index = (((r_diff + 0xFF) / 2) << 16) | //slightly reduce precision (division by 2) to squeeze value into single byte
                         (((g_diff + 0xFF) / 2) <<  8) |
                         (( b_diff + 0xFF) / 2);
#endif
    return diffToDist[index];
}


#if defined _MSC_VER && !defined NDEBUG
    const int debugPixelX = -1;
    const int debugPixelY = 58;

    thread_local bool breakIntoDebugger = false;
#endif


enum BlendType
{
    BLEND_NONE = 0,
    BLEND_NORMAL,   //a normal indication to blend
    BLEND_DOMINANT, //a strong indication to blend
    //attention: BlendType must fit into the value range of 2 bit!!!
};

struct BlendResult
{
    BlendType
    blend_e, blend_f,
             blend_h, blend_i;
};


struct Kernel_3x3
{
    uint32_t
    a, b, c,
    d, e, f,
    g, h, i;
};

struct Kernel_4x4 : Kernel_3x3
{
    uint32_t j, k, l, m, n, o, p;
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

template <class ColorDistance>
FORCE_INLINE //detect blend direction
BlendResult preProcessCorners(const Kernel_4x4& ker, const xbrz::ScalerCfg& cfg) //result: E, F, H, I corners of "GradientType"
{
#if defined _MSC_VER && !defined NDEBUG
    if (breakIntoDebugger)
        __debugbreak(); //__asm int 3;
#endif
    if ((ker.e == ker.f &&
         ker.h == ker.i) ||
        (ker.e == ker.h &&
         ker.f == ker.i))
        return {};

    auto dist = [&](uint32_t pix1, uint32_t pix2) { return ColorDistance::dist(pix1, pix2, cfg.testAttribute); };

    const double hf = dist(ker.g, ker.e) + dist(ker.e, ker.c) + dist(ker.k, ker.i) + dist(ker.i, ker.o) + cfg.centerDirectionBias * dist(ker.h, ker.f);
    const double ei = dist(ker.d, ker.h) + dist(ker.h, ker.l) + dist(ker.b, ker.f) + dist(ker.f, ker.n) + cfg.centerDirectionBias * dist(ker.e, ker.i);

    BlendResult result = {};
    if (hf < ei) //test sample: 70% of values max(hf, ei) / min(hf, ei) are between 1.1 and 3.7 with median being 1.8
    {
        const bool dominantGradient = cfg.dominantDirectionThreshold * hf < ei;
        if (ker.e != ker.f && ker.e != ker.h)
            result.blend_e = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;

        if (ker.i != ker.h && ker.i != ker.f)
            result.blend_i = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;
    }
    else if (ei < hf)
    {
        const bool dominantGradient = cfg.dominantDirectionThreshold * ei < hf;
        if (ker.h != ker.e && ker.h != ker.i)
            result.blend_h = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;

        if (ker.f != ker.e && ker.f != ker.i)
            result.blend_f = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;
    }
    return result;
}

#define DEF_GETTER(x) template <RotationDegree rotDeg> uint32_t inline get_##x(const Kernel_3x3& ker) { return ker.x; }
//we cannot and NEED NOT write "ker.##x" since ## concatenates preprocessor tokens but "." is not a token
DEF_GETTER(b) DEF_GETTER(c)
DEF_GETTER(d) DEF_GETTER(e) DEF_GETTER(f)
DEF_GETTER(g) DEF_GETTER(h) DEF_GETTER(i)
#undef DEF_GETTER

#define DEF_GETTER(x, y) template <> inline uint32_t get_##x<ROT_90>(const Kernel_3x3& ker) { return ker.y; }
DEF_GETTER(b, d) DEF_GETTER(c, a)
DEF_GETTER(d, h) DEF_GETTER(e, e) DEF_GETTER(f, b)
DEF_GETTER(g, i) DEF_GETTER(h, f) DEF_GETTER(i, c)
#undef DEF_GETTER

#define DEF_GETTER(x, y) template <> inline uint32_t get_##x<ROT_180>(const Kernel_3x3& ker) { return ker.y; }
DEF_GETTER(b, h) DEF_GETTER(c, g)
DEF_GETTER(d, f) DEF_GETTER(e, e) DEF_GETTER(f, d)
DEF_GETTER(g, c) DEF_GETTER(h, b) DEF_GETTER(i, a)
#undef DEF_GETTER

#define DEF_GETTER(x, y) template <> inline uint32_t get_##x<ROT_270>(const Kernel_3x3& ker) { return ker.y; }
DEF_GETTER(b, f) DEF_GETTER(c, i)
DEF_GETTER(d, b) DEF_GETTER(e, e) DEF_GETTER(f, h)
DEF_GETTER(g, a) DEF_GETTER(h, d) DEF_GETTER(i, g)
#undef DEF_GETTER


//compress four blend types into a single byte
//inline BlendType getTopL   (unsigned char b) { return static_cast<BlendType>(0x3 & b); }
inline BlendType getTopR   (unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 2)); }
inline BlendType getBottomR(unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 4)); }
inline BlendType getBottomL(unsigned char b) { return static_cast<BlendType>(0x3 & (b >> 6)); }

inline void clearAddTopL(unsigned char& b, BlendType bt) { b = static_cast<unsigned char>(bt); }
inline void addTopR     (unsigned char& b, BlendType bt) { b |= (bt << 2); } //buffer is assumed to be initialized before preprocessing!
inline void addBottomR  (unsigned char& b, BlendType bt) { b |= (bt << 4); } //e.g. via clearAddTopL()
inline void addBottomL  (unsigned char& b, BlendType bt) { b |= (bt << 6); } //

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

template <class Scaler, class ColorDistance, RotationDegree rotDeg>
FORCE_INLINE //perf: quite worth it!
void blendPixel(const Kernel_3x3& ker,
                uint32_t* target, int trgWidth,
                unsigned char blendInfo, //result of preprocessing all four corners of pixel "E"
                const xbrz::ScalerCfg& cfg)
{
#define b get_b<rotDeg>(ker)
#define c get_c<rotDeg>(ker)
#define d get_d<rotDeg>(ker)
#define e get_e<rotDeg>(ker)
#define f get_f<rotDeg>(ker)
#define g get_g<rotDeg>(ker)
#define h get_h<rotDeg>(ker)
#define i get_i<rotDeg>(ker)

#if defined _MSC_VER && !defined NDEBUG
    if (breakIntoDebugger)
        __debugbreak(); //__asm int 3;
#endif

    const unsigned char blend = rotateBlendInfo<rotDeg>(blendInfo);

    if (getBottomR(blend) >= BLEND_NORMAL)
    {
        auto eq   = [&](uint32_t pix1, uint32_t pix2) { return ColorDistance::dist(pix1, pix2, cfg.testAttribute) < cfg.equalColorTolerance; };
        auto dist = [&](uint32_t pix1, uint32_t pix2) { return ColorDistance::dist(pix1, pix2, cfg.testAttribute); };

        const bool doLineBlend = [&]  // MSVC workaround: remove -> bool
        {
            if (getBottomR(blend) >= BLEND_DOMINANT)
                return true;

            //make sure there is no second blending in an adjacent rotation for this pixel: handles insular pixels, mario eyes
            if (getTopR(blend) != BLEND_NONE && !eq(e, g)) //but support double-blending for 90° corners
                return false;
            if (getBottomL(blend) != BLEND_NONE && !eq(e, c))
                return false;

            //no full blending for L-shapes; blend corner only (handles "mario mushroom eyes")
            if (!eq(e, i) && eq(g, h) && eq(h, i) && eq(i, f) && eq(f, c))
                return false;

            return true;
        }();

        const uint32_t px = dist(e, f) <= dist(e, h) ? f : h; //choose most similar color

        OutputMatrix<Scaler::scale, rotDeg> out(target, trgWidth);

        if (doLineBlend)
        {
            const double fg = dist(f, g); //test sample: 70% of values max(fg, hc) / min(fg, hc) are between 1.1 and 3.7 with median being 1.9
            const double hc = dist(h, c); //

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

    //#undef a
#undef b
#undef c
#undef d
#undef e
#undef f
#undef g
#undef h
#undef i
}


class OobReaderTransparent
{
public:
    OobReaderTransparent(const uint32_t* src, int srcWidth, int srcHeight, int srcPitch, int y) :
        s_m1(0 <= y - 1 && y - 1 < srcHeight ? src + srcPitch * (y - 1) : nullptr),
        s_0 (0 <= y     && y     < srcHeight ? src + srcPitch *  y      : nullptr),
        s_p1(0 <= y + 1 && y + 1 < srcHeight ? src + srcPitch * (y + 1) : nullptr),
        s_p2(0 <= y + 2 && y + 2 < srcHeight ? src + srcPitch * (y + 2) : nullptr),
        srcWidth_(srcWidth) {}

    void readPonm(Kernel_4x4& ker, int x) const //(x, y) is at kernel position E
    {
        [[likely]] if (const int x_p2 = x + 2; 0 <= x_p2 && x_p2 < srcWidth_)
        {
            ker.p = s_m1 ? s_m1[x_p2] : 0;
            ker.o = s_0  ? s_0 [x_p2] : 0;
            ker.n = s_p1 ? s_p1[x_p2] : 0;
            ker.m = s_p2 ? s_p2[x_p2] : 0;
        }
        else
        {
            ker.p = 0;
            ker.o = 0;
            ker.n = 0;
            ker.m = 0;
        }
    }

private:
    const uint32_t* const s_m1;
    const uint32_t* const s_0;
    const uint32_t* const s_p1;
    const uint32_t* const s_p2;
    const int srcWidth_;
};


class OobReaderDuplicate
{
public:
    OobReaderDuplicate(const uint32_t* src, int srcWidth, int srcHeight, int srcPitch, int y) :
        s_m1(src + srcPitch * std::clamp(y - 1, 0, srcHeight - 1)),
        s_0 (src + srcPitch * std::clamp(y,     0, srcHeight - 1)),
        s_p1(src + srcPitch * std::clamp(y + 1, 0, srcHeight - 1)),
        s_p2(src + srcPitch * std::clamp(y + 2, 0, srcHeight - 1)),
        srcWidth_(srcWidth) {}

    void readPonm(Kernel_4x4& ker, int x) const //(x, y) is at kernel position E
    {
        const int x_p2 = std::clamp(x + 2, 0, srcWidth_ - 1);
        ker.p = s_m1[x_p2];
        ker.o = s_0 [x_p2];
        ker.n = s_p1[x_p2];
        ker.m = s_p2[x_p2];
    }

private:
    const uint32_t* const s_m1;
    const uint32_t* const s_0;
    const uint32_t* const s_p1;
    const uint32_t* const s_p2;
    const int srcWidth_;
};


inline
void fillBlock(uint32_t* trg, int trgWidth, uint32_t col, int blockSize)
{
    for (int y = 0; y < blockSize; ++y, trg += trgWidth)
        //    std::fill(trg, trg + blockSize, col);
        for (int x = 0; x < blockSize; ++x)
            trg[x] = col;
}


template <class Scaler, class ColorDistance, class OobReader> //scaler policy: see "Scaler2x" reference implementation
void scaleImage(const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, int srcPitch, int trgPitch, const xbrz::ScalerCfg& cfg, int yFirst, int yLast)
{
    yFirst = std::max(yFirst, 0);
    yLast  = std::min(yLast, srcHeight);
    if (yFirst >= yLast || srcWidth <= 0)
        return;

    const int trgWidth = trgPitch;

    //(ab)use space of "sizeof(uint32_t) * srcWidth * Scaler::scale" at the end of the image as temporary
    //buffer for "on the fly preprocessing" without risk of accidental overwriting before accessing
    unsigned char* const preProcBuf = reinterpret_cast<unsigned char*>(trg + yLast * Scaler::scale * trgWidth) - srcWidth;

    //initialize preprocessing buffer for first row of current stripe: detect upper left and right corner blending
    //this cannot be optimized for adjacent processing stripes; we must not allow for a memory race condition!
    {
        const OobReader oobReader(src, srcWidth, srcHeight, srcPitch, yFirst - 1);

        //initialize at position x = -1
        Kernel_4x4 ker4 = {};
        oobReader.readPonm(ker4, -4); //hack: read a, d, g, j at x = -1
        ker4.a = ker4.p;
        ker4.d = ker4.o;
        ker4.g = ker4.n;
        ker4.j = ker4.m;

        oobReader.readPonm(ker4, -3);
        ker4.b = ker4.p;
        ker4.e = ker4.o;
        ker4.h = ker4.n;
        ker4.k = ker4.m;

        oobReader.readPonm(ker4, -2);
        ker4.c = ker4.p;
        ker4.f = ker4.o;
        ker4.i = ker4.n;
        ker4.l = ker4.m;

        oobReader.readPonm(ker4, -1);

        {
            const BlendResult res = preProcessCorners<ColorDistance>(ker4, cfg);
            clearAddTopL(preProcBuf[0], res.blend_i); //set 1st known corner for (0, yFirst)
        }

        for (int x = 0; x < srcWidth; ++x)
        {
            ker4.a = ker4.b;    //shift previous kernel to the left
            ker4.d = ker4.e;    // -----------------
            ker4.g = ker4.h;    // | A | B | C | P |
            ker4.j = ker4.k;    // |---|---|---|---|
            /**/                // | D | E | F | O | (x, yFirst - 1) is at position E
            ker4.b = ker4.c;    // |---|---|---|---|
            ker4.e = ker4.f;    // | G | H | I | N |
            ker4.h = ker4.i;    // |---|---|---|---|
            ker4.k = ker4.l;    // | J | K | L | M |
            /**/                // -----------------
            ker4.c = ker4.p;
            ker4.f = ker4.o;
            ker4.i = ker4.n;
            ker4.l = ker4.m;

            oobReader.readPonm(ker4, x);

            /*  preprocessing blend result:
                ---------
                | E | F |   evaluate corner between E, F, H, I
                |---+---|   current input pixel is at position E
                | H | I |
                ---------                                        */
            const BlendResult res = preProcessCorners<ColorDistance>(ker4, cfg);
            addTopR(preProcBuf[x], res.blend_h); //set 2nd known corner for (x, yFirst)

            if (x + 1 < srcWidth)
                clearAddTopL(preProcBuf[x + 1], res.blend_i); //set 1st known corner for (x + 1, yFirst)
        }
    }
    //------------------------------------------------------------------------------------

    for (int y = yFirst; y < yLast; ++y)
    {
        uint32_t* out = trg + Scaler::scale * y * trgWidth; //consider MT "striped" access

        const OobReader oobReader(src, srcWidth, srcHeight, srcPitch, y);

        //initialize at position x = -1
        Kernel_4x4 ker4 = {};
        oobReader.readPonm(ker4, -4); //hack: read a, d, g, j at x = -1
        ker4.a = ker4.p;
        ker4.d = ker4.o;
        ker4.g = ker4.n;
        ker4.j = ker4.m;

        oobReader.readPonm(ker4, -3);
        ker4.b = ker4.p;
        ker4.e = ker4.o;
        ker4.h = ker4.n;
        ker4.k = ker4.m;

        oobReader.readPonm(ker4, -2);
        ker4.c = ker4.p;
        ker4.f = ker4.o;
        ker4.i = ker4.n;
        ker4.l = ker4.m;

        oobReader.readPonm(ker4, -1);

        unsigned char blend_xy1 = 0; //corner blending for current (x, y + 1) position
        {
            const BlendResult res = preProcessCorners<ColorDistance>(ker4, cfg);
            clearAddTopL(blend_xy1, res.blend_i); //set 1st known corner for (0, y + 1) and buffer for use on next column

            addBottomL(preProcBuf[0], res.blend_f); //set 3rd known corner for (0, y)
        }

        for (int x = 0; x < srcWidth; ++x, out += Scaler::scale)
        {
#if defined _MSC_VER && !defined NDEBUG
            breakIntoDebugger = debugPixelX == x && debugPixelY == y;
#endif
            ker4.a = ker4.b;    //shift previous kernel to the left
            ker4.d = ker4.e;    // -----------------
            ker4.g = ker4.h;    // | A | B | C | P |
            ker4.j = ker4.k;    // |---|---|---|---|
            /**/                // | D | E | F | O | (x, y) is at position E
            ker4.b = ker4.c;    // |---|---|---|---|
            ker4.e = ker4.f;    // | G | H | I | N |
            ker4.h = ker4.i;    // |---|---|---|---|
            ker4.k = ker4.l;    // | J | K | L | M |
            /**/                // -----------------
            ker4.c = ker4.p;
            ker4.f = ker4.o;
            ker4.i = ker4.n;
            ker4.l = ker4.m;

            oobReader.readPonm(ker4, x);

            //evaluate the four corners on bottom-right of current pixel
            unsigned char blend_xy = preProcBuf[x]; //for current (x, y) position
            {
                /*  preprocessing blend result:
                    ---------
                    | E | F |   evaluate corner between E, F, H, I
                    |---+---|   current input pixel is at position E
                    | H | I |
                    ---------                                        */
                const BlendResult res = preProcessCorners<ColorDistance>(ker4, cfg);
                addBottomR(blend_xy, res.blend_e); //all four corners of (x, y) have been determined at this point due to processing sequence!

                addTopR(blend_xy1, res.blend_h); //set 2nd known corner for (x, y + 1)
                preProcBuf[x] = blend_xy1; //store on current buffer position for use on next row

                [[likely]] if (x + 1 < srcWidth)
                {
                    //blend_xy1 -> blend_x1y1
                    clearAddTopL(blend_xy1, res.blend_i); //set 1st known corner for (x + 1, y + 1) and buffer for use on next column

                    addBottomL(preProcBuf[x + 1], res.blend_f); //set 3rd known corner for (x + 1, y)
                }
            }

            //fill block of size scale * scale with the given color
            fillBlock(out, trgWidth, ker4.e, Scaler::scale);

            //place *after* preprocessing step, to not overwrite the results while processing the last pixel!

            //blend all four corners of current pixel
            if (blendingNeeded(blend_xy))
            {
                const Kernel_3x3& ker3 = ker4; //"The Things We Do for [Perf]"
                blendPixel<Scaler, ColorDistance, ROT_0  >(ker3, out, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ColorDistance, ROT_90 >(ker3, out, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ColorDistance, ROT_180>(ker3, out, trgWidth, blend_xy, cfg);
                blendPixel<Scaler, ColorDistance, ROT_270>(ker3, out, trgWidth, blend_xy, cfg);
            }
        }
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

//------------------------------------------------------------------------------------

struct ColorDistanceRGB
{
    static double dist(uint32_t pix1, uint32_t pix2, double testAttribute)
    {
        return distYCbCrBuffered(pix1, pix2, testAttribute);

        //if (pix1 == pix2) //about 4% perf boost
        //    return 0;
        //return distYCbCr(pix1, pix2, luminanceWeight);
    }
};

struct ColorDistanceARGB
{
    static double dist(uint32_t pix1, uint32_t pix2, double testAttribute)
    {
        const double a1 = getAlpha(pix1) / 255.0 ;
        const double a2 = getAlpha(pix2) / 255.0 ;

        /*  Requirements for a color distance handling alpha channel: with a1, a2 in [0, 1]

            1. if a1 = a2, distance should be: a1 * distYCbCr()
            2. if a1 = 0,  distance should be: a2 * distYCbCr(black, white) = a2 * 255
            3. if a1 = 1,  ??? maybe: 255 * (1 - a2) + a2 * distYCbCr()

            std::min(a1, a2) * distYCbCrBuffered(pix1, pix2) + 255 * abs(a1 - a2);

            alternative? std::sqrt(a1 * a2 * square(distYCbCrBuffered(pix1, pix2)) + square(255 * (a1 - a2)));   */

        //=> following code is 15% faster:
        const double d = distYCbCrBuffered(pix1, pix2, testAttribute);
        if (a1 < a2)
            return a1 * d + 255 * (a2 - a1);
        else
            return a2 * d + 255 * (a1 - a2);
    }
};


struct ColorDistanceUnbufferedARGB
{
    static double dist(uint32_t pix1, uint32_t pix2, double testAttribute)
    {
        const double a1 = getAlpha(pix1) / 255.0 ;
        const double a2 = getAlpha(pix2) / 255.0 ;

        const double d = distYCbCr(pix1, pix2, testAttribute);
        if (a1 < a2)
            return a1 * d + 255 * (a2 - a1);
        else
            return a2 * d + 255 * (a1 - a2);
    }
};


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
        for (int y = yFirst; y < yLast; ++y)
        {
            const uint32_t* srcLine = src + y * srcPitchPixels;
            uint32_t* trgLine = trg + y * trgPitchPixels;
            std::copy(srcLine, srcLine + srcWidth, trgLine);
        }
        return;
    }

    static_assert(SCALE_FACTOR_MAX == 6);
    switch (colFmt)
    {
        case ColorFormat::rgb:
            switch (factor)
            {
                case 2: return scaleImage<Scaler2x<ColorGradientRGB>, ColorDistanceRGB, OobReaderDuplicate>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 3: return scaleImage<Scaler3x<ColorGradientRGB>, ColorDistanceRGB, OobReaderDuplicate>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 4: return scaleImage<Scaler4x<ColorGradientRGB>, ColorDistanceRGB, OobReaderDuplicate>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 5: return scaleImage<Scaler5x<ColorGradientRGB>, ColorDistanceRGB, OobReaderDuplicate>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 6: return scaleImage<Scaler6x<ColorGradientRGB>, ColorDistanceRGB, OobReaderDuplicate>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
            }
            break;

        case ColorFormat::argb:
            switch (factor)
            {
                case 2: return scaleImage<Scaler2x<ColorGradientARGB>, ColorDistanceARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 3: return scaleImage<Scaler3x<ColorGradientARGB>, ColorDistanceARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 4: return scaleImage<Scaler4x<ColorGradientARGB>, ColorDistanceARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 5: return scaleImage<Scaler5x<ColorGradientARGB>, ColorDistanceARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 6: return scaleImage<Scaler6x<ColorGradientARGB>, ColorDistanceARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
            }
            break;

        case ColorFormat::argbUnbuffered:
            switch (factor)
            {
                case 2: return scaleImage<Scaler2x<ColorGradientARGB>, ColorDistanceUnbufferedARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 3: return scaleImage<Scaler3x<ColorGradientARGB>, ColorDistanceUnbufferedARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 4: return scaleImage<Scaler4x<ColorGradientARGB>, ColorDistanceUnbufferedARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 5: return scaleImage<Scaler5x<ColorGradientARGB>, ColorDistanceUnbufferedARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
                case 6: return scaleImage<Scaler6x<ColorGradientARGB>, ColorDistanceUnbufferedARGB, OobReaderTransparent>(src, trg, srcWidth, srcHeight, srcPitchPixels, trgPitchPixels, cfg, yFirst, yLast);
            }
            break;
    }
    assert(false);
}


bool xbrz::equalColorTest2(uint32_t col1, uint32_t col2, ColorFormat colFmt, double equalColorTolerance, double testAttribute)
{
    switch (colFmt)
    {
        case ColorFormat::rgb:
            return ColorDistanceRGB::dist(col1, col2, testAttribute) < equalColorTolerance;
        case ColorFormat::argb:
            return ColorDistanceARGB::dist(col1, col2, testAttribute) < equalColorTolerance;
        case ColorFormat::argbUnbuffered:
            return ColorDistanceUnbufferedARGB::dist(col1, col2, testAttribute) < equalColorTolerance;
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


#if 0
//#include <ppl.h>
void bilinearScaleCpu(const uint32_t* src, int srcWidth, int srcHeight,
                      /**/  uint32_t* trg, int trgWidth, int trgHeight)
{
    const int TASK_GRANULARITY = 16;

    concurrency::task_group tg;

    for (int i = 0; i < trgHeight; i += TASK_GRANULARITY)
        tg.run([=]
    {
        const int iLast = std::min(i + TASK_GRANULARITY, trgHeight);
        bilinearScale(src, srcWidth, srcHeight, srcWidth * sizeof(uint32_t),
                      trg, trgWidth, trgHeight, trgWidth * sizeof(uint32_t),
        i, iLast, [](uint32_t pix) { return pix; });
    });
    tg.wait();
}


//Perf: AMP vs CPU: merely ~10% shorter runtime (scaling 1280x800 -> 1920x1080)
//#include <amp.h>
void bilinearScaleAmp(const uint32_t* src, int srcWidth, int srcHeight, //throw concurrency::runtime_exception
                      /**/  uint32_t* trg, int trgWidth, int trgHeight)
{
    //C++ AMP reference:       https://docs.microsoft.com/en-us/cpp/parallel/amp/reference/reference-cpp-amp
    //introduction to C++ AMP: https://docs.microsoft.com/en-us/archive/msdn-magazine/2012/april/c-a-code-based-introduction-to-c-amp
    using namespace concurrency;
    //TODO: pitch

    if (srcHeight <= 0 || srcWidth <= 0) return;

    const float scaleX = static_cast<float>(trgWidth ) / srcWidth;
    const float scaleY = static_cast<float>(trgHeight) / srcHeight;

    array_view<const uint32_t, 2> srcView(srcHeight, srcWidth, src);
    array_view<      uint32_t, 2> trgView(trgHeight, trgWidth, trg);
    trgView.discard_data();

    parallel_for_each(trgView.extent, [=](index<2> idx) restrict(amp) //throw ?
    {
        const int y = idx[0];
        const int x = idx[1];
        //Perf notes:
        //    -> float-based calculation is (almost) 2x as fas as double!
        //    -> no noticeable improvement via tiling: https://docs.microsoft.com/en-us/archive/msdn-magazine/2012/april/c-amp-introduction-to-tiling-in-c-amp
        //    -> no noticeable improvement with restrict(amp,cpu)
        //    -> iterating over y-axis only is significantly slower!
        //    -> pre-calculating x,y-dependent variables in a buffer + array_view<> is ~ 20 % slower!
        const int y1 = srcHeight * y / trgHeight;
        int y2 = y1 + 1;
        if (y2 == srcHeight) --y2;

        const float yy1 = y / scaleY - y1;
        const float y2y = 1 - yy1;
        //-------------------------------------
        const int x1 = srcWidth * x / trgWidth;
        int x2 = x1 + 1;
        if (x2 == srcWidth) --x2;

        const float xx1 = x / scaleX - x1;
        const float x2x = 1 - xx1;
        //-------------------------------------
        const float x2xy2y = x2x * y2y;
        const float xx1y2y = xx1 * y2y;
        const float x2xyy1 = x2x * yy1;
        const float xx1yy1 = xx1 * yy1;

        auto interpolate = [=](int offset)
        {
            /*
                https://en.wikipedia.org/wiki/Bilinear_interpolation
                (c11(x2 - x) + c21(x - x1)) * (y2 - y ) +
                (c12(x2 - x) + c22(x - x1)) * (y  - y1)
            */
            const auto c11 = (srcView(y1, x1) >> (8 * offset)) & 0xff;
            const auto c21 = (srcView(y1, x2) >> (8 * offset)) & 0xff;
            const auto c12 = (srcView(y2, x1) >> (8 * offset)) & 0xff;
            const auto c22 = (srcView(y2, x2) >> (8 * offset)) & 0xff;

            return c11 * x2xy2y + c21 * xx1y2y +
                   c12 * x2xyy1 + c22 * xx1yy1;
        };

        const float bi = interpolate(0);
        const float gi = interpolate(1);
        const float ri = interpolate(2);
        const float ai = interpolate(3);

        const auto b = static_cast<uint32_t>(bi + 0.5f);
        const auto g = static_cast<uint32_t>(gi + 0.5f);
        const auto r = static_cast<uint32_t>(ri + 0.5f);
        const auto a = static_cast<uint32_t>(ai + 0.5f);

        trgView(y, x) = (a << 24) | (r << 16) | (g << 8) | b;
    });
    trgView.synchronize(); //throw ?
}
#endif
