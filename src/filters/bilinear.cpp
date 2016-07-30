/**     Code adapted from Exult source code by Forgotten
 **	Scale.cc - Trying to scale with bilinear interpolation.
 **
 **	Written: 6/14/00 - JSF
 **/

#include "../System.h"

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
  uint8_t row_cur[3*322];
  uint8_t row_next[3*322];
  uint8_t *rgb_row_cur = row_cur;
  uint8_t *rgb_row_next = row_next;

  uint32_t *to = (uint32_t *)dstPtr;
  uint32_t *to_odd = (uint32_t *)(dstPtr + dstPitch);

  int from_width = width;

  uint32_t *from = (uint32_t *)srcPtr;
  fill_rgb_row_32(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    uint32_t *from_orig = from;
    uint32_t *to_orig = to;

    if (y+1 < height)
      fill_rgb_row_32(from+width+1, from_width, rgb_row_next,
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
  fill_rgb_row_32(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    uint32_t *from_orig = from;
    uint32_t *to_orig = to;

    if (y+1 < height)
      fill_rgb_row_32(from+width+1, from_width, rgb_row_next,
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

