// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "System.h"
#include <stdlib.h>
#include <memory.h>

#ifdef MMX
extern "C" bool cpu_mmx;
#endif

/*
 * Thanks to Kawaks' Mr. K for the code

   Incorporated into vba by Anthony Di Franco
*/

static u8 *frm1 = NULL;
static u8 *frm2 = NULL;
static u8 *frm3 = NULL;

extern int RGB_LOW_BITS_MASK;
extern u32 qRGB_COLOR_MASK[2];

static void Init()
{
  frm1 = (u8 *)calloc(322*242,4);
  // 1 frame ago
  frm2 = (u8 *)calloc(322*242,4);
  // 2 frames ago
  frm3 = (u8 *)calloc(322*242,4);
  // 3 frames ago
}

void InterframeCleanup()
{
  if(frm1)
    free(frm1);
  if(frm2)
    free(frm2);
  if(frm3)
    free(frm3);
  frm1 = frm2 = frm3 = NULL;
}

#ifdef MMX
static void SmartIB_MMX(u8 *srcPtr, u32 srcPitch, int width, int height)
{
  u16 *src0 = (u16 *)srcPtr;
  u16 *src1 = (u16 *)frm1;
  u16 *src2 = (u16 *)frm2;
  u16 *src3 = (u16 *)frm3;
  
  int count = width >> 2;
    
  for(int i = 0; i < height; i++) {
#ifdef __GNUC__
    asm volatile (
                  "push %4\n"
                  "movq 0(%5), %%mm7\n" // colorMask
                  "0:\n"
                  "movq 0(%0), %%mm0\n" // src0
                  "movq 0(%1), %%mm1\n" // src1
                  "movq 0(%2), %%mm2\n" // src2
                  "movq 0(%3), %%mm3\n" // src3
                  "movq %%mm0, 0(%3)\n" // src3 = src0
                  "movq %%mm0, %%mm4\n"
                  "movq %%mm1, %%mm5\n"
                  "pcmpeqw %%mm2, %%mm5\n" // src1 == src2 (A)
                  "pcmpeqw %%mm3, %%mm4\n" // src3 == src0 (B)
                  "por %%mm5, %%mm4\n" // A | B
                  "movq %%mm2, %%mm5\n"
                  "pcmpeqw %%mm0, %%mm5\n" // src0 == src2 (C)
                  "pcmpeqw %%mm1, %%mm3\n" // src1 == src3 (D)
                  "por %%mm3, %%mm5\n" // C|D
                  "pandn %%mm5, %%mm4\n" // (!(A|B))&(C|D)
                  "movq %%mm0, %%mm2\n"
                  "pand %%mm7, %%mm2\n" // color & colorMask
                  "pand %%mm7, %%mm1\n" // src1 & colorMask
                  "psrlw $1, %%mm2\n" // (color & colorMask) >> 1 (E)
                  "psrlw $1, %%mm1\n" // (src & colorMask) >> 1 (F)
                  "paddw %%mm2, %%mm1\n" // E+F
                  "pand %%mm4, %%mm1\n" // (E+F) & res
                  "pandn %%mm0, %%mm4\n" // color& !res
                  
                  "por %%mm1, %%mm4\n"
                  "movq %%mm4, 0(%0)\n" // src0 = res
                  
                  "addl $8, %0\n"
                  "addl $8, %1\n"
                  "addl $8, %2\n"
                  "addl $8, %3\n"
                  
                  "decl %4\n"
                  "jnz 0b\n"
                  "pop %4\n"
                  "emms\n"
                  : "+r" (src0), "+r" (src1), "+r" (src2), "+r" (src3)
                  : "r" (count), "r" (qRGB_COLOR_MASK)
                  );
#else
    __asm {
      movq mm7, qword ptr [qRGB_COLOR_MASK];
      mov eax, src0;
      mov ebx, src1;
      mov ecx, src2;
      mov edx, src3;
      mov edi, count;
    label0:
      movq mm0, qword ptr [eax]; // src0
      movq mm1, qword ptr [ebx]; // src1
      movq mm2, qword ptr [ecx]; // src2
      movq mm3, qword ptr [edx]; // src3
      movq qword ptr [edx], mm0; // src3 = src0           
      movq mm4, mm0;
      movq mm5, mm1;
      pcmpeqw mm5, mm2; // src1 == src2 (A)
      pcmpeqw mm4, mm3; // src3 == src0 (B)
      por mm4, mm5; // A | B
      movq mm5, mm2;
      pcmpeqw mm5, mm0; // src0 == src2 (C)
      pcmpeqw mm3, mm1; // src1 == src3 (D)
      por mm5, mm3; // C|D
      pandn mm4, mm5; // (!(A|B))&(C|D)
      movq mm2, mm0;
      pand mm2, mm7; // color & colorMask
      pand mm1, mm7; // src1 & colorMask
      psrlw mm2, 1; // (color & colorMask) >> 1 (E)
      psrlw mm1, 1; // (src & colorMask) >> 1 (F)
      paddw mm1, mm2; // E+F
      pand mm1, mm4; // (E+F) & res
      pandn mm4, mm0; // color & !res
      
      por mm4, mm1;
      movq qword ptr [eax], mm4; // src0 = res
                  
      add eax, 8;
      add ebx, 8;
      add ecx, 8;
      add edx, 8;

      dec edi;
      jnz label0;
      mov src0, eax;
      mov src1, ebx;
      mov src2, ecx;
      mov src3, edx;
      emms;
    }
#endif
    src0+=2;
    src1+=2;
    src2+=2;
    src3+=2;
  }
                
  /* Swap buffers around */
  u8 *temp = frm1;
  frm1 = frm3;
  frm3 = frm2;
  frm2 = temp;  
}
#endif

void SmartIB(u8 *srcPtr, u32 srcPitch, int width, int height)
{
  if(frm1 == NULL) {
    Init();
  }
#ifdef MMX
  if(cpu_mmx) {
    SmartIB_MMX(srcPtr, srcPitch, width, height);
    return;
  }
#endif

  u16 colorMask = ~RGB_LOW_BITS_MASK;
  
  u16 *src0 = (u16 *)srcPtr;
  u16 *src1 = (u16 *)frm1;
  u16 *src2 = (u16 *)frm2;
  u16 *src3 = (u16 *)frm3;

  int sPitch = srcPitch >> 1;

  int pos = 0;
  for (int j = 0; j < height;  j++)
    for (int i = 0; i < sPitch; i++) {
      u16 color = src0[pos];
      src0[pos] =
        (src1[pos] != src2[pos]) &&
        (src3[pos] != color) &&
        ((color == src2[pos]) || (src1[pos] == src3[pos]))
        ? (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1)) :
        color;
      src3[pos] = color; /* oldest buffer now holds newest frame */
      pos++;
    }
  
  /* Swap buffers around */
  u8 *temp = frm1;
  frm1 = frm3;
  frm3 = frm2;
  frm2 = temp;
}

#ifdef MMX
static void SmartIB32_MMX(u8 *srcPtr, u32 srcPitch, int width, int height)
{
  u32 *src0 = (u32 *)srcPtr;
  u32 *src1 = (u32 *)frm1;
  u32 *src2 = (u32 *)frm2;
  u32 *src3 = (u32 *)frm3;

  int count = width >> 1;
  
  for(int i = 0; i < height; i++) {
#ifdef __GNUC__    
    asm volatile (
                  "push %4\n"
                  "movq 0(%5), %%mm7\n" // colorMask
                  "0:\n"
                  "movq 0(%0), %%mm0\n" // src0
                  "movq 0(%1), %%mm1\n" // src1
                  "movq 0(%2), %%mm2\n" // src2
                  "movq 0(%3), %%mm3\n" // src3
                  "movq %%mm0, 0(%3)\n" // src3 = src0
                  "movq %%mm0, %%mm4\n"
                  "movq %%mm1, %%mm5\n"
                  "pcmpeqd %%mm2, %%mm5\n" // src1 == src2 (A)
                  "pcmpeqd %%mm3, %%mm4\n" // src3 == src0 (B)
                  "por %%mm5, %%mm4\n" // A | B
                  "movq %%mm2, %%mm5\n"
                  "pcmpeqd %%mm0, %%mm5\n" // src0 == src2 (C)
                  "pcmpeqd %%mm1, %%mm3\n" // src1 == src3 (D)
                  "por %%mm3, %%mm5\n" // C|D
                  "pandn %%mm5, %%mm4\n" // (!(A|B))&(C|D)
                  "movq %%mm0, %%mm2\n"
                  "pand %%mm7, %%mm2\n" // color & colorMask
                  "pand %%mm7, %%mm1\n" // src1 & colorMask
                  "psrld $1, %%mm2\n" // (color & colorMask) >> 1 (E)
                  "psrld $1, %%mm1\n" // (src & colorMask) >> 1 (F)
                  "paddd %%mm2, %%mm1\n" // E+F
                  "pand %%mm4, %%mm1\n" // (E+F) & res
                  "pandn %%mm0, %%mm4\n" // color& !res
                  
                  "por %%mm1, %%mm4\n"
                  "movq %%mm4, 0(%0)\n" // src0 = res
                  
                  "addl $8, %0\n"
                  "addl $8, %1\n"
                  "addl $8, %2\n"
                  "addl $8, %3\n"
                  
                  "decl %4\n"
                  "jnz 0b\n"
                  "pop %4\n"
                  "emms\n"
                  : "+r" (src0), "+r" (src1), "+r" (src2), "+r" (src3)
                  : "r" (count), "r" (qRGB_COLOR_MASK)
                  );
#else
    __asm {
      movq mm7, qword ptr [qRGB_COLOR_MASK];
      mov eax, src0;
      mov ebx, src1;
      mov ecx, src2;
      mov edx, src3;
      mov edi, count;
    label0:
      movq mm0, qword ptr [eax]; // src0
      movq mm1, qword ptr [ebx]; // src1
      movq mm2, qword ptr [ecx]; // src2
      movq mm3, qword ptr [edx]; // src3
      movq qword ptr [edx], mm0; // src3 = src0           
      movq mm4, mm0;
      movq mm5, mm1;
      pcmpeqd mm5, mm2; // src1 == src2 (A)
      pcmpeqd mm4, mm3; // src3 == src0 (B)
      por mm4, mm5; // A | B
      movq mm5, mm2;
      pcmpeqd mm5, mm0; // src0 == src2 (C)
      pcmpeqd mm3, mm1; // src1 == src3 (D)
      por mm5, mm3; // C|D
      pandn mm4, mm5; // (!(A|B))&(C|D)
      movq mm2, mm0;
      pand mm2, mm7; // color & colorMask
      pand mm1, mm7; // src1 & colorMask
      psrld mm2, 1; // (color & colorMask) >> 1 (E)
      psrld mm1, 1; // (src & colorMask) >> 1 (F)
      paddd mm1, mm2; // E+F
      pand mm1, mm4; // (E+F) & res
      pandn mm4, mm0; // color & !res
      
      por mm4, mm1;
      movq qword ptr [eax], mm4; // src0 = res
                  
      add eax, 8;
      add ebx, 8;
      add ecx, 8;
      add edx, 8;

      dec edi;
      jnz label0;
      mov src0, eax;
      mov src1, ebx;
      mov src2, ecx;
      mov src3, edx;
      emms;
    }
#endif
    
    src0++;
    src1++;
    src2++;
    src3++;
  }
  /* Swap buffers around */
  u8 *temp = frm1;
  frm1 = frm3;
  frm3 = frm2;
  frm2 = temp;  
}
#endif

void SmartIB32(u8 *srcPtr, u32 srcPitch, int width, int height)
{
  if(frm1 == NULL) {
    Init();
  }
#ifdef MMX
  if(cpu_mmx) {
    SmartIB32_MMX(srcPtr, srcPitch, width, height);
    return;
  }
#endif
  
  u32 *src0 = (u32 *)srcPtr;
  u32 *src1 = (u32 *)frm1;
  u32 *src2 = (u32 *)frm2;
  u32 *src3 = (u32 *)frm3;

  u32 colorMask = 0xfefefe;

  int sPitch = srcPitch >> 2;
  int pos = 0;

  for (int j = 0; j < height;  j++)
    for (int i = 0; i < sPitch; i++) {
      u32 color = src0[pos];
      src0[pos] =
        (src1[pos] != src2[pos]) &&
        (src3[pos] != color) &&
        ((color == src2[pos]) || (src1[pos] == src3[pos]))
        ? (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1)) :
        color;
      src3[pos] = color; /* oldest buffer now holds newest frame */
      pos++;
    }
  
  /* Swap buffers around */
  u8 *temp = frm1;
  frm1 = frm3;
  frm3 = frm2;
  frm2 = temp;
}

#ifdef MMX
static void MotionBlurIB_MMX(u8 *srcPtr, u32 srcPitch, int width, int height)
{
  u16 *src0 = (u16 *)srcPtr;
  u16 *src1 = (u16 *)frm1;

  int count = width >> 2;
  
  for(int i = 0; i < height; i++) {
#ifdef __GNUC__    
    asm volatile (
                  "push %2\n"
                  "movq 0(%3), %%mm7\n" // colorMask
                  "0:\n"
                  "movq 0(%0), %%mm0\n" // src0
                  "movq 0(%1), %%mm1\n" // src1
                  "movq %%mm0, 0(%1)\n" // src1 = src0
                  "pand %%mm7, %%mm0\n" // color & colorMask
                  "pand %%mm7, %%mm1\n" // src1 & colorMask
                  "psrlw $1, %%mm0\n" // (color & colorMask) >> 1 (E)
                  "psrlw $1, %%mm1\n" // (src & colorMask) >> 1 (F)
                  "paddw %%mm1, %%mm0\n" // E+F

                  "movq %%mm0, 0(%0)\n" // src0 = res
                  
                  "addl $8, %0\n"
                  "addl $8, %1\n"
                  
                  "decl %2\n"
                  "jnz 0b\n"
                  "pop %2\n"
                  "emms\n"
                  : "+r" (src0), "+r" (src1)
                  : "r" (count), "r" (qRGB_COLOR_MASK)
                  );
#else
    __asm {
      movq mm7, qword ptr [qRGB_COLOR_MASK];
      mov eax, src0;
      mov ebx, src1;
      mov edi, count;
    label0:
      movq mm0, qword ptr [eax]; // src0
      movq mm1, qword ptr [ebx]; // src1
      movq qword ptr [ebx], mm0; // src1 = src0
      pand mm0, mm7; // color & colorMask
      pand mm1, mm7; // src1 & colorMask
      psrlw mm0, 1; // (color & colorMask) >> 1 (E)
      psrlw mm1, 1; // (src & colorMask) >> 1 (F)
      paddw mm0, mm1; // E+F

      movq qword ptr [eax], mm0; // src0 = res
                  
      add eax, 8;
      add ebx, 8;

      dec edi;
      jnz label0;
      mov src0, eax;
      mov src1, ebx;
      emms;
    }
#endif
    src0+=2;
    src1+=2;
  }
}
#endif

void MotionBlurIB(u8 *srcPtr, u32 srcPitch, int width, int height)
{
  if(frm1 == NULL) {
    Init();
  }

#ifdef MMX
  if(cpu_mmx) {
    MotionBlurIB_MMX(srcPtr, srcPitch, width, height);
    return;
  }
#endif
  
  u16 colorMask = ~RGB_LOW_BITS_MASK;
  
  u16 *src0 = (u16 *)srcPtr;
  u16 *src1 = (u16 *)frm1;

  int sPitch = srcPitch >> 1;

  int pos = 0;
  for (int j = 0; j < height;  j++)
    for (int i = 0; i < sPitch; i++) {
      u16 color = src0[pos];
      src0[pos] =
        (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1));
      src1[pos] = color;
      pos++;
    }
}

#ifdef MMX
static void MotionBlurIB32_MMX(u8 *srcPtr, u32 srcPitch, int width, int height)
{
  u32 *src0 = (u32 *)srcPtr;
  u32 *src1 = (u32 *)frm1;

  int count = width >> 1;  

  for(int i = 0; i < height; i++) {
#ifdef __GNUC__    
    asm volatile (
                  "push %2\n"
                  "movq 0(%3), %%mm7\n" // colorMask
                  "0:\n"
                  "movq 0(%0), %%mm0\n" // src0
                  "movq 0(%1), %%mm1\n" // src1
                  "movq %%mm0, 0(%1)\n" // src1 = src0
                  "pand %%mm7, %%mm0\n" // color & colorMask
                  "pand %%mm7, %%mm1\n" // src1 & colorMask
                  "psrld $1, %%mm0\n" // (color & colorMask) >> 1 (E)
                  "psrld $1, %%mm1\n" // (src & colorMask) >> 1 (F)
                  "paddd %%mm1, %%mm0\n" // E+F

                  "movq %%mm0, 0(%0)\n" // src0 = res
                  
                  "addl $8, %0\n"
                  "addl $8, %1\n"
                  
                  "decl %2\n"
                  "jnz 0b\n"
                  "pop %2\n"
                  "emms\n"
                  : "+r" (src0), "+r" (src1)
                  : "r" (count), "r" (qRGB_COLOR_MASK)
                  );
#else
    __asm {
      movq mm7, qword ptr [qRGB_COLOR_MASK];
      mov eax, src0;
      mov ebx, src1;
      mov edi, count;
    label0:
      movq mm0, qword ptr [eax]; // src0
      movq mm1, qword ptr [ebx]; // src1
      movq qword ptr [ebx], mm0; // src1 = src0
      pand mm0, mm7; // color & colorMask
      pand mm1, mm7; // src1 & colorMask
      psrld mm0, 1; // (color & colorMask) >> 1 (E)
      psrld mm1, 1; // (src & colorMask) >> 1 (F)
      paddd mm0, mm1; // E+F

      movq qword ptr [eax], mm0; // src0 = res
                  
      add eax, 8;
      add ebx, 8;

      dec edi;
      jnz label0;
      mov src0, eax;
      mov src1, ebx;
      emms;
    }
#endif
    src0++;
    src1++;
  }
}
#endif

void MotionBlurIB32(u8 *srcPtr, u32 srcPitch, int width, int height)
{
  if(frm1 == NULL) {
    Init();
  }

#ifdef MMX
  if(cpu_mmx) {
    MotionBlurIB32_MMX(srcPtr, srcPitch, width, height);
    return;
  }
#endif
  
  u32 *src0 = (u32 *)srcPtr;
  u32 *src1 = (u32 *)frm1;

  u32 colorMask = 0xfefefe;

  int sPitch = srcPitch >> 2;
  int pos = 0;

  for (int j = 0; j < height;  j++)
    for (int i = 0; i < sPitch; i++) {
      u32 color = src0[pos];
      src0[pos] = (((color & colorMask) >> 1) +
                   ((src1[pos] & colorMask) >> 1));
      src1[pos] = color;
      pos++;
    }
}
