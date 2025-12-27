#include <cstdint>

#include "simd_common.h"

extern int RGB_LOW_BITS_MASK;

// ============================================================================
// SSE2 Implementations
// ============================================================================

#ifdef USE_SSE2

// Scanlines32: 2x scale with black scanlines (alternate rows = 0)
static void Scanlines32_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                              uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; y++) {
        uint32_t *src = reinterpret_cast<uint32_t*>(srcPtr + y * srcPitch);
        uint32_t *dst0 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2) * dstPitch);
        uint32_t *dst1 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        // Process 4 pixels at a time
        for (; x + 4 <= width; x += 4) {
            __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));

            // Duplicate each pixel: [A B C D] -> [A A B B] [C C D D]
            __m128i lo = _mm_unpacklo_epi32(pixels, pixels);
            __m128i hi = _mm_unpackhi_epi32(pixels, pixels);

            // Store duplicated pixels to row 0
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2]), lo);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2 + 4]), hi);

            // Store zeros to row 1 (scanline)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2]), zero);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2 + 4]), zero);
        }

        // Scalar tail
        for (; x < width; x++) {
            uint32_t color = src[x];
            dst0[x * 2] = color;
            dst0[x * 2 + 1] = color;
            dst1[x * 2] = 0;
            dst1[x * 2 + 1] = 0;
        }
    }
}

// Scanlines (16-bit): 2x scale with black scanlines
static void Scanlines_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                            uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; y++) {
        uint16_t *src = reinterpret_cast<uint16_t*>(srcPtr + y * srcPitch);
        uint16_t *dst0 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2) * dstPitch);
        uint16_t *dst1 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        // Process 8 pixels at a time
        for (; x + 8 <= width; x += 8) {
            __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));

            // Duplicate each 16-bit pixel
            __m128i lo = _mm_unpacklo_epi16(pixels, pixels);
            __m128i hi = _mm_unpackhi_epi16(pixels, pixels);

            // Store duplicated pixels to row 0
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2]), lo);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2 + 8]), hi);

            // Store zeros to row 1 (scanline)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2]), zero);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2 + 8]), zero);
        }

        // Scalar tail
        for (; x < width; x++) {
            uint16_t color = src[x];
            dst0[x * 2] = color;
            dst0[x * 2 + 1] = color;
            dst1[x * 2] = 0;
            dst1[x * 2 + 1] = 0;
        }
    }
}

// ScanlinesTV32: 2x scale with interpolation and darkened scanlines
// Row 0: [original, interpolated with next]
// Row 1: [darkened original, darkened interpolated]
static void ScanlinesTV32_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                                uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    const __m128i mask = _mm_set1_epi32(0x00FEFEFE);

    for (int y = 0; y < height; y++) {
        uint32_t *src = reinterpret_cast<uint32_t*>(srcPtr + y * srcPitch);
        uint32_t *dst0 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2) * dstPitch);
        uint32_t *dst1 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        // Process 4 pixels at a time, output 8 pixels
        for (; x + 4 <= width; x += 4) {
            // Load current 4 pixels and next 4 (shifted by 1)
            __m128i curr = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));

            // Create shifted version for interpolation: [B C D E] where curr = [A B C D]
            __m128i next;
            if (x + 5 <= width) {
                // Load next group and blend
                __m128i nextGroup = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x + 1]));
                next = nextGroup;
            } else {
                // Near edge - shift within current register and replicate last
                next = _mm_srli_si128(curr, 4);  // Shift right by 4 bytes
                // Insert last pixel at end
                uint32_t lastPixel = src[width - 1];
                __m128i lastVec = _mm_set1_epi32(lastPixel);
                // Combine: next has [B C D 0], we want [B C D last]
                next = _mm_or_si128(next, _mm_slli_si128(lastVec, 12));
            }

            // Interpolate: (curr + next) / 2
            __m128i curr_masked = _mm_and_si128(curr, mask);
            __m128i next_masked = _mm_and_si128(next, mask);
            __m128i interp = _mm_add_epi32(_mm_srli_epi32(curr_masked, 1),
                                           _mm_srli_epi32(next_masked, 1));

            // Interleave original and interpolated: [A interp(A,B) B interp(B,C) ...]
            __m128i out0 = _mm_unpacklo_epi32(curr, interp);  // [A interp0 B interp1]
            __m128i out1 = _mm_unpackhi_epi32(curr, interp);  // [C interp2 D interp3]

            // Store row 0 (original + interpolated)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2]), out0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2 + 4]), out1);

            // Darken for row 1: color' = (color >> 1) + (color >> 2) = color * 0.75
            // For out0
            __m128i out0_masked = _mm_and_si128(out0, mask);
            __m128i out0_half = _mm_srli_epi32(out0_masked, 1);
            __m128i out0_half_masked = _mm_and_si128(out0_half, mask);
            __m128i out0_quarter = _mm_srli_epi32(out0_half_masked, 1);
            __m128i dark0 = _mm_add_epi32(out0_half, out0_quarter);

            // For out1
            __m128i out1_masked = _mm_and_si128(out1, mask);
            __m128i out1_half = _mm_srli_epi32(out1_masked, 1);
            __m128i out1_half_masked = _mm_and_si128(out1_half, mask);
            __m128i out1_quarter = _mm_srli_epi32(out1_half_masked, 1);
            __m128i dark1 = _mm_add_epi32(out1_half, out1_quarter);

            // Store row 1 (darkened)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2]), dark0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2 + 4]), dark1);
        }

        // Scalar tail
        uint32_t colorMask = ~RGB_LOW_BITS_MASK;
        for (; x < width; x++) {
            uint32_t colorA = src[x];
            uint32_t colorB = (x + 1 < width) ? src[x + 1] : colorA;
            uint32_t temp = ((colorA & colorMask) >> 1) + ((colorB & colorMask) >> 1);

            dst0[x * 2] = colorA;
            dst0[x * 2 + 1] = temp;

            uint32_t darkA = ((colorA & colorMask) >> 1);
            darkA += ((darkA & colorMask) >> 1);
            uint32_t darkTemp = ((temp & colorMask) >> 1);
            darkTemp += ((darkTemp & colorMask) >> 1);

            dst1[x * 2] = darkA;
            dst1[x * 2 + 1] = darkTemp;
        }
    }
}

// ScanlinesTV (16-bit): 2x scale with interpolation and darkened scanlines
static void ScanlinesTV_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                              uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    const __m128i mask = _mm_set1_epi16(static_cast<int16_t>(~RGB_LOW_BITS_MASK));

    for (int y = 0; y < height; y++) {
        uint16_t *src = reinterpret_cast<uint16_t*>(srcPtr + y * srcPitch);
        uint16_t *dst0 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2) * dstPitch);
        uint16_t *dst1 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        // Process 8 pixels at a time
        for (; x + 8 <= width; x += 8) {
            __m128i curr = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));

            // Create shifted version for interpolation
            __m128i next;
            if (x + 9 <= width) {
                next = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x + 1]));
            } else {
                next = _mm_srli_si128(curr, 2);  // Shift right by 2 bytes (1 pixel)
                uint16_t lastPixel = src[width - 1];
                __m128i lastVec = _mm_set1_epi16(lastPixel);
                next = _mm_or_si128(next, _mm_slli_si128(lastVec, 14));
            }

            // Interpolate: (curr + next) / 2
            __m128i curr_masked = _mm_and_si128(curr, mask);
            __m128i next_masked = _mm_and_si128(next, mask);
            __m128i interp = _mm_add_epi16(_mm_srli_epi16(curr_masked, 1),
                                           _mm_srli_epi16(next_masked, 1));

            // Interleave original and interpolated
            __m128i out0 = _mm_unpacklo_epi16(curr, interp);
            __m128i out1 = _mm_unpackhi_epi16(curr, interp);

            // Store row 0
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2]), out0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2 + 8]), out1);

            // Darken for row 1
            __m128i out0_masked = _mm_and_si128(out0, mask);
            __m128i out0_half = _mm_srli_epi16(out0_masked, 1);
            __m128i out0_half_masked = _mm_and_si128(out0_half, mask);
            __m128i out0_quarter = _mm_srli_epi16(out0_half_masked, 1);
            __m128i dark0 = _mm_add_epi16(out0_half, out0_quarter);

            __m128i out1_masked = _mm_and_si128(out1, mask);
            __m128i out1_half = _mm_srli_epi16(out1_masked, 1);
            __m128i out1_half_masked = _mm_and_si128(out1_half, mask);
            __m128i out1_quarter = _mm_srli_epi16(out1_half_masked, 1);
            __m128i dark1 = _mm_add_epi16(out1_half, out1_quarter);

            // Store row 1
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2]), dark0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2 + 8]), dark1);
        }

        // Scalar tail - use original algorithm
        for (; x < width; x++) {
            uint16_t colorA = src[x];
            uint16_t colorB = (x + 1 < width) ? src[x + 1] : colorA;
            uint16_t interp = static_cast<uint16_t>(
                ((colorA & ~RGB_LOW_BITS_MASK) >> 1) + ((colorB & ~RGB_LOW_BITS_MASK) >> 1));

            dst0[x * 2] = colorA;
            dst0[x * 2 + 1] = interp;

            uint16_t darkA = static_cast<uint16_t>((colorA & ~RGB_LOW_BITS_MASK) >> 1);
            darkA = static_cast<uint16_t>(darkA + ((darkA & ~RGB_LOW_BITS_MASK) >> 1));
            uint16_t darkInterp = static_cast<uint16_t>((interp & ~RGB_LOW_BITS_MASK) >> 1);
            darkInterp = static_cast<uint16_t>(darkInterp + ((darkInterp & ~RGB_LOW_BITS_MASK) >> 1));

            dst1[x * 2] = darkA;
            dst1[x * 2 + 1] = darkInterp;
        }
    }
}

#endif  // USE_SSE2

void Scanlines (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
  Scanlines_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
  uint8_t *nextLine, *finish;

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;
    uint32_t nextPixel;

    finish = (uint8_t *) bP + ((width+2) << 1);
    nextPixel = *bP++;

    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;
      uint32_t colorA, colorB;

#ifdef WORDS_BIGENDIAN
      colorA = currentPixel >> 16;
      colorB = currentPixel & 0xffff;
#else
      colorA = currentPixel & 0xffff;
      colorB = currentPixel >> 16;
#endif

      *(dP) = colorA | colorA<<16;
      *(nL) = 0;

#ifdef WORDS_BIGENDIAN
      colorA = nextPixel >> 16;
#else
      colorA = nextPixel & 0xffff;
#endif

      *(dP + 1) = colorB | (colorB << 16);
      *(nL + 1) = 0;

      dP += 2;
      nL += 2;
    } while ((uint8_t *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
#endif
}

void Scanlines32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                 uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
  Scanlines32_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
  uint8_t *nextLine, *finish;

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;
    uint32_t nextPixel;

    finish = (uint8_t *) bP + ((width+1) << 2);
    nextPixel = *bP++;

    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;

      uint32_t colorA, colorB;

      colorA = currentPixel;
      colorB = nextPixel;

      *(dP) = colorA;
      *(dP+1) = colorA;
      *(nL) = 0;
      *(nL+1) = 0;

      *(dP + 2) = colorB;
      *(dP + 3) = colorB;
      *(nL+2) = 0;
      *(nL+3) = 0;

      nextPixel = *bP++;

      dP += 4;
      nL += 4;
    } while ((uint8_t *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
#endif
}

void ScanlinesTV(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                 uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
  ScanlinesTV_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
  uint8_t *nextLine, *finish;
  uint32_t colorMask = ~(RGB_LOW_BITS_MASK | (RGB_LOW_BITS_MASK << 16));

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;
    uint32_t nextPixel;

    finish = (uint8_t *) bP + ((width+2) << 1);
    nextPixel = *bP++;

    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;

      uint32_t colorA, colorB;

#ifdef WORDS_BIGENDIAN
      colorA = currentPixel >> 16;
      colorB = currentPixel & 0xFFFF;
#else
      colorA = currentPixel & 0xFFFF;
      colorB = currentPixel >> 16;
#endif

      *(dP) = colorA = colorA | ((((colorA & colorMask) >> 1) +
                                  ((colorB & colorMask) >> 1))) << 16;
      colorA = ((colorA & colorMask) >> 1);
      colorA += ((colorA & colorMask) >> 1);
      *(nL) = colorA;

      colorA = nextPixel & 0xFFFF;

      *(dP + 1) = colorB = colorB | ((((colorA & colorMask) >> 1) +
                                      ((colorB & colorMask) >> 1))) << 16;
      colorB = ((colorB & colorMask) >> 1);
      colorB += ((colorB & colorMask) >> 1);

      *(nL + 1) = colorB;

      dP += 2;
      nL += 2;
    } while ((uint8_t *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
#endif
}

void ScanlinesTV32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                   uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
  ScanlinesTV32_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
  uint8_t *nextLine, *finish;
  uint32_t colorMask = ~RGB_LOW_BITS_MASK;

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;
    uint32_t nextPixel;

    finish = (uint8_t *) bP + ((width+1) << 2);
    nextPixel = *bP++;

    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;

      uint32_t colorA, colorB, temp;

      colorA = currentPixel;
      colorB = nextPixel;

      *(dP) = colorA;
      *(dP+1) = temp = ((colorA & colorMask) >> 1) +
        ((colorB & colorMask) >> 1);
      temp = ((temp & colorMask) >> 1);
      temp += ((temp & colorMask) >> 1);
      colorA = ((colorA & colorMask) >> 1);
      colorA += ((colorA & colorMask) >> 1);

      *(nL) = colorA;
      *(nL+1) = temp;

      dP += 2;
      nL += 2;
    } while ((uint8_t *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
#endif
}
