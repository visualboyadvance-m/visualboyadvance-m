/**     Code adapted from Exult source code by Forgotten
 **	Scale.cc - Trying to scale with bilinear interpolation.
 **
 **	Written: 6/14/00 - JSF
 **/

#include "core/base/system.h"
#include "simd_common.h"

// ============================================================================
// SSE2 Implementations
// ============================================================================

#ifdef USE_SSE2

// Bilinear32: 2x scale with bilinear interpolation using SSE2
// Operates directly on packed ARGB pixels using byte-wise averaging
static void Bilinear32_SSE2(uint8_t *srcPtr, uint32_t srcPitch,
                             uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
    for (int y = 0; y < height; y++) {
        uint32_t *src0 = reinterpret_cast<uint32_t*>(srcPtr + y * srcPitch);
        uint32_t *src1 = (y + 1 < height) ?
            reinterpret_cast<uint32_t*>(srcPtr + (y + 1) * srcPitch) : src0;
        uint32_t *dst0 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2) * dstPitch);
        uint32_t *dst1 = reinterpret_cast<uint32_t*>(dstPtr + (y * 2 + 1) * dstPitch);

        int x = 0;
        // Process 4 source pixels at a time, output 8 destination pixels
        for (; x + 4 <= width; x += 4) {
            // Load 4 pixels from current row and next row
            __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src0[x]));
            __m128i c = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src1[x]));

            // Load shifted pixels (for right neighbors)
            __m128i b, d;
            if (x + 5 <= width) {
                b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src0[x + 1]));
                d = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src1[x + 1]));
            } else {
                // Edge handling: shift and replicate last pixel
                b = _mm_srli_si128(a, 4);
                b = _mm_or_si128(b, _mm_slli_si128(_mm_shuffle_epi32(a, _MM_SHUFFLE(3, 3, 3, 3)), 12));
                d = _mm_srli_si128(c, 4);
                d = _mm_or_si128(d, _mm_slli_si128(_mm_shuffle_epi32(c, _MM_SHUFFLE(3, 3, 3, 3)), 12));
            }

            // Compute interpolations using byte-wise averaging
            // Upper right: (a + b) / 2
            __m128i ab = _mm_avg_epu8(a, b);
            // Lower left: (a + c) / 2
            __m128i ac = _mm_avg_epu8(a, c);
            // Lower right: (a + b + c + d) / 4 = avg(avg(a,b), avg(c,d))
            __m128i cd = _mm_avg_epu8(c, d);
            __m128i abcd = _mm_avg_epu8(ab, cd);

            // Interleave results for output
            // Row 0: [a0 ab0 a1 ab1 a2 ab2 a3 ab3]
            __m128i row0_lo = _mm_unpacklo_epi32(a, ab);   // [a0 ab0 a1 ab1]
            __m128i row0_hi = _mm_unpackhi_epi32(a, ab);   // [a2 ab2 a3 ab3]

            // Row 1: [ac0 abcd0 ac1 abcd1 ac2 abcd2 ac3 abcd3]
            __m128i row1_lo = _mm_unpacklo_epi32(ac, abcd);
            __m128i row1_hi = _mm_unpackhi_epi32(ac, abcd);

            // Store row 0
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2]), row0_lo);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst0[x * 2 + 4]), row0_hi);

            // Store row 1
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2]), row1_lo);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst1[x * 2 + 4]), row1_hi);
        }

        // Scalar tail
        for (; x < width; x++) {
            uint32_t a_px = src0[x];
            uint32_t b_px = (x + 1 < width) ? src0[x + 1] : a_px;
            uint32_t c_px = src1[x];
            uint32_t d_px = (x + 1 < width) ? src1[x + 1] : c_px;

            // Use byte-wise averaging (matches _mm_avg_epu8 behavior)
            auto avg2 = [](uint32_t p1, uint32_t p2) -> uint32_t {
                uint32_t rb = ((p1 & 0xFF00FF) + (p2 & 0xFF00FF) + 0x010001) >> 1;
                uint32_t g  = ((p1 & 0x00FF00) + (p2 & 0x00FF00) + 0x000100) >> 1;
                return (rb & 0xFF00FF) | (g & 0x00FF00);
            };

            uint32_t ab = avg2(a_px, b_px);
            uint32_t ac = avg2(a_px, c_px);
            uint32_t cd = avg2(c_px, d_px);
            uint32_t abcd = avg2(ab, cd);

            dst0[x * 2] = a_px;
            dst0[x * 2 + 1] = ab;
            dst1[x * 2] = ac;
            dst1[x * 2 + 1] = abcd;
        }
    }
}

#endif  // USE_SSE2

#define RGB(r,g,b) ((r)>>3) << systemRedShift |\
  ((g) >> 3) << systemGreenShift |\
  ((b) >> 3) << systemBlueShift\

static void fill_rgb_row_16(uint16_t *from, int src_width, uint8_t *row, int width)
{
  uint8_t *copy_start = row + src_width*3;
  uint8_t *all_stop = row + width*3;
  while (row < copy_start) {
    uint16_t color = *from++;
    *row++ = ((color >> systemRedShift) & 0x1f) << 3;
    *row++ = ((color >> systemGreenShift) & 0x1f) << 3;
    *row++ = ((color >> systemBlueShift) & 0x1f) << 3;
  }
  // any remaining elements to be written to 'row' are a replica of the
  // preceding pixel
  uint8_t *p = row-3;
  while (row < all_stop) {
    // we're guaranteed three elements per pixel; could unroll the loop
    // further, especially with a Duff's Device, but the gains would be
    // probably limited (judging by profiler output)
    *row++ = *p++;
    *row++ = *p++;
    *row++ = *p++;
  }
}

static void fill_rgb_row_32(uint32_t *from, int src_width, uint8_t *row, int width)
{
  uint8_t *copy_start = row + src_width*3;
  uint8_t *all_stop = row + width*3;
  while (row < copy_start) {
    uint32_t color = *from++;
    *row++ = ((color >> systemRedShift) & 0x1f) << 3;
    *row++ = ((color >> systemGreenShift) & 0x1f) << 3;
    *row++ = ((color >> systemBlueShift) & 0x1f) << 3;
  }
  // any remaining elements to be written to 'row' are a replica of the
  // preceding pixel
  uint8_t *p = row-3;
  while (row < all_stop) {
    // we're guaranteed three elements per pixel; could unroll the loop
    // further, especially with a Duff's Device, but the gains would be
    // probably limited (judging by profiler output)
    *row++ = *p++;
    *row++ = *p++;
    *row++ = *p++;
  }
}

void Bilinear(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
              uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t row_cur[3*322];
  uint8_t row_next[3*322];
  uint8_t *rgb_row_cur = row_cur;
  uint8_t *rgb_row_next = row_next;

  uint16_t *to = (uint16_t *)dstPtr;
  uint16_t *to_odd = (uint16_t *)(dstPtr + dstPitch);

  int from_width = width;
  uint16_t *from = (uint16_t *)srcPtr;
  fill_rgb_row_16(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    uint16_t *from_orig = from;
    uint16_t *to_orig = to;

    if (y+1 < height)
      fill_rgb_row_16(from+width+2, from_width, rgb_row_next,
                   width+1);
    else
      fill_rgb_row_16(from, from_width, rgb_row_next, width+1);

    // every pixel in the src region, is extended to 4 pixels in the
    // destination, arranged in a square 'quad'; if the current src
    // pixel is 'a', then in what follows 'b' is the src pixel to the
    // right, 'c' is the src pixel below, and 'd' is the src pixel to
    // the right and down
    uint8_t *cur_row  = rgb_row_cur;
    uint8_t *next_row = rgb_row_next;
    uint8_t *ar = cur_row++;
    uint8_t *ag = cur_row++;
    uint8_t *ab = cur_row++;
    uint8_t *cr = next_row++;
    uint8_t *cg = next_row++;
    uint8_t *cb = next_row++;
    for(int x=0; x < width; x++) {
      uint8_t *br = cur_row++;
      uint8_t *bg = cur_row++;
      uint8_t *bb = cur_row++;
      uint8_t *dr = next_row++;
      uint8_t *dg = next_row++;
      uint8_t *db = next_row++;

      // upper left pixel in quad: just copy it in
      *to++ = RGB(*ar, *ag, *ab);

      // upper right
      *to++ = RGB((*ar+*br)>>1, (*ag+*bg)>>1, (*ab+*bb)>>1);

      // lower left
      *to_odd++ = RGB((*ar+*cr)>>1, (*ag+*cg)>>1, (*ab+*cb)>>1);

      // lower right
      *to_odd++ = RGB((*ar+*br+*cr+*dr)>>2,
                      (*ag+*bg+*cg+*dg)>>2,
                      (*ab+*bb+*cb+*db)>>2);

      // 'b' becomes 'a', 'd' becomes 'c'
      ar = br;
      ag = bg;
      ab = bb;
      cr = dr;
      cg = dg;
      cb = db;
    }

    // the "next" rgb row becomes the current; the old current rgb row is
    // recycled and serves as the new "next" row
    uint8_t *temp;
    temp = rgb_row_cur;
    rgb_row_cur = rgb_row_next;
    rgb_row_next = temp;

    // update the pointers for start of next pair of lines
    from = (uint16_t *)((uint8_t *)from_orig + srcPitch);
    to = (uint16_t *)((uint8_t *)to_orig + (dstPitch << 1));
    to_odd = (uint16_t *)((uint8_t *)to + dstPitch);
  }
}

void BilinearPlus(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                  uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t row_cur[3*322];
  uint8_t row_next[3*322];
  uint8_t *rgb_row_cur = row_cur;
  uint8_t *rgb_row_next = row_next;

  uint16_t *to = (uint16_t *)dstPtr;
  uint16_t *to_odd = (uint16_t *)(dstPtr + dstPitch);

  int from_width = width;
  uint16_t *from = (uint16_t *)srcPtr;
  fill_rgb_row_16(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    uint16_t *from_orig = from;
    uint16_t *to_orig = to;

    if (y+1 < height)
      fill_rgb_row_16(from+width+2, from_width, rgb_row_next,
                   width+1);
    else
      fill_rgb_row_16(from, from_width, rgb_row_next, width+1);

    // every pixel in the src region, is extended to 4 pixels in the
    // destination, arranged in a square 'quad'; if the current src
    // pixel is 'a', then in what follows 'b' is the src pixel to the
    // right, 'c' is the src pixel below, and 'd' is the src pixel to
    // the right and down
    uint8_t *cur_row  = rgb_row_cur;
    uint8_t *next_row = rgb_row_next;
    uint8_t *ar = cur_row++;
    uint8_t *ag = cur_row++;
    uint8_t *ab = cur_row++;
    uint8_t *cr = next_row++;
    uint8_t *cg = next_row++;
    uint8_t *cb = next_row++;
    for(int x=0; x < width; x++) {
      uint8_t *br = cur_row++;
      uint8_t *bg = cur_row++;
      uint8_t *bb = cur_row++;
      uint8_t *dr = next_row++;
      uint8_t *dg = next_row++;
      uint8_t *db = next_row++;

      // upper left pixel in quad: just copy it in
      //*to++ = manip.rgb(*ar, *ag, *ab);
#ifdef USE_ORIGINAL_BILINEAR_PLUS
      *to++ = RGB(
                  (((*ar)<<2) +((*ar)) + (*cr+*br+*br) )>> 3,
                  (((*ag)<<2) +((*ag)) + (*cg+*bg+*bg) )>> 3,
                  (((*ab)<<2) +((*ab)) + (*cb+*bb+*bb) )>> 3);
#else
      *to++ = RGB(
                  (((*ar)<<3) +((*ar)<<1) + (*cr+*br+*br+*cr) )>> 4,
                  (((*ag)<<3) +((*ag)<<1) + (*cg+*bg+*bg+*cg) )>> 4,
                  (((*ab)<<3) +((*ab)<<1) + (*cb+*bb+*bb+*cb) )>> 4);
#endif

      // upper right
      *to++ = RGB((*ar+*br)>>1, (*ag+*bg)>>1, (*ab+*bb)>>1);

      // lower left
      *to_odd++ = RGB((*ar+*cr)>>1, (*ag+*cg)>>1, (*ab+*cb)>>1);

      // lower right
      *to_odd++ = RGB((*ar+*br+*cr+*dr)>>2,
                      (*ag+*bg+*cg+*dg)>>2,
                      (*ab+*bb+*cb+*db)>>2);

      // 'b' becomes 'a', 'd' becomes 'c'
      ar = br;
      ag = bg;
      ab = bb;
      cr = dr;
      cg = dg;
      cb = db;
    }

    // the "next" rgb row becomes the current; the old current rgb row is
    // recycled and serves as the new "next" row
    uint8_t *temp;
    temp = rgb_row_cur;
    rgb_row_cur = rgb_row_next;
    rgb_row_next = temp;

    // update the pointers for start of next pair of lines
    from = (uint16_t *)((uint8_t *)from_orig + srcPitch);
    to = (uint16_t *)((uint8_t *)to_orig + (dstPitch << 1));
    to_odd = (uint16_t *)((uint8_t *)to + dstPitch);
  }
}

void Bilinear32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#ifdef USE_SSE2
  Bilinear32_SSE2(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
#else
  uint8_t row_cur[3*322];
  uint8_t row_next[3*322];
  uint8_t *rgb_row_cur = row_cur;
  uint8_t *rgb_row_next = row_next;

  uint32_t *to = (uint32_t *)dstPtr;
  uint32_t *to_odd = (uint32_t *)(dstPtr + dstPitch);

  int from_width = width;

  uint32_t *from = (uint32_t *)srcPtr;
  uint32_t Nextline = srcPitch >> 2;  // srcPitch in bytes / 4 = stride in uint32_t elements
  fill_rgb_row_32(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    uint32_t *from_orig = from;
    uint32_t *to_orig = to;

    if (y+1 < height)
      fill_rgb_row_32(from+Nextline, from_width, rgb_row_next,
                   width+1);
    else
      fill_rgb_row_32(from, from_width, rgb_row_next, width+1);

    // every pixel in the src region, is extended to 4 pixels in the
    // destination, arranged in a square 'quad'; if the current src
    // pixel is 'a', then in what follows 'b' is the src pixel to the
    // right, 'c' is the src pixel below, and 'd' is the src pixel to
    // the right and down
    uint8_t *cur_row  = rgb_row_cur;
    uint8_t *next_row = rgb_row_next;
    uint8_t *ar = cur_row++;
    uint8_t *ag = cur_row++;
    uint8_t *ab = cur_row++;
    uint8_t *cr = next_row++;
    uint8_t *cg = next_row++;
    uint8_t *cb = next_row++;
    for(int x=0; x < width; x++) {
      uint8_t *br = cur_row++;
      uint8_t *bg = cur_row++;
      uint8_t *bb = cur_row++;
      uint8_t *dr = next_row++;
      uint8_t *dg = next_row++;
      uint8_t *db = next_row++;

      // upper left pixel in quad: just copy it in
      *to++ = RGB(*ar, *ag, *ab);

      // upper right
      *to++ = RGB((*ar+*br)>>1, (*ag+*bg)>>1, (*ab+*bb)>>1);

      // lower left
      *to_odd++ = RGB((*ar+*cr)>>1, (*ag+*cg)>>1, (*ab+*cb)>>1);

      // lower right
      *to_odd++ = RGB((*ar+*br+*cr+*dr)>>2,
                      (*ag+*bg+*cg+*dg)>>2,
                      (*ab+*bb+*cb+*db)>>2);

      // 'b' becomes 'a', 'd' becomes 'c'
      ar = br;
      ag = bg;
      ab = bb;
      cr = dr;
      cg = dg;
      cb = db;
    }

    // the "next" rgb row becomes the current; the old current rgb row is
    // recycled and serves as the new "next" row
    uint8_t *temp;
    temp = rgb_row_cur;
    rgb_row_cur = rgb_row_next;
    rgb_row_next = temp;

    // update the pointers for start of next pair of lines
    from = (uint32_t *)((uint8_t *)from_orig + srcPitch);
    to = (uint32_t *)((uint8_t *)to_orig + (dstPitch << 1));
    to_odd = (uint32_t *)((uint8_t *)to + dstPitch);
  }
#endif
}

void BilinearPlus32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                    uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t row_cur[3*322];
  uint8_t row_next[3*322];
  uint8_t *rgb_row_cur = row_cur;
  uint8_t *rgb_row_next = row_next;

  uint32_t *to = (uint32_t *)dstPtr;
  uint32_t *to_odd = (uint32_t *)(dstPtr + dstPitch);

  int from_width = width;

  uint32_t *from = (uint32_t *)srcPtr;
  uint32_t Nextline = srcPitch >> 2;  // srcPitch in bytes / 4 = stride in uint32_t elements
  fill_rgb_row_32(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    uint32_t *from_orig = from;
    uint32_t *to_orig = to;

    if (y+1 < height)
      fill_rgb_row_32(from+Nextline, from_width, rgb_row_next,
                   width+1);
    else
      fill_rgb_row_32(from, from_width, rgb_row_next, width+1);

    // every pixel in the src region, is extended to 4 pixels in the
    // destination, arranged in a square 'quad'; if the current src
    // pixel is 'a', then in what follows 'b' is the src pixel to the
    // right, 'c' is the src pixel below, and 'd' is the src pixel to
    // the right and down
    uint8_t *cur_row  = rgb_row_cur;
    uint8_t *next_row = rgb_row_next;
    uint8_t *ar = cur_row++;
    uint8_t *ag = cur_row++;
    uint8_t *ab = cur_row++;
    uint8_t *cr = next_row++;
    uint8_t *cg = next_row++;
    uint8_t *cb = next_row++;
    for(int x=0; x < width; x++) {
      uint8_t *br = cur_row++;
      uint8_t *bg = cur_row++;
      uint8_t *bb = cur_row++;
      uint8_t *dr = next_row++;
      uint8_t *dg = next_row++;
      uint8_t *db = next_row++;

      // upper left pixel in quad: just copy it in
      //*to++ = manip.rgb(*ar, *ag, *ab);
#ifdef USE_ORIGINAL_BILINEAR_PLUS
      *to++ = RGB(
                  (((*ar)<<2) +((*ar)) + (*cr+*br+*br) )>> 3,
                  (((*ag)<<2) +((*ag)) + (*cg+*bg+*bg) )>> 3,
                  (((*ab)<<2) +((*ab)) + (*cb+*bb+*bb) )>> 3);
#else
      *to++ = RGB(
                  (((*ar)<<3) +((*ar)<<1) + (*cr+*br+*br+*cr) )>> 4,
                  (((*ag)<<3) +((*ag)<<1) + (*cg+*bg+*bg+*cg) )>> 4,
                  (((*ab)<<3) +((*ab)<<1) + (*cb+*bb+*bb+*cb) )>> 4);
#endif

      // upper right
      *to++ = RGB((*ar+*br)>>1, (*ag+*bg)>>1, (*ab+*bb)>>1);

      // lower left
      *to_odd++ = RGB((*ar+*cr)>>1, (*ag+*cg)>>1, (*ab+*cb)>>1);

      // lower right
      *to_odd++ = RGB((*ar+*br+*cr+*dr)>>2,
                      (*ag+*bg+*cg+*dg)>>2,
                      (*ab+*bb+*cb+*db)>>2);

      // 'b' becomes 'a', 'd' becomes 'c'
      ar = br;
      ag = bg;
      ab = bb;
      cr = dr;
      cg = dg;
      cb = db;
    }

    // the "next" rgb row becomes the current; the old current rgb row is
    // recycled and serves as the new "next" row
    uint8_t *temp;
    temp = rgb_row_cur;
    rgb_row_cur = rgb_row_next;
    rgb_row_next = temp;

    // update the pointers for start of next pair of lines
    from = (uint32_t *)((uint8_t *)from_orig + srcPitch);
    to = (uint32_t *)((uint8_t *)to_orig + (dstPitch << 1));
    to_odd = (uint32_t *)((uint8_t *)to + dstPitch);
  }
}

