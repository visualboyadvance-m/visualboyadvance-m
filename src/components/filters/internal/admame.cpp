/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 1999-2002 Andrea Mazzoleni
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

/*
 * Alternatively at the previous license terms, you are allowed to use this
 * code in your program with these conditions:
 * - the program is not used in commercial activities.
 * - the whole source code of the program is released with the binary.
 */

#include <cstdint>

#include "../simd128.h"

#ifdef MMX
extern "C" bool cpu_mmx;
#endif

namespace {

using namespace vbam_simd;

// ============================================================================
// Scale2x row kernel, scalar: dst gets 2 * count pixels from the three source
// rows (the row above, the row itself, the row below).
// ============================================================================

template <typename Pixel>
void internal_scale2x_def(Pixel* dst, const Pixel* src0, const Pixel* src1, const Pixel* src2, unsigned count) {
  /* first pixel */
  dst[0] = src1[0];
  if (src1[1] == src0[0] && src2[0] != src0[0])
    dst[1] = src0[0];
  else
    dst[1] = src1[0];
  ++src0;
  ++src1;
  ++src2;
  dst += 2;

  /* central pixels */
  count -= 2;
  while (count) {
    if (src0[0] != src2[0] && src1[-1] != src1[1]) {
      dst[0] = src1[-1] == src0[0] ? src0[0] : src1[0];
      dst[1] = src1[1] == src0[0] ? src0[0] : src1[0];
    } else {
      dst[0] = src1[0];
      dst[1] = src1[0];
    }

    ++src0;
    ++src1;
    ++src2;
    dst += 2;
    --count;
  }

  /* last pixel */
  if (src1[-1] == src0[0] && src2[0] != src0[0])
    dst[0] = src0[0];
  else
    dst[0] = src1[0];
  dst[1] = src1[0];
}

// ============================================================================
// Scale2x row kernel, vector (SSE2 / NEON): the central pixels a lane group at
// a time, the first and last pixel as in the scalar kernel.
// ============================================================================

struct Lanes32 {
  using Pixel = uint32_t;
  static constexpr int kLanes = 4;
  static V Eq(V a, V b) { return cmpeq_u32(a, b); }
  static void Store2(Pixel* p, V a, V b) { store2_u32(p, a, b); }
};

struct Lanes16 {
  using Pixel = uint16_t;
  static constexpr int kLanes = 8;
  static V Eq(V a, V b) { return cmpeq_u16(a, b); }
  static void Store2(Pixel* p, V a, V b) { store2_u16(p, a, b); }
};

template <class L>
void internal_scale2x_vec(typename L::Pixel* dst, const typename L::Pixel* src0, const typename L::Pixel* src1,
                          const typename L::Pixel* src2, unsigned count) {
  using Pixel = typename L::Pixel;

  dst[0] = src1[0];
  if (src1[1] == src0[0] && src2[0] != src0[0])
    dst[1] = src0[0];
  else
    dst[1] = src1[0];
  ++src0; ++src1; ++src2;
  dst += 2;
  count -= 2;

  // central pixels: the left and right neighbours are unaligned loads at +-1
  while (count >= L::kLanes) {
    const V cur = load(src1);
    const V left = load(src1 - 1);
    const V right = load(src1 + 1);
    const V top = load(src0);
    const V bot = load(src2);

    const V condition = andnot(L::Eq(top, bot), not_(L::Eq(left, right)));  // top != bot && left != right
    const V leftSel = and_(condition, L::Eq(left, top));
    const V rightSel = and_(condition, L::Eq(right, top));
    const V leftOut = vselect(leftSel, top, cur);
    const V rightOut = vselect(rightSel, top, cur);

    L::Store2(dst, leftOut, rightOut);

    src0 += L::kLanes; src1 += L::kLanes; src2 += L::kLanes;
    dst += 2 * L::kLanes;
    count -= L::kLanes;
  }

  while (count > 0) {
    if (src0[0] != src2[0] && src1[-1] != src1[1]) {
      dst[0] = src1[-1] == src0[0] ? src0[0] : src1[0];
      dst[1] = src1[1] == src0[0] ? src0[0] : src1[0];
    } else {
      dst[0] = src1[0];
      dst[1] = src1[0];
    }
    ++src0; ++src1; ++src2;
    dst += 2;
    --count;
  }

  if (src1[-1] == src0[0] && src2[0] != src0[0])
    dst[0] = src0[0];
  else
    dst[0] = src1[0];
  dst[1] = src1[0];
  (void)sizeof(Pixel);
}

// Whole frame through a row kernel: the first and last rows use themselves as
// the missing neighbour row.
template <typename Pixel, class RowKernel>
void scale2x_frame(RowKernel row, uint8_t* srcPtr, uint32_t srcPitch, uint8_t* dstPtr, uint32_t dstPitch, int width,
                   int height) {
  const Pixel* src0 = reinterpret_cast<const Pixel*>(srcPtr);
  const Pixel* src1 = reinterpret_cast<const Pixel*>(srcPtr + srcPitch);
  const Pixel* src2 = reinterpret_cast<const Pixel*>(srcPtr + 2 * srcPitch);
  Pixel* dst0 = reinterpret_cast<Pixel*>(dstPtr);
  Pixel* dst1 = reinterpret_cast<Pixel*>(dstPtr + dstPitch);

  if (height == 1) {
    row(dst0, src0, src0, src0, width);
    row(dst1, src0, src0, src0, width);
    return;
  }

  row(dst0, src0, src0, src1, width);
  row(dst1, src1, src0, src0, width);

  int count = height - 2;
  while (count) {
    dst0 = reinterpret_cast<Pixel*>(reinterpret_cast<uint8_t*>(dst0) + 2 * dstPitch);
    dst1 = reinterpret_cast<Pixel*>(reinterpret_cast<uint8_t*>(dst1) + 2 * dstPitch);
    row(dst0, src0, src1, src2, width);
    row(dst1, src2, src1, src0, width);
    src0 = src1;
    src1 = src2;
    src2 = reinterpret_cast<const Pixel*>(reinterpret_cast<const uint8_t*>(src2) + srcPitch);
    --count;
  }
  dst0 = reinterpret_cast<Pixel*>(reinterpret_cast<uint8_t*>(dst0) + 2 * dstPitch);
  dst1 = reinterpret_cast<Pixel*>(reinterpret_cast<uint8_t*>(dst1) + 2 * dstPitch);
  row(dst0, src0, src1, src1, width);
  row(dst1, src1, src1, src0, width);
}

}  // namespace

#ifdef MMX
static void internal_scale2x_16_mmx_single(uint16_t* dst, const uint16_t* src0, const uint16_t* src1, const uint16_t* src2, unsigned count) {
  /* always do the first and last run */
  count -= 2*4;

#ifdef __GNUC__
  __asm__ __volatile__(
                       /* first run */
                       /* set the current, current_pre, current_next registers */
                       "movq 0(%1), %%mm0\n"
                       "movq 0(%1),%%mm7\n"
                       "movq 8(%1),%%mm1\n"
                       "psllq $48,%%mm0\n"
                       "psllq $48,%%mm1\n"
                       "psrlq $48, %%mm0\n"
                       "movq %%mm7,%%mm2\n"
                       "movq %%mm7,%%mm3\n"
                       "psllq $16,%%mm2\n"
                       "psrlq $16,%%mm3\n"
                       "por %%mm2,%%mm0\n"
                       "por %%mm3,%%mm1\n"

                       /* current_upper */
                       "movq (%0),%%mm6\n"

                       /* compute the upper-left pixel for dst on %%mm2 */
                       /* compute the upper-right pixel for dst on %%mm4 */
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "movq %%mm0,%%mm3\n"
                       "movq %%mm1,%%mm5\n"
                       "pcmpeqw %%mm6,%%mm2\n"
                       "pcmpeqw %%mm6,%%mm4\n"
                       "pcmpeqw (%2),%%mm3\n"
                       "pcmpeqw (%2),%%mm5\n"
                       "pandn %%mm2,%%mm3\n"
                       "pandn %%mm4,%%mm5\n"
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "pcmpeqw %%mm1,%%mm2\n"
                       "pcmpeqw %%mm0,%%mm4\n"
                       "pandn %%mm3,%%mm2\n"
                       "pandn %%mm5,%%mm4\n"
                       "movq %%mm2,%%mm3\n"
                       "movq %%mm4,%%mm5\n"
                       "pand %%mm6,%%mm2\n"
                       "pand %%mm6,%%mm4\n"
                       "pandn %%mm7,%%mm3\n"
                       "pandn %%mm7,%%mm5\n"
                       "por %%mm3,%%mm2\n"
                       "por %%mm5,%%mm4\n"

                       /* set *dst */
                       "movq %%mm2,%%mm3\n"
                       "punpcklwd %%mm4,%%mm2\n"
                       "punpckhwd %%mm4,%%mm3\n"
                       "movq %%mm2,(%3)\n"
                       "movq %%mm3,8(%3)\n"

                       /* next */
                       "addl $8,%0\n"
                       "addl $8,%1\n"
                       "addl $8,%2\n"
                       "addl $16,%3\n"

                       /* central runs */
                       "shrl $2,%4\n"
                       "jz 1f\n"

                       "0:\n"

                       /* set the current, current_pre, current_next registers */
                       "movq -8(%1),%%mm0\n"
                       "movq (%1),%%mm7\n"
                       "movq 8(%1),%%mm1\n"
                       "psrlq $48,%%mm0\n"
                       "psllq $48,%%mm1\n"
                       "movq %%mm7,%%mm2\n"
                       "movq %%mm7,%%mm3\n"
                       "psllq $16,%%mm2\n"
                       "psrlq $16,%%mm3\n"
                       "por %%mm2,%%mm0\n"
                       "por %%mm3,%%mm1\n"

                       /* current_upper */
                       "movq (%0),%%mm6\n"

                       /* compute the upper-left pixel for dst on %%mm2 */
                       /* compute the upper-right pixel for dst on %%mm4 */
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "movq %%mm0,%%mm3\n"
                       "movq %%mm1,%%mm5\n"
                       "pcmpeqw %%mm6,%%mm2\n"
                       "pcmpeqw %%mm6,%%mm4\n"
                       "pcmpeqw (%2),%%mm3\n"
                       "pcmpeqw (%2),%%mm5\n"
                       "pandn %%mm2,%%mm3\n"
                       "pandn %%mm4,%%mm5\n"
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "pcmpeqw %%mm1,%%mm2\n"
                       "pcmpeqw %%mm0,%%mm4\n"
                       "pandn %%mm3,%%mm2\n"
                       "pandn %%mm5,%%mm4\n"
                       "movq %%mm2,%%mm3\n"
                       "movq %%mm4,%%mm5\n"
                       "pand %%mm6,%%mm2\n"
                       "pand %%mm6,%%mm4\n"
                       "pandn %%mm7,%%mm3\n"
                       "pandn %%mm7,%%mm5\n"
                       "por %%mm3,%%mm2\n"
                       "por %%mm5,%%mm4\n"

                       /* set *dst */
                       "movq %%mm2,%%mm3\n"
                       "punpcklwd %%mm4,%%mm2\n"
                       "punpckhwd %%mm4,%%mm3\n"
                       "movq %%mm2,(%3)\n"
                       "movq %%mm3,8(%3)\n"

                       /* next */
                       "addl $8,%0\n"
                       "addl $8,%1\n"
                       "addl $8,%2\n"
                       "addl $16,%3\n"

                       "decl %4\n"
                       "jnz 0b\n"
                       "1:\n"

                       /* final run */
                       /* set the current, current_pre, current_next registers */
                       "movq (%1),%%mm1\n"
                       "movq (%1),%%mm7\n"
                       "movq -8(%1),%%mm0\n"
                       "psrlq $48,%%mm1\n"
                       "psrlq $48,%%mm0\n"
                       "psllq $48,%%mm1\n"
                       "movq %%mm7,%%mm2\n"
                       "movq %%mm7,%%mm3\n"
                       "psllq $16,%%mm2\n"
                       "psrlq $16,%%mm3\n"
                       "por %%mm2,%%mm0\n"
                       "por %%mm3,%%mm1\n"

                       /* current_upper */
                       "movq (%0),%%mm6\n"

                       /* compute the upper-left pixel for dst on %%mm2 */
                       /* compute the upper-right pixel for dst on %%mm4 */
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "movq %%mm0,%%mm3\n"
                       "movq %%mm1,%%mm5\n"
                       "pcmpeqw %%mm6,%%mm2\n"
                       "pcmpeqw %%mm6,%%mm4\n"
                       "pcmpeqw (%2),%%mm3\n"
                       "pcmpeqw (%2),%%mm5\n"
                       "pandn %%mm2,%%mm3\n"
                       "pandn %%mm4,%%mm5\n"
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "pcmpeqw %%mm1,%%mm2\n"
                       "pcmpeqw %%mm0,%%mm4\n"
                       "pandn %%mm3,%%mm2\n"
                       "pandn %%mm5,%%mm4\n"
                       "movq %%mm2,%%mm3\n"
                       "movq %%mm4,%%mm5\n"
                       "pand %%mm6,%%mm2\n"
                       "pand %%mm6,%%mm4\n"
                       "pandn %%mm7,%%mm3\n"
                       "pandn %%mm7,%%mm5\n"
                       "por %%mm3,%%mm2\n"
                       "por %%mm5,%%mm4\n"

                       /* set *dst */
                       "movq %%mm2,%%mm3\n"
                       "punpcklwd %%mm4,%%mm2\n"
                       "punpckhwd %%mm4,%%mm3\n"
                       "movq %%mm2,(%3)\n"
                       "movq %%mm3,8(%3)\n"
                       "emms\n"

                       : "+r" (src0), "+r" (src1), "+r" (src2), "+r" (dst), "+r" (count)
                       :
                       : "cc"
                       );
#else
  __asm {
    mov eax, src0;
    mov ebx, src1;
    mov ecx, src2;
    mov edx, dst;
    mov esi, count;

    /* first run */
    /* set the current, current_pre, current_next registers */
    movq mm0, qword ptr [ebx];
    movq mm7, qword ptr [ebx];
    movq mm1, qword ptr [ebx + 8];
    psllq mm0, 48;
    psllq mm1, 48;
    psrlq mm0, 48;
    movq mm2, mm7;
    movq mm3, mm7;
    psllq mm2, 16;
    psrlq mm3, 16;
    por mm0, mm2;
    por mm1, mm3;

    /* current_upper */
    movq mm6, qword ptr [eax];

    /* compute the upper-left pixel for dst on %%mm2 */
    /* compute the upper-right pixel for dst on %%mm4 */
    movq mm2, mm0;
    movq mm4, mm1;
    movq mm3, mm0;
    movq mm5, mm1;
    pcmpeqw mm2, mm6;
    pcmpeqw mm4, mm6;
    pcmpeqw mm3, qword ptr [ecx];
    pcmpeqw mm5, qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqw mm2,mm1;
    pcmpeqw mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst0 */
    movq mm3,mm2;
    punpcklwd mm2,mm4;
    punpckhwd mm3,mm4;
    movq qword ptr [edx], mm2;
    movq qword ptr [edx + 8], mm3;

    /* next */
    add eax, 8;
    add ebx, 8;
    add ecx, 8;
    add edx, 16;

    /* central runs */
    shr esi, 2;
    jz label1;
    align 4;
  label0:

    /* set the current, current_pre, current_next registers */
    movq mm0, qword ptr [ebx-8];
    movq mm7, qword ptr [ebx];
    movq mm1, qword ptr [ebx+8];
    psrlq mm0,48;
    psllq mm1,48;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,16;
    psrlq mm3,16;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6, qword ptr [eax];

    /* compute the upper-left pixel for dst on %%mm2 */
    /* compute the upper-right pixel for dst on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqw mm2,mm6;
    pcmpeqw mm4,mm6;
    pcmpeqw mm3, qword ptr [ecx];
    pcmpeqw mm5, qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqw mm2,mm1;
    pcmpeqw mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst */
    movq mm3,mm2;
    punpcklwd mm2,mm4;
    punpckhwd mm3,mm4;
    movq qword ptr [edx], mm2;
    movq qword ptr [edx+8], mm3;

    /* next */
    add eax,8;
    add ebx,8;
    add ecx,8;
    add edx,16;

    dec esi;
    jnz label0;
  label1:

    /* final run */
    /* set the current, current_pre, current_next registers */
    movq mm1, qword ptr [ebx];
    movq mm7, qword ptr [ebx];
    movq mm0, qword ptr [ebx-8];
    psrlq mm1,48;
    psrlq mm0,48;
    psllq mm1,48;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,16;
    psrlq mm3,16;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6, qword ptr [eax];

    /* compute the upper-left pixel for dst on %%mm2 */
    /* compute the upper-right pixel for dst on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqw mm2,mm6;
    pcmpeqw mm4,mm6;
    pcmpeqw mm3, qword ptr [ecx];
    pcmpeqw mm5, qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqw mm2,mm1;
    pcmpeqw mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst */
    movq mm3,mm2;
    punpcklwd mm2,mm4;
    punpckhwd mm3,mm4;
    movq qword ptr [edx], mm2;
    movq qword ptr [edx+8], mm3;

    mov src0, eax;
    mov src1, ebx;
    mov src2, ecx;
    mov dst, edx;
    mov count, esi;

    emms;
  }
#endif
}

static void internal_scale2x_32_mmx_single(uint32_t* dst, const uint32_t* src0, const uint32_t* src1, const uint32_t* src2, unsigned count) {
  /* always do the first and last run */
  count -= 2*2;

#ifdef __GNUC__
  __asm__ __volatile__(
                       /* first run */
                       /* set the current, current_pre, current_next registers */
                       "movq 0(%1),%%mm0\n"
                       "movq 0(%1),%%mm7\n"
                       "movq 8(%1),%%mm1\n"
                       "psllq $32,%%mm0\n"
                       "psllq $32,%%mm1\n"
                       "psrlq $32,%%mm0\n"
                       "movq %%mm7,%%mm2\n"
                       "movq %%mm7,%%mm3\n"
                       "psllq $32,%%mm2\n"
                       "psrlq $32,%%mm3\n"
                       "por %%mm2,%%mm0\n"
                       "por %%mm3,%%mm1\n"

                       /* current_upper */
                       "movq (%0),%%mm6\n"

                       /* compute the upper-left pixel for dst on %%mm2 */
                       /* compute the upper-right pixel for dst on %%mm4 */
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "movq %%mm0,%%mm3\n"
                       "movq %%mm1,%%mm5\n"
                       "pcmpeqd %%mm6,%%mm2\n"
                       "pcmpeqd %%mm6,%%mm4\n"
                       "pcmpeqd (%2),%%mm3\n"
                       "pcmpeqd (%2),%%mm5\n"
                       "pandn %%mm2,%%mm3\n"
                       "pandn %%mm4,%%mm5\n"
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "pcmpeqd %%mm1,%%mm2\n"
                       "pcmpeqd %%mm0,%%mm4\n"
                       "pandn %%mm3,%%mm2\n"
                       "pandn %%mm5,%%mm4\n"
                       "movq %%mm2,%%mm3\n"
                       "movq %%mm4,%%mm5\n"
                       "pand %%mm6,%%mm2\n"
                       "pand %%mm6,%%mm4\n"
                       "pandn %%mm7,%%mm3\n"
                       "pandn %%mm7,%%mm5\n"
                       "por %%mm3,%%mm2\n"
                       "por %%mm5,%%mm4\n"

                       /* set *dst */
                       "movq %%mm2,%%mm3\n"
                       "punpckldq %%mm4,%%mm2\n"
                       "punpckhdq %%mm4,%%mm3\n"
                       "movq %%mm2,(%3)\n"
                       "movq %%mm3, 8(%3)\n"

                       /* next */
                       "addl $8,%0\n"
                       "addl $8,%1\n"
                       "addl $8,%2\n"
                       "addl $16,%3\n"

                       /* central runs */
                       "shrl $1,%4\n"
                       "jz 1f\n"

                       "0:\n"

                       /* set the current, current_pre, current_next registers */
                       "movq -8(%1),%%mm0\n"
                       "movq (%1),%%mm7\n"
                       "movq 8(%1),%%mm1\n"
                       "psrlq $32,%%mm0\n"
                       "psllq $32,%%mm1\n"
                       "movq %%mm7,%%mm2\n"
                       "movq %%mm7,%%mm3\n"
                       "psllq $32,%%mm2\n"
                       "psrlq $32,%%mm3\n"
                       "por %%mm2,%%mm0\n"
                       "por %%mm3,%%mm1\n"

                       /* current_upper */
                       "movq (%0),%%mm6\n"

                       /* compute the upper-left pixel for dst on %%mm2 */
                       /* compute the upper-right pixel for dst on %%mm4 */
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "movq %%mm0,%%mm3\n"
                       "movq %%mm1,%%mm5\n"
                       "pcmpeqd %%mm6,%%mm2\n"
                       "pcmpeqd %%mm6,%%mm4\n"
                       "pcmpeqd (%2),%%mm3\n"
                       "pcmpeqd (%2),%%mm5\n"
                       "pandn %%mm2,%%mm3\n"
                       "pandn %%mm4,%%mm5\n"
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "pcmpeqd %%mm1,%%mm2\n"
                       "pcmpeqd %%mm0,%%mm4\n"
                       "pandn %%mm3,%%mm2\n"
                       "pandn %%mm5,%%mm4\n"
                       "movq %%mm2,%%mm3\n"
                       "movq %%mm4,%%mm5\n"
                       "pand %%mm6,%%mm2\n"
                       "pand %%mm6,%%mm4\n"
                       "pandn %%mm7,%%mm3\n"
                       "pandn %%mm7,%%mm5\n"
                       "por %%mm3,%%mm2\n"
                       "por %%mm5,%%mm4\n"

                       /* set *dst */
                       "movq %%mm2,%%mm3\n"
                       "punpckldq %%mm4,%%mm2\n"
                       "punpckhdq %%mm4,%%mm3\n"
                       "movq %%mm2,(%3)\n"
                       "movq %%mm3,8(%3)\n"

                       /* next */
                       "addl $8,%0\n"
                       "addl $8,%1\n"
                       "addl $8,%2\n"
                       "addl $16,%3\n"

                       "decl %4\n"
                       "jnz 0b\n"
                       "1:\n"

                       /* final run */
                       /* set the current, current_pre, current_next registers */
                       "movq (%1),%%mm1\n"
                       "movq (%1),%%mm7\n"
                       "movq -8(%1), %%mm0\n"
                       "psrlq $32,%%mm1\n"
                       "psrlq $32,%%mm0\n"
                       "psllq $32,%%mm1\n"
                       "movq %%mm7,%%mm2\n"
                       "movq %%mm7,%%mm3\n"
                       "psllq $32,%%mm2\n"
                       "psrlq $32,%%mm3\n"
                       "por %%mm2,%%mm0\n"
                       "por %%mm3,%%mm1\n"

                       /* current_upper */
                       "movq (%0),%%mm6\n"

                       /* compute the upper-left pixel for dst on %%mm2 */
                       /* compute the upper-right pixel for dst on %%mm4 */
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "movq %%mm0,%%mm3\n"
                       "movq %%mm1,%%mm5\n"
                       "pcmpeqd %%mm6,%%mm2\n"
                       "pcmpeqd %%mm6,%%mm4\n"
                       "pcmpeqd (%2),%%mm3\n"
                       "pcmpeqd (%2),%%mm5\n"
                       "pandn %%mm2,%%mm3\n"
                       "pandn %%mm4,%%mm5\n"
                       "movq %%mm0,%%mm2\n"
                       "movq %%mm1,%%mm4\n"
                       "pcmpeqd %%mm1,%%mm2\n"
                       "pcmpeqd %%mm0,%%mm4\n"
                       "pandn %%mm3,%%mm2\n"
                       "pandn %%mm5,%%mm4\n"
                       "movq %%mm2,%%mm3\n"
                       "movq %%mm4,%%mm5\n"
                       "pand %%mm6,%%mm2\n"
                       "pand %%mm6,%%mm4\n"
                       "pandn %%mm7,%%mm3\n"
                       "pandn %%mm7,%%mm5\n"
                       "por %%mm3,%%mm2\n"
                       "por %%mm5,%%mm4\n"

                       /* set *dst */
                       "movq %%mm2,%%mm3\n"
                       "punpckldq %%mm4,%%mm2\n"
                       "punpckhdq %%mm4,%%mm3\n"
                       "movq %%mm2,(%3)\n"
                       "movq %%mm3,8(%3)\n"
                       "emms\n"

                       : "+r" (src0), "+r" (src1), "+r" (src2), "+r" (dst), "+r" (count)
                       :
                       : "cc"
                       );
#else
  __asm {
    mov eax, src0;
    mov ebx, src1;
    mov ecx, src2;
    mov edx, dst;
    mov esi, count;

    /* first run */
    /* set the current, current_pre, current_next registers */
    movq mm0,qword ptr [ebx];
    movq mm7,qword ptr [ebx];
    movq mm1,qword ptr [ebx + 8];
    psllq mm0,32;
    psllq mm1,32;
    psrlq mm0,32;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,32;
    psrlq mm3,32;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6,qword ptr [eax];

    /* compute the upper-left pixel for dst on %%mm2 */
    /* compute the upper-right pixel for dst on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqd mm2,mm6;
    pcmpeqd mm4,mm6;
    pcmpeqd mm3,qword ptr [ecx];
    pcmpeqd mm5,qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqd mm2,mm1;
    pcmpeqd mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst */
    movq mm3,mm2;
    punpckldq mm2,mm4;
    punpckhdq mm3,mm4;
    movq qword ptr [edx],mm2;
    movq qword ptr [edx+8],mm3;

    /* next */
    add eax,8;
    add ebx,8;
    add ecx,8;
    add edx,16;

    /* central runs */
    shr esi,1;
    jz label1;
label0:

  /* set the current, current_pre, current_next registers */
    movq mm0,qword ptr [ebx-8];
    movq mm7,qword ptr [ebx];
    movq mm1,qword ptr [ebx+8];
    psrlq mm0,32;
    psllq mm1,32;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,32;
    psrlq mm3,32;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6,qword ptr[eax];

    /* compute the upper-left pixel for dst on %%mm2 */
    /* compute the upper-right pixel for dst on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqd mm2,mm6;
    pcmpeqd mm4,mm6;
    pcmpeqd mm3,qword ptr[ecx];
    pcmpeqd mm5,qword ptr[ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqd mm2,mm1;
    pcmpeqd mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst */
    movq mm3,mm2;
    punpckldq mm2,mm4;
    punpckhdq mm3,mm4;
    movq qword ptr [edx],mm2;
    movq qword ptr [edx+8],mm3;

    /* next */
    add eax,8;
    add ebx,8;
    add ecx,8;
    add edx,16;

    dec esi;
    jnz label0;
label1:

    /* final run */
    /* set the current, current_pre, current_next registers */
    movq mm1,qword ptr [ebx];
    movq mm7,qword ptr [ebx];
    movq mm0,qword ptr [ebx-8];
    psrlq mm1,32;
    psrlq mm0,32;
    psllq mm1,32;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,32;
    psrlq mm3,32;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6,qword ptr [eax];

    /* compute the upper-left pixel for dst on %%mm2 */
    /* compute the upper-right pixel for dst on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqd mm2,mm6;
    pcmpeqd mm4,mm6;
    pcmpeqd mm3,qword ptr [ecx];
    pcmpeqd mm5,qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqd mm2,mm1;
    pcmpeqd mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst */
    movq mm3,mm2;
    punpckldq mm2,mm4;
    punpckhdq mm3,mm4;
    movq qword ptr [edx],mm2;
    movq qword ptr [edx+8],mm3;

    mov src0, eax;
    mov src1, ebx;
    mov src2, ecx;
    mov dst, edx;
    mov count, esi;

    emms;
  }
#endif
}


static void internal_scale2x_16_mmx(uint16_t* dst0, uint16_t* dst1, const uint16_t* src0, const uint16_t* src1, const uint16_t* src2, unsigned count) {
  //	assert( count >= 2*4 );
  internal_scale2x_16_mmx_single(dst0, src0, src1, src2, count);
  internal_scale2x_16_mmx_single(dst1, src2, src1, src0, count);
}

static void internal_scale2x_32_mmx(uint32_t* dst0, uint32_t* dst1, const uint32_t* src0, const uint32_t* src1, const uint32_t* src2, unsigned count) {
  //	assert( count >= 2*2 );
  internal_scale2x_32_mmx_single(dst0, src0, src1, src2, count);
  internal_scale2x_32_mmx_single(dst1, src2, src1, src0, count);
}
#endif

void AdMame2x(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
              uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  if (Enabled()) {
    scale2x_frame<uint16_t>(internal_scale2x_vec<Lanes16>, srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    return;
  }
#ifdef MMX
  if(cpu_mmx) {
    uint16_t *dst0 = (uint16_t *)dstPtr;
    uint16_t *dst1 = dst0 + (dstPitch >> 1);

    uint16_t *src0 = (uint16_t *)srcPtr;
    uint16_t *src1 = src0 + (srcPitch >> 1);
    uint16_t *src2 = src1 + (srcPitch >> 1);

    internal_scale2x_16_mmx(dst0, dst1, src0, src0, src1, width);

    int count = height;

    count -= 2;
    while(count) {
      dst0 += dstPitch;
      dst1 += dstPitch;
      internal_scale2x_16_mmx(dst0, dst1, src0, src1, src2, width);
      src0 = src1;
      src1 = src2;
      src2 += srcPitch >> 1;
      --count;
    }
    dst0 += dstPitch;
    dst1 += dstPitch;
    internal_scale2x_16_mmx(dst0, dst1, src0, src1, src1, width);
    return;
  }
#endif
  scale2x_frame<uint16_t>(internal_scale2x_def<uint16_t>, srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void AdMame2x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  if (Enabled()) {
    scale2x_frame<uint32_t>(internal_scale2x_vec<Lanes32>, srcPtr, srcPitch, dstPtr, dstPitch, width, height);
    return;
  }
#ifdef MMX
  if(cpu_mmx) {
    uint32_t *dst0 = (uint32_t *)dstPtr;
    uint32_t *dst1 = dst0 + (dstPitch >> 2);

    uint32_t *src0 = (uint32_t *)srcPtr;
    uint32_t *src1 = src0 + (srcPitch >> 2);
    uint32_t *src2 = src1 + (srcPitch >> 2);

    internal_scale2x_32_mmx(dst0, dst1, src0, src0, src1, width);

    int count = height;

    count -= 2;
    while(count) {
      dst0 += dstPitch >> 1;
      dst1 += dstPitch >> 1;
      internal_scale2x_32_mmx(dst0, dst1, src0, src1, src2, width);
      src0 = src1;
      src1 = src2;
      src2 += srcPitch >> 2;
      --count;
    }
    dst0 += dstPitch >> 1;
    dst1 += dstPitch >> 1;
    internal_scale2x_32_mmx(dst0, dst1, src0, src1, src1, width);
    return;
  }
#endif
  scale2x_frame<uint32_t>(internal_scale2x_def<uint32_t>, srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}
