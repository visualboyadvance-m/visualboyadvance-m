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

#include "../System.h"

#ifdef MMX
extern "C" bool cpu_mmx;
#endif

static void internal_scale2x_16_def(uint16_t *dst, const uint16_t* src0, const uint16_t* src1, const uint16_t* src2, unsigned count) {
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

static void internal_scale2x_32_def(uint32_t* dst,
                                    const uint32_t* src0,
                                    const uint32_t* src1,
                                    const uint32_t* src2,
                                    unsigned count)
{
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
  uint16_t *dst0 = (uint16_t *)dstPtr;
  uint16_t *dst1 = dst0 + (dstPitch >> 1);

  uint16_t *src0 = (uint16_t *)srcPtr;
  uint16_t *src1 = src0 + (srcPitch >> 1);
  uint16_t *src2 = src1 + (srcPitch >> 1);
#ifdef MMX
  if(cpu_mmx) {
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
  } else {
#endif
    internal_scale2x_16_def(dst0, src0, src0, src1, width);
    internal_scale2x_16_def(dst1, src1, src0, src0, width);

    int count = height;

    count -= 2;
    while(count) {
      dst0 += dstPitch;
      dst1 += dstPitch;
      internal_scale2x_16_def(dst0, src0, src1, src2, width);
      internal_scale2x_16_def(dst1, src2, src1, src0, width);
      src0 = src1;
      src1 = src2;
      src2 += srcPitch >> 1;
      --count;
    }
    dst0 += dstPitch;
    dst1 += dstPitch;
    internal_scale2x_16_def(dst0, src0, src1, src1, width);
    internal_scale2x_16_def(dst1, src1, src1, src0, width);
#ifdef MMX
  }
#endif
}

void AdMame2x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint32_t *dst0 = (uint32_t *)dstPtr;
  uint32_t *dst1 = dst0 + (dstPitch >> 2);

  uint32_t *src0 = (uint32_t *)srcPtr;
  uint32_t *src1 = src0 + (srcPitch >> 2);
  uint32_t *src2 = src1 + (srcPitch >> 2);
#ifdef MMX
  if(cpu_mmx) {
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
  } else {
#endif
    internal_scale2x_32_def(dst0, src0, src0, src1, width);
    internal_scale2x_32_def(dst1, src1, src0, src0, width);

    int count = height;

    count -= 2;
    while(count) {
      dst0 += dstPitch >> 1;
      dst1 += dstPitch >> 1;
      internal_scale2x_32_def(dst0, src0, src1, src2, width);
      internal_scale2x_32_def(dst1, src2, src1, src0, width);
      src0 = src1;
      src1 = src2;
      src2 += srcPitch >> 2;
      --count;
    }
    dst0 += dstPitch >> 1;
    dst1 += dstPitch >> 1;
    internal_scale2x_32_def(dst0, src0, src1, src1, width);
    internal_scale2x_32_def(dst1, src1, src1, src0, width);
#ifdef MMX
  }
#endif
}
