#include <cstdint>

#include "simd_common.h"

// ============================================================================
// SSE2 Implementations
// ============================================================================

#ifdef USE_SSE2

static void Simple2x32_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                             uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    for (int y = 0; y < height; y++) {
        uint32_t *src = reinterpret_cast<uint32_t*>(srcPtr + y * srcPitch);
        uint32_t *dst0 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2) * dstPitch);
        uint32_t *dst1 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        // Process 4 source pixels at a time, producing 8 output pixels
        for (; x + 4 <= width; x += 4) {
            __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));

            // Duplicate each pixel: [A B C D] -> [A A B B] and [C C D D]
            __m128i lo = _mm_unpacklo_epi32(pixels, pixels);  // [A A B B]
            __m128i hi = _mm_unpackhi_epi32(pixels, pixels);  // [C C D D]

            // Store to both rows
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2]), lo);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2 + 4]), hi);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2]), lo);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2 + 4]), hi);
        }

        // Scalar tail for remaining pixels
        for (; x < width; x++) {
            uint32_t color = src[x];
            dst0[x * 2] = color;
            dst0[x * 2 + 1] = color;
            dst1[x * 2] = color;
            dst1[x * 2 + 1] = color;
        }
    }
}

static void Simple2x16_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                             uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    for (int y = 0; y < height; y++) {
        uint16_t *src = reinterpret_cast<uint16_t*>(srcPtr + y * srcPitch);
        uint16_t *dst0 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2) * dstPitch);
        uint16_t *dst1 = reinterpret_cast<uint16_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        // Process 8 source pixels at a time, producing 16 output pixels
        for (; x + 8 <= width; x += 8) {
            __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));

            // Duplicate each pixel: [A B C D E F G H] -> [A A B B C C D D] and [E E F F G G H H]
            __m128i lo = _mm_unpacklo_epi16(pixels, pixels);
            __m128i hi = _mm_unpackhi_epi16(pixels, pixels);

            // Store to both rows
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2]), lo);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2 + 8]), hi);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2]), lo);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2 + 8]), hi);
        }

        // Scalar tail
        for (; x < width; x++) {
            uint16_t color = src[x];
            dst0[x * 2] = color;
            dst0[x * 2 + 1] = color;
            dst1[x * 2] = color;
            dst1[x * 2 + 1] = color;
        }
    }
}

static void Simple3x32_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                             uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    for (int y = 0; y < height; y++) {
        uint32_t *src = reinterpret_cast<uint32_t*>(srcPtr + y * srcPitch);
        uint32_t *dst0 = reinterpret_cast<uint32_t*>(dstPtr + (y * 3) * dstPitch);
        uint32_t *dst1 = reinterpret_cast<uint32_t*>(dstPtr + (y * 3 + 1) * dstPitch);
        uint32_t *dst2 = reinterpret_cast<uint32_t*>(dstPtr + (y * 3 + 2) * dstPitch);

        int x = 0;
        // Process 4 source pixels, producing 12 output pixels per row
        // Use SSE2-only instructions (no blend which requires SSE4.1)
        for (; x + 4 <= width; x += 4) {
            __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));

            // Broadcast each pixel: [A B C D] -> individual broadcasts
            __m128i a = _mm_shuffle_epi32(pixels, _MM_SHUFFLE(0, 0, 0, 0));  // [A A A A]
            __m128i b = _mm_shuffle_epi32(pixels, _MM_SHUFFLE(1, 1, 1, 1));  // [B B B B]
            __m128i c = _mm_shuffle_epi32(pixels, _MM_SHUFFLE(2, 2, 2, 2));  // [C C C C]
            __m128i d = _mm_shuffle_epi32(pixels, _MM_SHUFFLE(3, 3, 3, 3));  // [D D D D]

            // Build output using shuffles only (SSE2 compatible)
            // out0 = [A A A B]: shuffle a and b, then combine
            __m128i ab = _mm_unpacklo_epi64(a, b);  // [A A B B]
            __m128i out0 = _mm_shuffle_epi32(ab, _MM_SHUFFLE(2, 0, 0, 0));  // [A A A B]

            // out1 = [B B C C]
            __m128i out1 = _mm_unpacklo_epi64(b, c);  // [B B C C]

            // out2 = [C D D D]
            __m128i cd = _mm_unpacklo_epi64(c, d);  // [C C D D]
            __m128i out2 = _mm_shuffle_epi32(cd, _MM_SHUFFLE(3, 3, 3, 0));  // [C D D D]

            // Store to all 3 rows
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 3]), out0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 3 + 4]), out1);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 3 + 8]), out2);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 3]), out0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 3 + 4]), out1);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 3 + 8]), out2);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst2[x * 3]), out0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst2[x * 3 + 4]), out1);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst2[x * 3 + 8]), out2);
        }

        // Scalar tail
        for (; x < width; x++) {
            uint32_t color = src[x];
            int dstX = x * 3;
            dst0[dstX] = dst0[dstX + 1] = dst0[dstX + 2] = color;
            dst1[dstX] = dst1[dstX + 1] = dst1[dstX + 2] = color;
            dst2[dstX] = dst2[dstX + 1] = dst2[dstX + 2] = color;
        }
    }
}

static void Simple4x32_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                             uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    for (int y = 0; y < height; y++) {
        uint32_t *src = reinterpret_cast<uint32_t*>(srcPtr + y * srcPitch);
        uint32_t *dst0 = reinterpret_cast<uint32_t*>(dstPtr + (y * 4) * dstPitch);
        uint32_t *dst1 = reinterpret_cast<uint32_t*>(dstPtr + (y * 4 + 1) * dstPitch);
        uint32_t *dst2 = reinterpret_cast<uint32_t*>(dstPtr + (y * 4 + 2) * dstPitch);
        uint32_t *dst3 = reinterpret_cast<uint32_t*>(dstPtr + (y * 4 + 3) * dstPitch);

        int x = 0;
        // Process 4 source pixels, producing 16 output pixels per row
        for (; x + 4 <= width; x += 4) {
            __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));

            // Broadcast each pixel to fill 4 positions
            __m128i a = _mm_shuffle_epi32(pixels, _MM_SHUFFLE(0, 0, 0, 0));  // [A A A A]
            __m128i b = _mm_shuffle_epi32(pixels, _MM_SHUFFLE(1, 1, 1, 1));  // [B B B B]
            __m128i c = _mm_shuffle_epi32(pixels, _MM_SHUFFLE(2, 2, 2, 2));  // [C C C C]
            __m128i d = _mm_shuffle_epi32(pixels, _MM_SHUFFLE(3, 3, 3, 3));  // [D D D D]

            // Store to all 4 rows
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 4]), a);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 4 + 4]), b);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 4 + 8]), c);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 4 + 12]), d);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 4]), a);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 4 + 4]), b);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 4 + 8]), c);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 4 + 12]), d);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst2[x * 4]), a);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst2[x * 4 + 4]), b);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst2[x * 4 + 8]), c);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst2[x * 4 + 12]), d);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst3[x * 4]), a);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst3[x * 4 + 4]), b);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst3[x * 4 + 8]), c);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst3[x * 4 + 12]), d);
        }

        // Scalar tail
        for (; x < width; x++) {
            uint32_t color = src[x];
            int dstX = x * 4;
            dst0[dstX] = dst0[dstX + 1] = dst0[dstX + 2] = dst0[dstX + 3] = color;
            dst1[dstX] = dst1[dstX + 1] = dst1[dstX + 2] = dst1[dstX + 3] = color;
            dst2[dstX] = dst2[dstX + 1] = dst2[dstX + 2] = dst2[dstX + 3] = color;
            dst3[dstX] = dst3[dstX + 1] = dst3[dstX + 2] = dst3[dstX + 3] = color;
        }
    }
}

#endif  // USE_SSE2

// ============================================================================
// Public API with dispatch
// ============================================================================

void Simple2x16(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
              uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
  Simple2x16_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
  uint8_t *nextLine, *finish;

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;

    finish = (uint8_t *) bP + ((width+2) << 1);
    currentPixel = *bP++;

    do {
#ifdef WORDS_BIGENDIAN
      uint32_t color = currentPixel >> 16;
#else
      uint32_t color = currentPixel & 0xffff;
#endif

      color = color | (color << 16);

      *(dP) = color;
      *(nL) = color;

#ifdef WORDS_BIGENDIAN
      color = currentPixel & 0xffff;
#else
      color = currentPixel >> 16;
#endif
      color = color| (color << 16);
      *(dP + 1) = color;
      *(nL + 1) = color;

      currentPixel = *bP++;

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

void Simple2x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
  Simple2x32_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
  uint8_t *nextLine, *finish;

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;

    finish = (uint8_t *) bP + ((width+1) << 2);
    currentPixel = *bP++;

    do {
      uint32_t color = currentPixel;

      *(dP) = color;
      *(dP+1) = color;
      *(nL) = color;
      *(nL + 1) = color;

      currentPixel = *bP++;

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


void Simple3x16(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
              uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#define magnification	3
#define colorBytes 2 // 16 bit colors = 2 byte colors

	// Generic Simple magnification filter
	int x, y; // Source Position Counter
	unsigned int dx, dy; // Destination pixel's pixels
	unsigned short col; // Source color

	srcPitch = (srcPitch / colorBytes) - width; // This is the part of the source pitch in pixels that is more than the source image
	dstPitch = dstPitch / colorBytes;

	unsigned short *src, *dst, *dst2;
	src = (unsigned short *)srcPtr; // Since everything is time-critical this should be better than converting the pointers x*y times
	dst = (unsigned short *)dstPtr;

	for (y = 0; y < height; y++) // Line
	{
		for (x = 0; x < width; x++) // Pixel in Line
		{
			col = *src;

      dst2 = dst;
			*dst2 = col;
			for (dy = 0; dy < magnification; dy++)
			{
				for (dx = 0; dx < magnification; dx++)
				{
					*dst2 = col;
					dst2++;
				}
				dst2+=dstPitch;
				dst2-=magnification;
			}

			src++;
			dst+=magnification;
		}
		src+=srcPitch;
		dst+=dstPitch * magnification;
		dst-=width * magnification;
	}
#undef magnification
#undef colorBytes
}



void Simple3x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
	Simple3x32_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
#define magnification	3
#define colorBytes 4 // 32 bit colors = 4 byte colors

	// Generic Simple magnification filter
	int x, y; // Source Position Counter
	unsigned int dx, dy; // Destination pixel's pixels
	unsigned int col; // Source color

	srcPitch = (srcPitch / colorBytes) - width; // This is the part of the source pitch in pixels that is more than the source image
	dstPitch = dstPitch / colorBytes;

	unsigned int *src, *dst, *dst2;
	src = (unsigned int *)srcPtr; // Since everything is time-critical this should be better than converting the pointers x*y times
	dst = (unsigned int *)dstPtr;

	for (y = 0; y < height; y++) // Line
	{
		for (x = 0; x < width; x++) // Pixel in Line
		{
			col = *src;

      dst2 = dst;
			*dst2 = col;
			for (dy = 0; dy < magnification; dy++)
			{
				for (dx = 0; dx < magnification; dx++)
				{
					*dst2 = col;
					dst2++;
				}
				dst2+=dstPitch;
				dst2-=magnification;
			}

			src++;
			dst+=magnification;
		}
		src+=srcPitch;
		dst+=dstPitch * magnification;
		dst-=width * magnification;
	}
#undef magnification
#undef colorBytes
#endif
}

void Simple4x16(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
              uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#define magnification	4
#define colorBytes 2 // 16 bit colors = 2 byte colors

	// Generic Simple magnification filter
	int x, y; // Source Position Counter
	unsigned int dx, dy; // Destination pixel's pixels
	unsigned short col; // Source color

	srcPitch = (srcPitch / colorBytes) - width; // This is the part of the source pitch in pixels that is more than the source image
	dstPitch = dstPitch / colorBytes;

	unsigned short *src, *dst, *dst2;
	src = (unsigned short *)srcPtr; // Since everything is time-critical this should be better than converting the pointers x*y times
	dst = (unsigned short *)dstPtr;

	for (y = 0; y < height; y++) // Line
	{
		for (x = 0; x < width; x++) // Pixel in Line
		{
			col = *src;

      dst2 = dst;
			*dst2 = col;
			for (dy = 0; dy < magnification; dy++)
			{
				for (dx = 0; dx < magnification; dx++)
				{
					*dst2 = col;
					dst2++;
				}
				dst2+=dstPitch;
				dst2-=magnification;
			}

			src++;
			dst+=magnification;
		}
		src+=srcPitch;
		dst+=dstPitch * magnification;
		dst-=width * magnification;
	}
#undef magnification
#undef colorBytes
}



void Simple4x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
	Simple4x32_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
#define magnification	4
#define colorBytes 4 // 32 bit colors = 4 byte colors

	// Generic Simple magnification filter
	int x, y; // Source Position Counter
	unsigned int dx, dy; // Destination pixel's pixels
	unsigned int col; // Source color

	srcPitch = (srcPitch / colorBytes) - width; // This is the part of the source pitch in pixels that is more than the source image
	dstPitch = dstPitch / colorBytes;

	unsigned int *src, *dst, *dst2;
	src = (unsigned int *)srcPtr; // Since everything is time-critical this should be better than converting the pointers x*y times
	dst = (unsigned int *)dstPtr;

	for (y = 0; y < height; y++) // Line
	{
		for (x = 0; x < width; x++) // Pixel in Line
		{
			col = *src;

      dst2 = dst;
			*dst2 = col;
			for (dy = 0; dy < magnification; dy++)
			{
				for (dx = 0; dx < magnification; dx++)
				{
					*dst2 = col;
					dst2++;
				}
				dst2+=dstPitch;
				dst2-=magnification;
			}

			src++;
			dst+=magnification;
		}
		src+=srcPitch;
		dst+=dstPitch * magnification;
		dst-=width * magnification;
	}
#undef magnification
#undef colorBytes
#endif
}
