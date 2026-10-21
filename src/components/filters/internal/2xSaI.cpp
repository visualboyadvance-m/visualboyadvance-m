// 2xSaI, Super 2xSaI and Super Eagle, 16 and 32 bits per pixel.
//
// Each filter is the classic decision tree of Derek Liauw Kie Fa's 2xSaI over a
// 4x4 neighbourhood. The scalar kernels keep that tree as written; the vector
// kernels (SSE2 / NEON, see simd128.h) evaluate every branch for a lane group
// of 4 (32-bit) or 8 (16-bit) pixels and pick the results with masks. The test
// suite checks that the two agree pixel for pixel.
//
// Interpolation: 32-bit pixels are averaged per 8-bit channel, rounding down,
// the unused upper byte comes out zero. 16-bit pixels use the channel masks
// of Init_2xSaI() (the same arithmetic as the MMX assembly), so the two low
// bits of a channel no longer bleed into the neighbouring channel.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../simd128.h"
#include "core/base/system.h"

extern int RGB_LOW_BITS_MASK;

extern "C"
{

#ifdef MMX
  void _2xSaILine (uint8_t *srcPtr, uint8_t *deltaPtr, uint32_t srcPitch,
                   uint32_t width, uint8_t *dstPtr, uint32_t dstPitch);
  void _2xSaISuperEagleLine (uint8_t *srcPtr, uint8_t *deltaPtr,
                             uint32_t srcPitch, uint32_t width,
                             uint8_t *dstPtr, uint32_t dstPitch);
  void _2xSaISuper2xSaILine (uint8_t *srcPtr, uint8_t *deltaPtr,
                             uint32_t srcPitch, uint32_t width,
                             uint8_t *dstPtr, uint32_t dstPitch);
    void Init_2xSaIMMX (uint32_t BitFormat);
  void BilinearMMX (uint16_t * A, uint16_t * B, uint16_t * C, uint16_t * D,
                    uint16_t * dx, uint16_t * dy, uint8_t *dP);
  void BilinearMMXGrid0 (uint16_t * A, uint16_t * B, uint16_t * C, uint16_t * D,
                         uint16_t * dx, uint16_t * dy, uint8_t *dP);
  void BilinearMMXGrid1 (uint16_t * A, uint16_t * B, uint16_t * C, uint16_t * D,
                         uint16_t * dx, uint16_t * dy, uint8_t *dP);
  void EndMMX ();

  bool cpu_mmx = 1;
#endif
}
static uint32_t redblueMask = 0xF81F;
static uint32_t greenMask = 0x7E0;

uint32_t qRGB_COLOR_MASK[2] = { 0xF7DEF7DE, 0xF7DEF7DE };

extern void hq2x_init(unsigned);

int Init_2xSaI(uint32_t BitFormat)
{
  if(systemColorDepth == 16) {
    if (BitFormat == 565) {
      redblueMask = 0xF81F;
      greenMask = 0x7E0;
      qRGB_COLOR_MASK[0] = qRGB_COLOR_MASK[1] = 0xF7DEF7DE;
      hq2x_init(16);
      RGB_LOW_BITS_MASK = 0x0821;
    } else if (BitFormat == 555) {
      redblueMask = 0x7C1F;
      greenMask = 0x3E0;
      qRGB_COLOR_MASK[0] = qRGB_COLOR_MASK[1] = 0x7BDE7BDE;
      hq2x_init(15);
      RGB_LOW_BITS_MASK = 0x0421;
    } else {
      return 0;
    }
  } else if(systemColorDepth == 32) {
    qRGB_COLOR_MASK[0] = qRGB_COLOR_MASK[1] = 0xfefefe;
    hq2x_init(32);
    RGB_LOW_BITS_MASK = 0x010101;
  } else
    return 0;

#ifdef MMX
    Init_2xSaIMMX (BitFormat);
#endif

  return 1;
}

namespace {

using namespace vbam_simd;

// ===========================================================================
// Scalar pixel arithmetic
// ===========================================================================

// 16-bit channel masks, from the globals Init_2xSaI() sets
struct Masks16 {
    uint16_t color;   // low bit of every channel clear:  (p & color) >> 1 halves the channels
    uint16_t low;     // the low bit of every channel
    uint16_t qcolor;  // two low bits of every channel clear
    uint16_t qlow;    // the two low bits of every channel

    static Masks16 Current() {
        const uint16_t low = static_cast<uint16_t>(RGB_LOW_BITS_MASK);
        const uint16_t color = static_cast<uint16_t>(qRGB_COLOR_MASK[0] & 0xffff);
        return {color, low, static_cast<uint16_t>(color & ~(low << 1)), static_cast<uint16_t>(low | (low << 1))};
    }
};

struct Scalar32 {
    using Pixel = uint32_t;

    Scalar32() {}

    static Pixel Interp(Pixel a, Pixel b) {
        const uint32_t r = (((a >> 16) & 0xff) + ((b >> 16) & 0xff)) >> 1;
        const uint32_t g = (((a >> 8) & 0xff) + ((b >> 8) & 0xff)) >> 1;
        const uint32_t bl = ((a & 0xff) + (b & 0xff)) >> 1;
        return (r << 16) | (g << 8) | bl;
    }

    static Pixel QInterp(Pixel a, Pixel b, Pixel c, Pixel d) {
        const uint32_t r = (((a >> 16) & 0xff) + ((b >> 16) & 0xff) + ((c >> 16) & 0xff) + ((d >> 16) & 0xff)) >> 2;
        const uint32_t g = (((a >> 8) & 0xff) + ((b >> 8) & 0xff) + ((c >> 8) & 0xff) + ((d >> 8) & 0xff)) >> 2;
        const uint32_t bl = ((a & 0xff) + (b & 0xff) + (c & 0xff) + (d & 0xff)) >> 2;
        return (r << 16) | (g << 8) | bl;
    }
};

struct Scalar16 {
    using Pixel = uint16_t;
    Masks16 m;

    Scalar16() : m(Masks16::Current()) {}

    Pixel Interp(Pixel a, Pixel b) const {
        return static_cast<Pixel>(((a & m.color) >> 1) + ((b & m.color) >> 1) + (a & b & m.low));
    }

    Pixel QInterp(Pixel a, Pixel b, Pixel c, Pixel d) const {
        const uint32_t hi = ((a & m.qcolor) >> 2) + ((b & m.qcolor) >> 2) + ((c & m.qcolor) >> 2) + ((d & m.qcolor) >> 2);
        const uint32_t lo = (((a & m.qlow) + (b & m.qlow) + (c & m.qlow) + (d & m.qlow)) >> 2) & m.qlow;
        return static_cast<Pixel>(hi + lo);
    }
};

static inline int GetResult(uint32_t A, uint32_t B, uint32_t C, uint32_t D)
{
  int x = 0;
  int y = 0;
  int r = 0;

  if (A == C)
    x += 1;
  else if (B == C)
    y += 1;
  if (A == D)
    x += 1;
  else if (B == D)
    y += 1;
  if (x <= 1)
    r += 1;
  if (y <= 1)
    r -= 1;
  return r;
}

// Clamped read: the neighbourhood of edge pixels repeats the edge
template <typename Pixel>
static inline Pixel GetPixel(const Pixel* src, int srcPitchPixels, int x, int y, int width, int height) {
    x = (x < 0) ? 0 : (x >= width) ? width - 1 : x;
    y = (y < 0) ? 0 : (y >= height) ? height - 1 : y;
    return src[y * srcPitchPixels + x];
}

// ===========================================================================
// Scalar kernels: the original decision trees
// ===========================================================================

template <class S>
void Super2xSaIScalar(const S& s, const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch,
                      int width, int height) {
  using Pixel = typename S::Pixel;
  const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr);
  const int srcPitchPixels = static_cast<int>(srcPitch / sizeof(Pixel));

  for (int y = 0; y < height; y++) {
    Pixel* dP0 = reinterpret_cast<Pixel*>(dstPtr + y * (dstPitch << 1));
    Pixel* dP1 = reinterpret_cast<Pixel*>(dstPtr + y * (dstPitch << 1) + dstPitch);

    for (int x = 0; x < width; x++) {
      Pixel color4, color5, color6;
      Pixel color1, color2, color3;
      Pixel colorA0, colorA1, colorA2, colorA3,
        colorB0, colorB1, colorB2, colorB3, colorS1, colorS2;
      Pixel product1a, product1b, product2a, product2b;

      //---------------------------------------    B1 B2
      //                                         4  5  6 S2
      //                                         1  2  3 S1
      //                                           A1 A2

      colorB0 = GetPixel(src, srcPitchPixels, x - 1, y - 1, width, height);
      colorB1 = GetPixel(src, srcPitchPixels, x,     y - 1, width, height);
      colorB2 = GetPixel(src, srcPitchPixels, x + 1, y - 1, width, height);
      colorB3 = GetPixel(src, srcPitchPixels, x + 2, y - 1, width, height);

      color4 = GetPixel(src, srcPitchPixels, x - 1, y, width, height);
      color5 = GetPixel(src, srcPitchPixels, x,     y, width, height);
      color6 = GetPixel(src, srcPitchPixels, x + 1, y, width, height);
      colorS2 = GetPixel(src, srcPitchPixels, x + 2, y, width, height);

      color1 = GetPixel(src, srcPitchPixels, x - 1, y + 1, width, height);
      color2 = GetPixel(src, srcPitchPixels, x,     y + 1, width, height);
      color3 = GetPixel(src, srcPitchPixels, x + 1, y + 1, width, height);
      colorS1 = GetPixel(src, srcPitchPixels, x + 2, y + 1, width, height);

      colorA0 = GetPixel(src, srcPitchPixels, x - 1, y + 2, width, height);
      colorA1 = GetPixel(src, srcPitchPixels, x,     y + 2, width, height);
      colorA2 = GetPixel(src, srcPitchPixels, x + 1, y + 2, width, height);
      colorA3 = GetPixel(src, srcPitchPixels, x + 2, y + 2, width, height);

      //--------------------------------------
      if (color2 == color6 && color5 != color3) {
        product2b = product1b = color2;
      } else if (color5 == color3 && color2 != color6) {
        product2b = product1b = color5;
      } else if (color5 == color3 && color2 == color6) {
        int r = 0;

        r += GetResult (color6, color5, color1, colorA1);
        r += GetResult (color6, color5, color4, colorB1);
        r += GetResult (color6, color5, colorA2, colorS1);
        r += GetResult (color6, color5, colorB2, colorS2);

        if (r > 0)
          product2b = product1b = color6;
        else if (r < 0)
          product2b = product1b = color5;
        else {
          product2b = product1b = s.Interp (color5, color6);
        }
      } else {
        if (color6 == color3 && color3 == colorA1
            && color2 != colorA2 && color3 != colorA0)
          product2b =
            s.QInterp (color3, color3, color3, color2);
        else if (color5 == color2 && color2 == colorA2
                 && colorA1 != color3 && color2 != colorA3)
          product2b =
            s.QInterp (color2, color2, color2, color3);
        else
          product2b = s.Interp (color2, color3);

        if (color6 == color3 && color6 == colorB1
            && color5 != colorB2 && color6 != colorB0)
          product1b =
            s.QInterp (color6, color6, color6, color5);
        else if (color5 == color2 && color5 == colorB2
                 && colorB1 != color6 && color5 != colorB3)
          product1b =
            s.QInterp (color6, color5, color5, color5);
        else
          product1b = s.Interp (color5, color6);
      }

      if (color5 == color3 && color2 != color6 && color4 == color5
          && color5 != colorA2)
        product2a = s.Interp (color2, color5);
      else
        if (color5 == color1 && color6 == color5
            && color4 != color2 && color5 != colorA0)
          product2a = s.Interp (color2, color5);
        else
          product2a = color2;

      if (color2 == color6 && color5 != color3 && color1 == color2
          && color2 != colorB2)
        product1a = s.Interp (color2, color5);
      else
        if (color4 == color2 && color3 == color2
            && color1 != color5 && color2 != colorB0)
          product1a = s.Interp (color2, color5);
        else
          product1a = color5;

      dP0[2 * x] = product1a;
      dP0[2 * x + 1] = product1b;
      dP1[2 * x] = product2a;
      dP1[2 * x + 1] = product2b;
    }
  }
}

template <class S>
void SuperEagleScalar(const S& s, const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch,
                      int width, int height) {
  using Pixel = typename S::Pixel;
  const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr);
  const int srcPitchPixels = static_cast<int>(srcPitch / sizeof(Pixel));

  for (int y = 0; y < height; y++) {
    Pixel* dP0 = reinterpret_cast<Pixel*>(dstPtr + y * (dstPitch << 1));
    Pixel* dP1 = reinterpret_cast<Pixel*>(dstPtr + y * (dstPitch << 1) + dstPitch);

    for (int x = 0; x < width; x++) {
      Pixel color4, color5, color6;
      Pixel color1, color2, color3;
      Pixel colorA1, colorA2, colorB1, colorB2, colorS1, colorS2;
      Pixel product1a, product1b, product2a, product2b;

      colorB1 = GetPixel(src, srcPitchPixels, x,     y - 1, width, height);
      colorB2 = GetPixel(src, srcPitchPixels, x + 1, y - 1, width, height);

      color4 = GetPixel(src, srcPitchPixels, x - 1, y, width, height);
      color5 = GetPixel(src, srcPitchPixels, x,     y, width, height);
      color6 = GetPixel(src, srcPitchPixels, x + 1, y, width, height);
      colorS2 = GetPixel(src, srcPitchPixels, x + 2, y, width, height);

      color1 = GetPixel(src, srcPitchPixels, x - 1, y + 1, width, height);
      color2 = GetPixel(src, srcPitchPixels, x,     y + 1, width, height);
      color3 = GetPixel(src, srcPitchPixels, x + 1, y + 1, width, height);
      colorS1 = GetPixel(src, srcPitchPixels, x + 2, y + 1, width, height);

      colorA1 = GetPixel(src, srcPitchPixels, x,     y + 2, width, height);
      colorA2 = GetPixel(src, srcPitchPixels, x + 1, y + 2, width, height);

      // --------------------------------------
      if (color2 == color6 && color5 != color3) {
        product1b = product2a = color2;
        if ((color1 == color2) || (color6 == colorB2)) {
          product1a = s.Interp (color2, color5);
          product1a = s.Interp (color2, product1a);
        } else {
          product1a = s.Interp (color5, color6);
        }

        if ((color6 == colorS2) || (color2 == colorA1)) {
          product2b = s.Interp (color2, color3);
          product2b = s.Interp (color2, product2b);
        } else {
          product2b = s.Interp (color2, color3);
        }
      } else if (color5 == color3 && color2 != color6) {
        product2b = product1a = color5;

        if ((colorB1 == color5) || (color3 == colorS1)) {
          product1b = s.Interp (color5, color6);
          product1b = s.Interp (color5, product1b);
        } else {
          product1b = s.Interp (color5, color6);
        }

        if ((color3 == colorA2) || (color4 == color5)) {
          product2a = s.Interp (color5, color2);
          product2a = s.Interp (color5, product2a);
        } else {
          product2a = s.Interp (color2, color3);
        }

      } else if (color5 == color3 && color2 == color6) {
        int r = 0;

        r += GetResult (color6, color5, color1, colorA1);
        r += GetResult (color6, color5, color4, colorB1);
        r += GetResult (color6, color5, colorA2, colorS1);
        r += GetResult (color6, color5, colorB2, colorS2);

        if (r > 0) {
          product1b = product2a = color2;
          product1a = product2b = s.Interp (color5, color6);
        } else if (r < 0) {
          product2b = product1a = color5;
          product1b = product2a = s.Interp (color5, color6);
        } else {
          product2b = product1a = color5;
          product1b = product2a = color2;
        }
      } else {
        product2b = product1a = s.Interp (color2, color6);
        product2b =
          s.QInterp (color3, color3, color3, product2b);
        product1a =
          s.QInterp (color5, color5, color5, product1a);

        product2a = product1b = s.Interp (color5, color3);
        product2a =
          s.QInterp (color2, color2, color2, product2a);
        product1b =
          s.QInterp (color6, color6, color6, product1b);
      }

      dP0[2 * x] = product1a;
      dP0[2 * x + 1] = product1b;
      dP1[2 * x] = product2a;
      dP1[2 * x + 1] = product2b;
    }
  }
}

template <class S>
void _2xSaIScalar(const S& s, const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch,
                  int width, int height) {
  using Pixel = typename S::Pixel;
  const Pixel* src = reinterpret_cast<const Pixel*>(srcPtr);
  const int srcPitchPixels = static_cast<int>(srcPitch / sizeof(Pixel));

  for (int y = 0; y < height; y++) {
    Pixel* dP0 = reinterpret_cast<Pixel*>(dstPtr + y * (dstPitch << 1));
    Pixel* dP1 = reinterpret_cast<Pixel*>(dstPtr + y * (dstPitch << 1) + dstPitch);

    for (int x = 0; x < width; x++) {
      Pixel colorA, colorB;
      Pixel colorC, colorD,
        colorE, colorF, colorG, colorH,
        colorI, colorJ, colorK, colorL,
        colorM, colorN, colorO;
      Pixel product, product1, product2;

      //---------------------------------------
      // Map of the pixels:                    I|E F|J
      //                                       G|A B|K
      //                                       H|C D|L
      //                                       M|N O|P   (P is not used by the rules)
      colorI = GetPixel(src, srcPitchPixels, x - 1, y - 1, width, height);
      colorE = GetPixel(src, srcPitchPixels, x,     y - 1, width, height);
      colorF = GetPixel(src, srcPitchPixels, x + 1, y - 1, width, height);
      colorJ = GetPixel(src, srcPitchPixels, x + 2, y - 1, width, height);

      colorG = GetPixel(src, srcPitchPixels, x - 1, y, width, height);
      colorA = GetPixel(src, srcPitchPixels, x,     y, width, height);
      colorB = GetPixel(src, srcPitchPixels, x + 1, y, width, height);
      colorK = GetPixel(src, srcPitchPixels, x + 2, y, width, height);

      colorH = GetPixel(src, srcPitchPixels, x - 1, y + 1, width, height);
      colorC = GetPixel(src, srcPitchPixels, x,     y + 1, width, height);
      colorD = GetPixel(src, srcPitchPixels, x + 1, y + 1, width, height);
      colorL = GetPixel(src, srcPitchPixels, x + 2, y + 1, width, height);

      colorM = GetPixel(src, srcPitchPixels, x - 1, y + 2, width, height);
      colorN = GetPixel(src, srcPitchPixels, x,     y + 2, width, height);
      colorO = GetPixel(src, srcPitchPixels, x + 1, y + 2, width, height);

      if ((colorA == colorD) && (colorB != colorC)) {
        if (((colorA == colorE) && (colorB == colorL)) ||
            ((colorA == colorC) && (colorA == colorF)
             && (colorB != colorE) && (colorB == colorJ))) {
          product = colorA;
        } else {
          product = s.Interp (colorA, colorB);
        }

        if (((colorA == colorG) && (colorC == colorO)) ||
            ((colorA == colorB) && (colorA == colorH)
             && (colorG != colorC) && (colorC == colorM))) {
          product1 = colorA;
        } else {
          product1 = s.Interp (colorA, colorC);
        }
        product2 = colorA;
      } else if ((colorB == colorC) && (colorA != colorD)) {
        if (((colorB == colorF) && (colorA == colorH)) ||
            ((colorB == colorE) && (colorB == colorD)
             && (colorA != colorF) && (colorA == colorI))) {
          product = colorB;
        } else {
          product = s.Interp (colorA, colorB);
        }

        if (((colorC == colorH) && (colorA == colorF)) ||
            ((colorC == colorG) && (colorC == colorD)
             && (colorA != colorH) && (colorA == colorI))) {
          product1 = colorC;
        } else {
          product1 = s.Interp (colorA, colorC);
        }
        product2 = colorB;
      } else if ((colorA == colorD) && (colorB == colorC)) {
        if (colorA == colorB) {
          product = colorA;
          product1 = colorA;
          product2 = colorA;
        } else {
          int r = 0;

          product1 = s.Interp (colorA, colorC);
          product = s.Interp (colorA, colorB);

          r += GetResult (colorA, colorB, colorG, colorE);
          r -= GetResult (colorB, colorA, colorK, colorF);
          r -= GetResult (colorB, colorA, colorH, colorN);
          r += GetResult (colorA, colorB, colorL, colorO);

          if (r > 0)
            product2 = colorA;
          else if (r < 0)
            product2 = colorB;
          else {
            product2 =
              s.QInterp (colorA, colorB, colorC,
                         colorD);
          }
        }
      } else {
        product2 = s.QInterp (colorA, colorB, colorC, colorD);

        if ((colorA == colorC) && (colorA == colorF)
            && (colorB != colorE) && (colorB == colorJ)) {
          product = colorA;
        } else if ((colorB == colorE) && (colorB == colorD)
                   && (colorA != colorF) && (colorA == colorI)) {
          product = colorB;
        } else {
          product = s.Interp (colorA, colorB);
        }

        if ((colorA == colorB) && (colorA == colorH)
            && (colorG != colorC) && (colorC == colorM)) {
          product1 = colorA;
        } else if ((colorC == colorG) && (colorC == colorD)
                   && (colorA != colorH) && (colorA == colorI)) {
          product1 = colorC;
        } else {
          product1 = s.Interp (colorA, colorC);
        }
      }

      dP0[2 * x] = colorA;
      dP0[2 * x + 1] = product;
      dP1[2 * x] = product1;
      dP1[2 * x + 1] = product2;
    }
  }
}

// ===========================================================================
// Vector lane arithmetic
// ===========================================================================

struct Lanes32 {
    using Pixel = uint32_t;
    static constexpr int kLanes = 4;
    V rgb;

    Lanes32() : rgb(set1_u32(0x00ffffff)) {}

    static V Eq(V a, V b) { return cmpeq_u32(a, b); }
    static V Ne(V a, V b) { return cmpne_u32(a, b); }
    static void Store2(Pixel* p, V a, V b) { store2_u32(p, a, b); }
    static V One() { return set1_u32(1); }
    static V Add(V a, V b) { return add_u32(a, b); }
    static V Sub(V a, V b) { return sub_u32(a, b); }
    static V GtS(V a, V b) { return cmpgt_s32(a, b); }

    // per channel (a + b) / 2 rounding down, upper byte zero
    V Interp(V a, V b) const { return and_(avgdown_u8(a, b), rgb); }

    // per channel (a + b + c + d) / 4 rounding down, upper byte zero
    V QInterp(V a, V b, V c, V d) const {
        const V z = zero();
        const V lo = srl_u16<2>(add_u16(add_u16(unpacklo_u8(a, z), unpacklo_u8(b, z)),
                                        add_u16(unpacklo_u8(c, z), unpacklo_u8(d, z))));
        const V hi = srl_u16<2>(add_u16(add_u16(unpackhi_u8(a, z), unpackhi_u8(b, z)),
                                        add_u16(unpackhi_u8(c, z), unpackhi_u8(d, z))));
        return and_(packus_s16_u8(lo, hi), rgb);
    }
};

struct Lanes16 {
    using Pixel = uint16_t;
    static constexpr int kLanes = 8;
    V color, low, qcolor, qlow;

    Lanes16() {
        const Masks16 m = Masks16::Current();
        color = set1_u16(m.color);
        low = set1_u16(m.low);
        qcolor = set1_u16(m.qcolor);
        qlow = set1_u16(m.qlow);
    }

    static V Eq(V a, V b) { return cmpeq_u16(a, b); }
    static V Ne(V a, V b) { return cmpne_u16(a, b); }
    static void Store2(Pixel* p, V a, V b) { store2_u16(p, a, b); }
    static V One() { return set1_u16(1); }
    static V Add(V a, V b) { return add_u16(a, b); }
    static V Sub(V a, V b) { return sub_u16(a, b); }
    static V GtS(V a, V b) { return cmpgt_s16(a, b); }

    V Interp(V a, V b) const {
        return add_u16(add_u16(srl_u16<1>(and_(a, color)), srl_u16<1>(and_(b, color))), and_(and_(a, b), low));
    }

    V QInterp(V a, V b, V c, V d) const {
        const V hi = add_u16(add_u16(srl_u16<2>(and_(a, qcolor)), srl_u16<2>(and_(b, qcolor))),
                             add_u16(srl_u16<2>(and_(c, qcolor)), srl_u16<2>(and_(d, qcolor))));
        const V lo = and_(srl_u16<2>(add_u16(add_u16(and_(a, qlow), and_(b, qlow)), add_u16(and_(c, qlow), and_(d, qlow)))),
                          qlow);
        return add_u16(hi, lo);
    }
};

// A lane group whose 2x2 blocks are all flat (the pixel equals its right, lower
// and diagonal neighbour in every lane) comes out as the pixel repeated in all
// three filters; the common case in pixel art, taken before any of the rules.
template <class L>
VBAM_SIMD_INLINE bool FlatBlocks(V p, V right, V below, V diag) {
    return all_ones(and_(and_(L::Eq(p, right), L::Eq(p, below)), L::Eq(p, diag)));
}

// GetResult() per lane, as a signed count in {-1, 0, 1}
template <class L>
VBAM_SIMD_INLINE V GetResultVec(V A, V B, V C, V D) {
    const V aC = L::Eq(A, C);
    const V bC = andnot(aC, L::Eq(B, C));
    const V aD = L::Eq(A, D);
    const V bD = andnot(aD, L::Eq(B, D));
    const V one = L::One();
    const V xLe1 = andnot(and_(aC, aD), one);  // x <= 1
    const V yLe1 = andnot(and_(bC, bD), one);  // y <= 1
    return L::Sub(xLe1, yLe1);
}

// The four padded source rows a kernel reads for output row y: y - 1 .. y + 2,
// each clamped to the image, with one pixel repeated on the left and enough
// on the right for the last lane group plus the x + 2 column.
template <typename Pixel>
class PaddedRows {
public:
    static constexpr int kPadLeft = 4;
    static constexpr int kPadRight = 32;

    PaddedRows(const uint8_t* src, uint32_t srcPitch, int width, int height)
        : src_(src), srcPitch_(srcPitch), width_(width), height_(height), rowLen_(kPadLeft + width + kPadRight),
          buf_(4 * rowLen_) {
        for (int y = -1; y <= 2; y++)
            Fill(y);
        next_ = 3;
    }

    // advance to output row y (rows are requested in increasing order)
    void Advance(int y) {
        while (next_ <= y + 2)
            Fill(next_++);
    }

    const Pixel* Row(int y) const { return buf_.data() + ((y + 8) & 3) * rowLen_ + kPadLeft; }

private:
    void Fill(int y) {
        const int sy = y < 0 ? 0 : y >= height_ ? height_ - 1 : y;
        const Pixel* srcRow = reinterpret_cast<const Pixel*>(src_ + sy * srcPitch_);
        Pixel* row = buf_.data() + ((y + 8) & 3) * rowLen_ + kPadLeft;
        std::memcpy(row, srcRow, width_ * sizeof(Pixel));
        std::fill(row - kPadLeft, row, srcRow[0]);
        std::fill(row + width_, row + width_ + kPadRight, srcRow[width_ - 1]);
    }

    const uint8_t* src_;
    uint32_t srcPitch_;
    int width_, height_, rowLen_;
    std::vector<Pixel> buf_;
    int next_;
};

// Runs a lane-group kernel over every row: full groups straight into the
// destination, the last partial group through a temporary.
template <class L, class Kernel>
void RunRows(const L& l, Kernel kernel, const uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch,
             int width, int height) {
    using Pixel = typename L::Pixel;
    constexpr int kLanes = L::kLanes;
    PaddedRows<Pixel> rows(srcPtr, srcPitch, width, height);

    for (int y = 0; y < height; y++) {
        rows.Advance(y);
        const Pixel* rm1 = rows.Row(y - 1);
        const Pixel* r0 = rows.Row(y);
        const Pixel* rp1 = rows.Row(y + 1);
        const Pixel* rp2 = rows.Row(y + 2);
        Pixel* dst0 = reinterpret_cast<Pixel*>(dstPtr + (y * 2) * dstPitch);
        Pixel* dst1 = reinterpret_cast<Pixel*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        for (; x + kLanes <= width; x += kLanes)
            kernel(l, rm1 + x, r0 + x, rp1 + x, rp2 + x, dst0 + 2 * x, dst1 + 2 * x);
        if (x < width) {
            Pixel tmp0[2 * kLanes], tmp1[2 * kLanes];
            kernel(l, rm1 + x, r0 + x, rp1 + x, rp2 + x, tmp0, tmp1);
            std::memcpy(dst0 + 2 * x, tmp0, 2 * (width - x) * sizeof(Pixel));
            std::memcpy(dst1 + 2 * x, tmp1, 2 * (width - x) * sizeof(Pixel));
        }
    }
}

// ===========================================================================
// Vector kernels: one lane group, every branch of the tree evaluated and
// selected by mask, in the order of the scalar if / else if chains
// ===========================================================================

template <class L>
struct Super2xSaIKernel {
    using Pixel = typename L::Pixel;
    void operator()(const L& l, const Pixel* rm1, const Pixel* r0, const Pixel* rp1, const Pixel* rp2, Pixel* dst0,
                    Pixel* dst1) const {
        const V c5 = load(r0), c6 = load(r0 + 1);
        const V c2 = load(rp1), c3 = load(rp1 + 1);
        if (FlatBlocks<L>(c5, c6, c2, c3)) {
            L::Store2(dst0, c5, c5);
            L::Store2(dst1, c5, c5);
            return;
        }
        const V B0 = load(rm1 - 1), B1 = load(rm1), B2 = load(rm1 + 1), B3 = load(rm1 + 2);
        const V c4 = load(r0 - 1), S2 = load(r0 + 2);
        const V c1 = load(rp1 - 1), S1 = load(rp1 + 2);
        const V A0 = load(rp2 - 1), A1 = load(rp2), A2 = load(rp2 + 1), A3 = load(rp2 + 2);

        const V eq26 = L::Eq(c2, c6), eq53 = L::Eq(c5, c3);
        const V m1 = andnot(eq53, eq26);
        const V m2 = andnot(eq26, eq53);
        const V m3 = and_(eq53, eq26);
        const V m4 = not_(or_(or_(m1, m2), m3));

        // branch 3: the GetResult vote (only where some lane takes it)
        V b3 = c5;
        if (!all_zero(m3)) {
            V r = GetResultVec<L>(c6, c5, c1, A1);
            r = L::Add(r, GetResultVec<L>(c6, c5, c4, B1));
            r = L::Add(r, GetResultVec<L>(c6, c5, A2, S1));
            r = L::Add(r, GetResultVec<L>(c6, c5, B2, S2));
            const V z = zero();
            const V rPos = L::GtS(r, z), rNeg = L::GtS(z, r);
            b3 = vselect(rPos, c6, vselect(rNeg, c5, l.Interp(c5, c6)));
        }

        // branch 4
        V b4_p2b = c5, b4_p1b = c5;
        if (!all_zero(m4)) {
            const V eq63 = L::Eq(c6, c3), eq52 = L::Eq(c5, c2);
            const V p2b_q1 = and_(and_(eq63, L::Eq(c3, A1)), and_(L::Ne(c2, A2), L::Ne(c3, A0)));
            const V p2b_q2 = and_(and_(eq52, L::Eq(c2, A2)), and_(L::Ne(A1, c3), L::Ne(c2, A3)));
            b4_p2b = vselect(p2b_q1, l.QInterp(c3, c3, c3, c2), vselect(p2b_q2, l.QInterp(c2, c2, c2, c3), l.Interp(c2, c3)));
            const V p1b_q1 = and_(and_(eq63, L::Eq(c6, B1)), and_(L::Ne(c5, B2), L::Ne(c6, B0)));
            const V p1b_q2 = and_(and_(eq52, L::Eq(c5, B2)), and_(L::Ne(B1, c6), L::Ne(c5, B3)));
            b4_p1b = vselect(p1b_q1, l.QInterp(c6, c6, c6, c5), vselect(p1b_q2, l.QInterp(c6, c5, c5, c5), l.Interp(c5, c6)));
        }

        const V product2b = vselect(m1, c2, vselect(m2, c5, vselect(m3, b3, b4_p2b)));
        const V product1b = vselect(m1, c2, vselect(m2, c5, vselect(m3, b3, b4_p1b)));

        const V i25 = l.Interp(c2, c5);
        const V p2a_c1 = and_(and_(eq53, L::Ne(c2, c6)), and_(L::Eq(c4, c5), L::Ne(c5, A2)));
        const V p2a_c2 = and_(and_(L::Eq(c5, c1), L::Eq(c6, c5)), and_(L::Ne(c4, c2), L::Ne(c5, A0)));
        const V product2a = vselect(or_(p2a_c1, p2a_c2), i25, c2);

        const V p1a_c1 = and_(and_(eq26, L::Ne(c5, c3)), and_(L::Eq(c1, c2), L::Ne(c2, B2)));
        const V p1a_c2 = and_(and_(L::Eq(c4, c2), L::Eq(c3, c2)), and_(L::Ne(c1, c5), L::Ne(c2, B0)));
        const V product1a = vselect(or_(p1a_c1, p1a_c2), i25, c5);

        L::Store2(dst0, product1a, product1b);
        L::Store2(dst1, product2a, product2b);
    }
};

template <class L>
struct SuperEagleKernel {
    using Pixel = typename L::Pixel;
    void operator()(const L& l, const Pixel* rm1, const Pixel* r0, const Pixel* rp1, const Pixel* rp2, Pixel* dst0,
                    Pixel* dst1) const {
        const V c5 = load(r0), c6 = load(r0 + 1);
        const V c2 = load(rp1), c3 = load(rp1 + 1);
        if (FlatBlocks<L>(c5, c6, c2, c3)) {
            L::Store2(dst0, c5, c5);
            L::Store2(dst1, c5, c5);
            return;
        }
        const V B1 = load(rm1), B2 = load(rm1 + 1);
        const V c4 = load(r0 - 1), S2 = load(r0 + 2);
        const V c1 = load(rp1 - 1), S1 = load(rp1 + 2);
        const V A1 = load(rp2), A2 = load(rp2 + 1);

        const V eq26 = L::Eq(c2, c6), eq53 = L::Eq(c5, c3);
        const V m1 = andnot(eq53, eq26);
        const V m2 = andnot(eq26, eq53);
        const V m3 = and_(eq53, eq26);
        const V m4 = not_(or_(or_(m1, m2), m3));

        const V i56 = l.Interp(c5, c6);
        const V i23 = l.Interp(c2, c3);
        const V i25 = l.Interp(c2, c5);

        // branch 1
        V b1_p1a = c5, b1_p2b = c5;
        if (!all_zero(m1)) {
            b1_p1a = vselect(or_(L::Eq(c1, c2), L::Eq(c6, B2)), l.Interp(c2, i25), i56);
            b1_p2b = vselect(or_(L::Eq(c6, S2), L::Eq(c2, A1)), l.Interp(c2, i23), i23);
        }

        // branch 2
        V b2_p1b = c5, b2_p2a = c5;
        if (!all_zero(m2)) {
            b2_p1b = vselect(or_(L::Eq(B1, c5), L::Eq(c3, S1)), l.Interp(c5, i56), i56);
            b2_p2a = vselect(or_(L::Eq(c3, A2), L::Eq(c4, c5)), l.Interp(c5, i25), i23);
        }

        // branch 3
        V b3_p1a = c5, b3_p1b = c5;
        if (!all_zero(m3)) {
            V r = GetResultVec<L>(c6, c5, c1, A1);
            r = L::Add(r, GetResultVec<L>(c6, c5, c4, B1));
            r = L::Add(r, GetResultVec<L>(c6, c5, A2, S1));
            r = L::Add(r, GetResultVec<L>(c6, c5, B2, S2));
            const V z = zero();
            const V rPos = L::GtS(r, z), rNeg = L::GtS(z, r);
            b3_p1a = vselect(rPos, i56, c5);              // r > 0: i56, r < 0: c5, else c5
            b3_p1b = vselect(rPos, c2, vselect(rNeg, i56, c2));
        }
        const V b3_p2a = b3_p1b;
        const V b3_p2b = b3_p1a;

        // branch 4
        V b4_p2b = c5, b4_p1a = c5, b4_p2a = c5, b4_p1b = c5;
        if (!all_zero(m4)) {
            const V i26 = l.Interp(c2, c6);
            const V i53 = l.Interp(c5, c3);
            b4_p2b = l.QInterp(c3, c3, c3, i26);
            b4_p1a = l.QInterp(c5, c5, c5, i26);
            b4_p2a = l.QInterp(c2, c2, c2, i53);
            b4_p1b = l.QInterp(c6, c6, c6, i53);
        }

        const V product1a = vselect(m1, b1_p1a, vselect(m2, c5, vselect(m3, b3_p1a, b4_p1a)));
        const V product1b = vselect(m1, c2, vselect(m2, b2_p1b, vselect(m3, b3_p1b, b4_p1b)));
        const V product2a = vselect(m1, c2, vselect(m2, b2_p2a, vselect(m3, b3_p2a, b4_p2a)));
        const V product2b = vselect(m1, b1_p2b, vselect(m2, c5, vselect(m3, b3_p2b, b4_p2b)));

        L::Store2(dst0, product1a, product1b);
        L::Store2(dst1, product2a, product2b);
    }
};

template <class L>
struct _2xSaIKernel {
    using Pixel = typename L::Pixel;
    void operator()(const L& l, const Pixel* rm1, const Pixel* r0, const Pixel* rp1, const Pixel* rp2, Pixel* dst0,
                    Pixel* dst1) const {
        // I|E F|J
        // G|A B|K
        // H|C D|L
        // M|N O|P
        const V A = load(r0), B = load(r0 + 1);
        const V C = load(rp1), D = load(rp1 + 1);
        if (FlatBlocks<L>(A, B, C, D)) {
            L::Store2(dst0, A, A);
            L::Store2(dst1, A, A);
            return;
        }
        const V I = load(rm1 - 1), E = load(rm1), F = load(rm1 + 1), J = load(rm1 + 2);
        const V G = load(r0 - 1), K = load(r0 + 2);
        const V H = load(rp1 - 1), Lp = load(rp1 + 2);
        const V M = load(rp2 - 1), N = load(rp2), O = load(rp2 + 1);

        const V eqAD = L::Eq(A, D), eqBC = L::Eq(B, C);
        const V m1 = andnot(eqBC, eqAD);
        const V m2 = andnot(eqAD, eqBC);
        const V m3 = and_(eqAD, eqBC);
        const V eqAB = L::Eq(A, B);
        const V m4 = not_(or_(or_(m1, m2), m3));

        const V iAB = l.Interp(A, B);
        const V iAC = l.Interp(A, C);
        // the four-way average is needed by branch 4 and by the undecided vote of branch 3
        const V needQ = or_(m4, andnot(eqAB, m3));
        const V qABCD = all_zero(needQ) ? A : l.QInterp(A, B, C, D);

        const V eqAE = L::Eq(A, E), eqAC = L::Eq(A, C), eqAF = L::Eq(A, F), eqAG = L::Eq(A, G),
                eqAH = L::Eq(A, H), eqAI = L::Eq(A, I), eqBE = L::Eq(B, E), eqBF = L::Eq(B, F), eqBJ = L::Eq(B, J),
                eqB_L = L::Eq(B, Lp), eqCG = L::Eq(C, G), eqCH = L::Eq(C, H), eqCM = L::Eq(C, M),
                eqCO = L::Eq(C, O), eqCD = L::Eq(C, D), eqBD = L::Eq(B, D);

        // the two "keep A / keep B" conditions shared by branches 1, 2 and 4
        const V keepA_h = and_(and_(eqAC, eqAF), and_(not_(eqBE), eqBJ));   // A == C && A == F && B != E && B == J
        const V keepB_h = and_(and_(eqBE, eqBD), and_(not_(eqAF), eqAI));   // B == E && B == D && A != F && A == I
        const V keepA_v = and_(and_(eqAB, eqAH), and_(not_(eqCG), eqCM));   // A == B && A == H && G != C && C == M
        const V keepC_v = and_(and_(eqCG, eqCD), and_(not_(eqAH), eqAI));   // C == G && C == D && A != H && A == I

        // branch 1
        const V b1_p = vselect(or_(and_(eqAE, eqB_L), keepA_h), A, iAB);
        const V b1_p1 = vselect(or_(and_(eqAG, eqCO), keepA_v), A, iAC);

        // branch 2
        const V b2_p = vselect(or_(and_(eqBF, eqAH), keepB_h), B, iAB);
        const V b2_p1 = vselect(or_(and_(eqCH, eqAF), keepC_v), C, iAC);

        // branch 3
        const V b3_p = vselect(eqAB, A, iAB);
        const V b3_p1 = vselect(eqAB, A, iAC);
        V b3_p2 = A;
        if (!all_zero(andnot(eqAB, m3))) {
            V r = GetResultVec<L>(A, B, G, E);
            r = L::Sub(r, GetResultVec<L>(B, A, K, F));
            r = L::Sub(r, GetResultVec<L>(B, A, H, N));
            r = L::Add(r, GetResultVec<L>(A, B, Lp, O));
            const V z = zero();
            const V rPos = L::GtS(r, z), rNeg = L::GtS(z, r);
            b3_p2 = vselect(eqAB, A, vselect(rPos, A, vselect(rNeg, B, qABCD)));
        }

        // branch 4
        const V b4_p = vselect(keepA_h, A, vselect(keepB_h, B, iAB));
        const V b4_p1 = vselect(keepA_v, A, vselect(keepC_v, C, iAC));

        const V product = vselect(m1, b1_p, vselect(m2, b2_p, vselect(m3, b3_p, b4_p)));
        const V product1 = vselect(m1, b1_p1, vselect(m2, b2_p1, vselect(m3, b3_p1, b4_p1)));
        const V product2 = vselect(m1, A, vselect(m2, B, vselect(m3, b3_p2, qABCD)));

        L::Store2(dst0, A, product);
        L::Store2(dst1, product1, product2);
    }
};

// the delta buffer records the frame that was filtered
template <typename Pixel>
void RecordDelta(uint8_t* deltaPtr, const uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    if (!deltaPtr)
        return;
    for (int y = 0; y < height; y++)
        std::memcpy(deltaPtr + y * srcPitch, srcPtr + y * srcPitch, width * sizeof(Pixel));
}

}  // namespace

void Super2xSaI (uint8_t *srcPtr, uint32_t srcPitch,
                 uint8_t *deltaPtr, uint8_t *dstPtr, uint32_t dstPitch,
                 int width, int height)
{
  if (Enabled()) {
    RunRows(Lanes16(), Super2xSaIKernel<Lanes16>(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    return;
  }
#ifdef MMX
  if (cpu_mmx && deltaPtr) {
    for (int row = 0; row < height; row++) {
      _2xSaISuper2xSaILine (srcPtr + row * srcPitch, deltaPtr + row * srcPitch,
                            srcPitch, width, dstPtr + row * dstPitch * 2, dstPitch);
    }
    return;
  }
#endif
  (void)deltaPtr;
  Super2xSaIScalar(Scalar16(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Super2xSaI32 (uint8_t *srcPtr, uint32_t srcPitch,
                   uint8_t * /* deltaPtr */, uint8_t *dstPtr, uint32_t dstPitch,
                   int width, int height)
{
  if (Enabled())
    RunRows(Lanes32(), Super2xSaIKernel<Lanes32>(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
  else
    Super2xSaIScalar(Scalar32(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void SuperEagle (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *deltaPtr,
                 uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  if (Enabled()) {
    RunRows(Lanes16(), SuperEagleKernel<Lanes16>(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    RecordDelta<uint16_t>(deltaPtr, srcPtr, srcPitch, width, height);
    return;
  }
#ifdef MMX
  if (cpu_mmx && deltaPtr) {
    for (int row = 0; row < height; row++) {
      _2xSaISuperEagleLine (srcPtr + row * srcPitch, deltaPtr + row * srcPitch,
                            srcPitch, width, dstPtr + row * dstPitch * 2, dstPitch);
    }
    return;
  }
#endif
  SuperEagleScalar(Scalar16(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
  RecordDelta<uint16_t>(deltaPtr, srcPtr, srcPitch, width, height);
}

void SuperEagle32 (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *deltaPtr,
                   uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  if (Enabled())
    RunRows(Lanes32(), SuperEagleKernel<Lanes32>(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
  else
    SuperEagleScalar(Scalar32(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
  RecordDelta<uint32_t>(deltaPtr, srcPtr, srcPitch, width, height);
}

void _2xSaI (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *deltaPtr,
             uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  if (Enabled()) {
    RunRows(Lanes16(), _2xSaIKernel<Lanes16>(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    return;
  }
#ifdef MMX
  if (cpu_mmx && deltaPtr) {
    for (int row = 0; row < height; row++) {
      _2xSaILine (srcPtr + row * srcPitch, deltaPtr + row * srcPitch,
                  srcPitch, width, dstPtr + row * dstPitch * 2, dstPitch);
    }
    return;
  }
#endif
  (void)deltaPtr;
  _2xSaIScalar(Scalar16(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void _2xSaI32 (uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
               uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  if (Enabled())
    RunRows(Lanes32(), _2xSaIKernel<Lanes32>(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
  else
    _2xSaIScalar(Scalar32(), srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

// Unused scaled 2xSaI, kept as in the original source
static uint32_t Bilinear (uint32_t A, uint32_t B, uint32_t x)
{
  unsigned long areaA, areaB;
  unsigned long result;

  if (A == B)
    return A;

  areaB = (x >> 11) & 0x1f;     // reduce 16 bit fraction to 5 bits
  areaA = 0x20 - areaB;

  A = (A & redblueMask) | ((A & greenMask) << 16);
  B = (B & redblueMask) | ((B & greenMask) << 16);

  result = ((areaA * A) + (areaB * B)) >> 5;

  return (result & redblueMask) | ((result >> 16) & greenMask);
}

static uint32_t Bilinear4 (uint32_t A, uint32_t B, uint32_t C, uint32_t D, uint32_t x,
                         uint32_t y)
{
  unsigned long areaA, areaB, areaC, areaD;
  unsigned long result, xy;

  x = (x >> 11) & 0x1f;
  y = (y >> 11) & 0x1f;
  xy = (x * y) >> 5;

  A = (A & redblueMask) | ((A & greenMask) << 16);
  B = (B & redblueMask) | ((B & greenMask) << 16);
  C = (C & redblueMask) | ((C & greenMask) << 16);
  D = (D & redblueMask) | ((D & greenMask) << 16);

  areaA = 0x20 + xy - x - y;
  areaB = x - xy;
  areaC = y - xy;
  areaD = xy;

  result = ((areaA * A) + (areaB * B) + (areaC * C) + (areaD * D)) >> 5;

  return (result & redblueMask) | ((result >> 16) & greenMask);
}

void Scale_2xSaI (uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                  uint8_t *dstPtr, uint32_t dstPitch,
                  uint32_t dstWidth, uint32_t dstHeight, int width, int height)
{
  uint8_t  *dP;

  uint32_t w;
  uint32_t h;
  uint32_t dw;
  uint32_t dh;
  uint32_t hfinish;
  uint32_t wfinish;

  uint32_t Nextline = srcPitch >> 1;
  const uint16_t* src = reinterpret_cast<const uint16_t*>(srcPtr);
  int srcPitch16 = static_cast<int>(Nextline);

  wfinish = (width - 1) << 16;  // convert to fixed point
  dw = wfinish / (dstWidth - 1);
  hfinish = (height - 1) << 16; // convert to fixed point
  dh = hfinish / (dstHeight - 1);

  for (h = 0; h < hfinish; h += dh) {
    uint32_t y1, y2;

    y1 = h & 0xffff;    // fraction part of fixed point
    int srcY = static_cast<int>(h >> 16);
    dP = dstPtr;
    y2 = 0x10000 - y1;

    w = 0;

    for (; w < wfinish;) {
      uint32_t A, B, C, D;
      uint32_t E, F, G, H;
      uint32_t I, J, K, L;
      uint32_t x1, x2, a1, f1, f2;
      uint32_t product1 = 0;

      int srcX = static_cast<int>(w >> 16);

      // Safe boundary-clamped pixel access
      A = GetPixel(src, srcPitch16, srcX,     srcY,     width, height);
      B = GetPixel(src, srcPitch16, srcX + 1, srcY,     width, height);
      C = GetPixel(src, srcPitch16, srcX,     srcY + 1, width, height);
      D = GetPixel(src, srcPitch16, srcX + 1, srcY + 1, width, height);
      E = GetPixel(src, srcPitch16, srcX,     srcY - 1, width, height);
      F = GetPixel(src, srcPitch16, srcX + 1, srcY - 1, width, height);
      G = GetPixel(src, srcPitch16, srcX - 1, srcY,     width, height);
      H = GetPixel(src, srcPitch16, srcX - 1, srcY + 1, width, height);
      I = GetPixel(src, srcPitch16, srcX + 2, srcY,     width, height);
      J = GetPixel(src, srcPitch16, srcX + 2, srcY + 1, width, height);
      K = GetPixel(src, srcPitch16, srcX,     srcY + 2, width, height);
      L = GetPixel(src, srcPitch16, srcX + 1, srcY + 2, width, height);

      x1 = w & 0xffff;  // fraction part of fixed point
      x2 = 0x10000 - x1;

      /*0*/
      if (A == B && C == D && A == C)
        product1 = A;
      else /*1*/ if (A == D && B != C) {
        f1 = (x1 >> 1) + (0x10000 >> 2);
        f2 = (y1 >> 1) + (0x10000 >> 2);
        if (y1 <= f1 && A == J && A != E)       // close to B
          {
            a1 = f1 - y1;
            product1 = Bilinear (A, B, a1);
          } else if (y1 >= f1 && A == G && A != L)      // close to C
            {
              a1 = y1 - f1;
              product1 = Bilinear (A, C, a1);
            }
        else if (x1 >= f2 && A == E && A != J)  // close to B
          {
            a1 = x1 - f2;
            product1 = Bilinear (A, B, a1);
          }
        else if (x1 <= f2 && A == L && A != G)  // close to C
          {
            a1 = f2 - x1;
            product1 = Bilinear (A, C, a1);
          }
        else if (y1 >= x1)      // close to C
          {
            a1 = y1 - x1;
            product1 = Bilinear (A, C, a1);
          }
        else if (y1 <= x1)      // close to B
          {
            a1 = x1 - y1;
            product1 = Bilinear (A, B, a1);
          }
      }
      else
        /*2*/
        if (B == C && A != D)
          {
            f1 = (x1 >> 1) + (0x10000 >> 2);
            f2 = (y1 >> 1) + (0x10000 >> 2);
            if (y2 >= f1 && B == H && B != F)   // close to A
              {
                a1 = y2 - f1;
                product1 = Bilinear (B, A, a1);
              }
            else if (y2 <= f1 && B == I && B != K)      // close to D
              {
                a1 = f1 - y2;
                product1 = Bilinear (B, D, a1);
              }
            else if (x2 >= f2 && B == F && B != H)      // close to A
              {
                a1 = x2 - f2;
                product1 = Bilinear (B, A, a1);
              }
            else if (x2 <= f2 && B == K && B != I)      // close to D
              {
                a1 = f2 - x2;
                product1 = Bilinear (B, D, a1);
              }
            else if (y2 >= x1)  // close to A
              {
                a1 = y2 - x1;
                product1 = Bilinear (B, A, a1);
              }
            else if (y2 <= x1)  // close to D
              {
                a1 = x1 - y2;
                product1 = Bilinear (B, D, a1);
              }
          }
      /*3*/
        else
          {
            product1 = Bilinear4 (A, B, C, D, x1, y1);
          }

      //end First Pixel
      *(uint32_t *) dP = product1;
      dP += 2;
      w += dw;
    }
    dstPtr += dstPitch;
  }
}
