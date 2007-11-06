/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 2003 Andrea Mazzoleni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Andrea Mazzoleni
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */
#include "System.h"
#include "interp.h"

/***************************************************************************/
/* HQ2x C implementation */

/*
 * This effect is a rewritten implementation of the hq2x effect made by Maxim Stepin
 */

static void hq2x_16_def(u16* dst0, u16* dst1, const u16* src0, const u16* src1, const u16* src2, unsigned count)
{
  unsigned i;

  for(i=0;i<count;++i) {
    unsigned char mask;

    u16 c[9];

    c[1] = src0[0];
    c[4] = src1[0];
    c[7] = src2[0];

    if (i>0) {
      c[0] = src0[-1];
      c[3] = src1[-1];
      c[6] = src2[-1];
    } else {
      c[0] = c[1];
      c[3] = c[4];
      c[6] = c[7];
    }

    if (i<count-1) {
      c[2] = src0[1];
      c[5] = src1[1];
      c[8] = src2[1];
    } else {
      c[2] = c[1];
      c[5] = c[4];
      c[8] = c[7];
    }

    mask = 0;

    if (interp_16_diff(c[0], c[4]))
      mask |= 1 << 0;
    if (interp_16_diff(c[1], c[4]))
      mask |= 1 << 1;
    if (interp_16_diff(c[2], c[4]))
      mask |= 1 << 2;
    if (interp_16_diff(c[3], c[4]))
      mask |= 1 << 3;
    if (interp_16_diff(c[5], c[4]))
      mask |= 1 << 4;
    if (interp_16_diff(c[6], c[4]))
      mask |= 1 << 5;
    if (interp_16_diff(c[7], c[4]))
      mask |= 1 << 6;
    if (interp_16_diff(c[8], c[4]))
      mask |= 1 << 7;

#define P0 dst0[0]
#define P1 dst0[1]
#define P2 dst1[0]
#define P3 dst1[1]
#define MUR interp_16_diff(c[1], c[5])
#define MDR interp_16_diff(c[5], c[7])
#define MDL interp_16_diff(c[7], c[3])
#define MUL interp_16_diff(c[3], c[1])
#define IC(p0) c[p0]
#define I11(p0,p1) interp_16_11(c[p0], c[p1])
#define I211(p0,p1,p2) interp_16_211(c[p0], c[p1], c[p2])
#define I31(p0,p1) interp_16_31(c[p0], c[p1])
#define I332(p0,p1,p2) interp_16_332(c[p0], c[p1], c[p2])
#define I431(p0,p1,p2) interp_16_431(c[p0], c[p1], c[p2])
#define I521(p0,p1,p2) interp_16_521(c[p0], c[p1], c[p2])
#define I53(p0,p1) interp_16_53(c[p0], c[p1])
#define I611(p0,p1,p2) interp_16_611(c[p0], c[p1], c[p2])
#define I71(p0,p1) interp_16_71(c[p0], c[p1])
#define I772(p0,p1,p2) interp_16_772(c[p0], c[p1], c[p2])
#define I97(p0,p1) interp_16_97(c[p0], c[p1])
#define I1411(p0,p1,p2) interp_16_1411(c[p0], c[p1], c[p2])
#define I151(p0,p1) interp_16_151(c[p0], c[p1])

    switch (mask) {
#include "hq2x.h"
    }

#undef P0
#undef P1
#undef P2
#undef P3
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef IC
#undef I11
#undef I211
#undef I31
#undef I332
#undef I431
#undef I521
#undef I53
#undef I611
#undef I71
#undef I772
#undef I97
#undef I1411
#undef I151

    src0 += 1;
    src1 += 1;
    src2 += 1;
    dst0 += 2;
    dst1 += 2;
  }
}

static void hq2x_32_def(u32* dst0, u32* dst1, const u32* src0, const u32* src1, const u32* src2, unsigned count)
{
  unsigned i;

  for(i=0;i<count;++i) {
    unsigned char mask;

    u32 c[9];

    c[1] = src0[0];
    c[4] = src1[0];
    c[7] = src2[0];

    if (i>0) {
      c[0] = src0[-1];
      c[3] = src1[-1];
      c[6] = src2[-1];
    } else {
      c[0] = c[1];
      c[3] = c[4];
      c[6] = c[7];
    }

    if (i<count-1) {
      c[2] = src0[1];
      c[5] = src1[1];
      c[8] = src2[1];
    } else {
      c[2] = c[1];
      c[5] = c[4];
      c[8] = c[7];
    }

    mask = 0;

    if (interp_32_diff(c[0], c[4]))
      mask |= 1 << 0;
    if (interp_32_diff(c[1], c[4]))
      mask |= 1 << 1;
    if (interp_32_diff(c[2], c[4]))
      mask |= 1 << 2;
    if (interp_32_diff(c[3], c[4]))
      mask |= 1 << 3;
    if (interp_32_diff(c[5], c[4]))
      mask |= 1 << 4;
    if (interp_32_diff(c[6], c[4]))
      mask |= 1 << 5;
    if (interp_32_diff(c[7], c[4]))
      mask |= 1 << 6;
    if (interp_32_diff(c[8], c[4]))
      mask |= 1 << 7;

#define P0 dst0[0]
#define P1 dst0[1]
#define P2 dst1[0]
#define P3 dst1[1]
#define MUR interp_32_diff(c[1], c[5])
#define MDR interp_32_diff(c[5], c[7])
#define MDL interp_32_diff(c[7], c[3])
#define MUL interp_32_diff(c[3], c[1])
#define IC(p0) c[p0]
#define I11(p0,p1) interp_32_11(c[p0], c[p1])
#define I211(p0,p1,p2) interp_32_211(c[p0], c[p1], c[p2])
#define I31(p0,p1) interp_32_31(c[p0], c[p1])
#define I332(p0,p1,p2) interp_32_332(c[p0], c[p1], c[p2])
#define I431(p0,p1,p2) interp_32_431(c[p0], c[p1], c[p2])
#define I521(p0,p1,p2) interp_32_521(c[p0], c[p1], c[p2])
#define I53(p0,p1) interp_32_53(c[p0], c[p1])
#define I611(p0,p1,p2) interp_32_611(c[p0], c[p1], c[p2])
#define I71(p0,p1) interp_32_71(c[p0], c[p1])
#define I772(p0,p1,p2) interp_32_772(c[p0], c[p1], c[p2])
#define I97(p0,p1) interp_32_97(c[p0], c[p1])
#define I1411(p0,p1,p2) interp_32_1411(c[p0], c[p1], c[p2])
#define I151(p0,p1) interp_32_151(c[p0], c[p1])

    switch (mask) {
#include "hq2x.h"
    }

#undef P0
#undef P1
#undef P2
#undef P3
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef IC
#undef I11
#undef I211
#undef I31
#undef I332
#undef I431
#undef I521
#undef I53
#undef I611
#undef I71
#undef I772
#undef I97
#undef I1411
#undef I151

    src0 += 1;
    src1 += 1;
    src2 += 1;
    dst0 += 2;
    dst1 += 2;
  }
}

/***************************************************************************/
/* LQ2x C implementation */

/*
 * This effect is derived from the hq2x effect made by Maxim Stepin
 */

static void lq2x_16_def(u16* dst0, u16* dst1, const u16* src0, const u16* src1, const u16* src2, unsigned count)
{
  unsigned i;

  for(i=0;i<count;++i) {
    unsigned char mask;

    u16 c[9];

    c[1] = src0[0];
    c[4] = src1[0];
    c[7] = src2[0];

    if (i>0) {
      c[0] = src0[-1];
      c[3] = src1[-1];
      c[6] = src2[-1];
    } else {
      c[0] = c[1];
      c[3] = c[4];
      c[6] = c[7];
    }

    if (i<count-1) {
      c[2] = src0[1];
      c[5] = src1[1];
      c[8] = src2[1];
    } else {
      c[2] = c[1];
      c[5] = c[4];
      c[8] = c[7];
    }

    mask = 0;

    if (c[0] != c[4])
      mask |= 1 << 0;
    if (c[1] != c[4])
      mask |= 1 << 1;
    if (c[2] != c[4])
      mask |= 1 << 2;
    if (c[3] != c[4])
      mask |= 1 << 3;
    if (c[5] != c[4])
      mask |= 1 << 4;
    if (c[6] != c[4])
      mask |= 1 << 5;
    if (c[7] != c[4])
      mask |= 1 << 6;
    if (c[8] != c[4])
      mask |= 1 << 7;

#define P0 dst0[0]
#define P1 dst0[1]
#define P2 dst1[0]
#define P3 dst1[1]
#define MUR (c[1] != c[5])
#define MDR (c[5] != c[7])
#define MDL (c[7] != c[3])
#define MUL (c[3] != c[1])
#define IC(p0) c[p0]
#define I11(p0,p1) interp_16_11(c[p0], c[p1])
#define I211(p0,p1,p2) interp_16_211(c[p0], c[p1], c[p2])
#define I31(p0,p1) interp_16_31(c[p0], c[p1])
#define I332(p0,p1,p2) interp_16_332(c[p0], c[p1], c[p2])
#define I431(p0,p1,p2) interp_16_431(c[p0], c[p1], c[p2])
#define I521(p0,p1,p2) interp_16_521(c[p0], c[p1], c[p2])
#define I53(p0,p1) interp_16_53(c[p0], c[p1])
#define I611(p0,p1,p2) interp_16_611(c[p0], c[p1], c[p2])
#define I71(p0,p1) interp_16_71(c[p0], c[p1])
#define I772(p0,p1,p2) interp_16_772(c[p0], c[p1], c[p2])
#define I97(p0,p1) interp_16_97(c[p0], c[p1])
#define I1411(p0,p1,p2) interp_16_1411(c[p0], c[p1], c[p2])
#define I151(p0,p1) interp_16_151(c[p0], c[p1])

    switch (mask) {
#include "lq2x.h"
    }

#undef P0
#undef P1
#undef P2
#undef P3
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef IC
#undef I11
#undef I211
#undef I31
#undef I332
#undef I431
#undef I521
#undef I53
#undef I611
#undef I71
#undef I772
#undef I97
#undef I1411
#undef I151

    src0 += 1;
    src1 += 1;
    src2 += 1;
    dst0 += 2;
    dst1 += 2;
  }
}

static void lq2x_32_def(u32* dst0, u32* dst1, const u32* src0, const u32* src1, const u32* src2, unsigned count)
{
  unsigned i;

  for(i=0;i<count;++i) {
    unsigned char mask;

    u32 c[9];

    c[1] = src0[0];
    c[4] = src1[0];
    c[7] = src2[0];

    if (i>0) {
      c[0] = src0[-1];
      c[3] = src1[-1];
      c[6] = src2[-1];
    } else {
      c[0] = c[1];
      c[3] = c[4];
      c[6] = c[7];
    }

    if (i<count-1) {
      c[2] = src0[1];
      c[5] = src1[1];
      c[8] = src2[1];
    } else {
      c[2] = c[1];
      c[5] = c[4];
      c[8] = c[7];
    }

    mask = 0;

    if (c[0] != c[4])
      mask |= 1 << 0;
    if (c[1] != c[4])
      mask |= 1 << 1;
    if (c[2] != c[4])
      mask |= 1 << 2;
    if (c[3] != c[4])
      mask |= 1 << 3;
    if (c[5] != c[4])
      mask |= 1 << 4;
    if (c[6] != c[4])
      mask |= 1 << 5;
    if (c[7] != c[4])
      mask |= 1 << 6;
    if (c[8] != c[4])
      mask |= 1 << 7;

#define P0 dst0[0]
#define P1 dst0[1]
#define P2 dst1[0]
#define P3 dst1[1]
#define MUR (c[1] != c[5])
#define MDR (c[5] != c[7])
#define MDL (c[7] != c[3])
#define MUL (c[3] != c[1])
#define IC(p0) c[p0]
#define I11(p0,p1) interp_32_11(c[p0], c[p1])
#define I211(p0,p1,p2) interp_32_211(c[p0], c[p1], c[p2])
#define I31(p0,p1) interp_32_31(c[p0], c[p1])
#define I332(p0,p1,p2) interp_32_332(c[p0], c[p1], c[p2])
#define I431(p0,p1,p2) interp_32_431(c[p0], c[p1], c[p2])
#define I521(p0,p1,p2) interp_32_521(c[p0], c[p1], c[p2])
#define I53(p0,p1) interp_32_53(c[p0], c[p1])
#define I611(p0,p1,p2) interp_32_611(c[p0], c[p1], c[p2])
#define I71(p0,p1) interp_32_71(c[p0], c[p1])
#define I772(p0,p1,p2) interp_32_772(c[p0], c[p1], c[p2])
#define I97(p0,p1) interp_32_97(c[p0], c[p1])
#define I1411(p0,p1,p2) interp_32_1411(c[p0], c[p1], c[p2])
#define I151(p0,p1) interp_32_151(c[p0], c[p1])

    switch (mask) {
#include "lq2x.h"
    }

#undef P0
#undef P1
#undef P2
#undef P3
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef IC
#undef I11
#undef I211
#undef I31
#undef I332
#undef I431
#undef I521
#undef I53
#undef I611
#undef I71
#undef I772
#undef I97
#undef I1411
#undef I151

    src0 += 1;
    src1 += 1;
    src2 += 1;
    dst0 += 2;
    dst1 += 2;
  }
}

void hq2x(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
          u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u16 *dst0 = (u16 *)dstPtr;
  u16 *dst1 = dst0 + (dstPitch >> 1);
  
  u16 *src0 = (u16 *)srcPtr;
  u16 *src1 = src0 + (srcPitch >> 1);
  u16 *src2 = src1 + (srcPitch >> 1);
  
  hq2x_16_def(dst0, dst1, src0, src0, src1, width);
  
  int count = height;
  
  count -= 2;
  while(count) {
    dst0 += dstPitch;
    dst1 += dstPitch;
    hq2x_16_def(dst0, dst1, src0, src1, src2, width);
    src0 = src1;
    src1 = src2;
    src2 += srcPitch >> 1;
    --count;
  }
  dst0 += dstPitch;
  dst1 += dstPitch;
  hq2x_16_def(dst0, dst1, src0, src1, src1, width);
}

void hq2x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
            u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u32 *dst0 = (u32 *)dstPtr;
  u32 *dst1 = dst0 + (dstPitch >> 2);
  
  u32 *src0 = (u32 *)srcPtr;
  u32 *src1 = src0 + (srcPitch >> 2);
  u32 *src2 = src1 + (srcPitch >> 2);
  hq2x_32_def(dst0, dst1, src0, src0, src1, width);
  
  int count = height;
  
  count -= 2;
  while(count) {
    dst0 += dstPitch >> 1;
    dst1 += dstPitch >> 1;
    hq2x_32_def(dst0, dst1, src0, src1, src2, width);
    src0 = src1;
    src1 = src2;
    src2 += srcPitch >> 2;
    --count;
  }
  dst0 += dstPitch >> 1;
  dst1 += dstPitch >> 1;
  hq2x_32_def(dst0, dst1, src0, src1, src1, width);
}

void lq2x(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
          u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u16 *dst0 = (u16 *)dstPtr;
  u16 *dst1 = dst0 + (dstPitch >> 1);
  
  u16 *src0 = (u16 *)srcPtr;
  u16 *src1 = src0 + (srcPitch >> 1);
  u16 *src2 = src1 + (srcPitch >> 1);
  
  lq2x_16_def(dst0, dst1, src0, src0, src1, width);
  
  int count = height;
  
  count -= 2;
  while(count) {
    dst0 += dstPitch;
    dst1 += dstPitch;
    lq2x_16_def(dst0, dst1, src0, src1, src2, width);
    src0 = src1;
    src1 = src2;
    src2 += srcPitch >> 1;
    --count;
  }
  dst0 += dstPitch;
  dst1 += dstPitch;
  lq2x_16_def(dst0, dst1, src0, src1, src1, width);
}

void lq2x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
            u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u32 *dst0 = (u32 *)dstPtr;
  u32 *dst1 = dst0 + (dstPitch >> 2);
  
  u32 *src0 = (u32 *)srcPtr;
  u32 *src1 = src0 + (srcPitch >> 2);
  u32 *src2 = src1 + (srcPitch >> 2);
  lq2x_32_def(dst0, dst1, src0, src0, src1, width);
  
  int count = height;
  
  count -= 2;
  while(count) {
    dst0 += dstPitch >> 1;
    dst1 += dstPitch >> 1;
    lq2x_32_def(dst0, dst1, src0, src1, src2, width);
    src0 = src1;
    src1 = src2;
    src2 += srcPitch >> 2;
    --count;
  }
  dst0 += dstPitch >> 1;
  dst1 += dstPitch >> 1;
  lq2x_32_def(dst0, dst1, src0, src1, src1, width);
}

void hq2x_init(unsigned bits_per_pixel)
{
  interp_set(bits_per_pixel);
}
