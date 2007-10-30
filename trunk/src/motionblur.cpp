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

extern int RGB_LOW_BITS_MASK;

void MotionBlur(u8 *srcPtr, u32 srcPitch, u8 *deltaPtr,
                u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;
  u32 colorMask = ~(RGB_LOW_BITS_MASK | (RGB_LOW_BITS_MASK << 16));
  u32 lowPixelMask = RGB_LOW_BITS_MASK;
  
  nextLine = dstPtr + dstPitch;
  
  do {
    u32 *bP = (u32 *) srcPtr;
    u32 *xP = (u32 *) deltaPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;
    u32 nextPixel;
    u32 currentDelta;
    u32 nextDelta;
    
    finish = (u8 *) bP + ((width+2) << 1);
    nextPixel = *bP++;
    nextDelta = *xP++;
    
    do {
      currentPixel = nextPixel;
      currentDelta = nextDelta;
      nextPixel = *bP++;
      nextDelta = *xP++;

      if(currentPixel != currentDelta) {
        u32 colorA, product, colorB;
        
        *(xP - 2) = currentPixel;
#ifdef WORDS_BIGENDIAN
        colorA = currentPixel >> 16;
        colorB = currentDelta >> 16;
#else
        colorA = currentPixel & 0xffff;
        colorB = currentDelta & 0xffff;
#endif

        product =   ((((colorA & colorMask) >> 1) +
                          ((colorB & colorMask) >> 1) +
                          (colorA & colorB & lowPixelMask)));
        
        *(dP) = product | product << 16;
        *(nL) = product | product << 16;

#ifdef WORDS_BIGENDIAN
        colorA = (currentPixel & 0xffff);
        colorB = (currentDelta & 0xffff);
#else
        colorA = currentPixel >> 16;
        colorB = currentDelta >> 16;
#endif
        product = ((((colorA & colorMask) >> 1) +
                  ((colorB & colorMask) >> 1) +
                    (colorA & colorB & lowPixelMask)));
        
        *(dP + 1) = product | product << 16;
        *(nL + 1) = product | product << 16;
      } else {
        u32 colorA, product;
        
        *(xP - 2) = currentPixel;
#ifdef WORDS_BIGENDIAN
        colorA = currentPixel >> 16;
#else
        colorA = currentPixel & 0xffff;
#endif
        
        product = colorA;
        
        *(dP) = product | product << 16;
        *(nL) = product | product << 16;
#ifdef WORDS_BIGENDIAN
        colorA = (currentPixel & 0xffff);
#else
        colorA = currentPixel >> 16;
#endif
        product = colorA;
        
        *(dP + 1) = product | product << 16;
        *(nL + 1) = product | product << 16;        
      }
      
      dP += 2;
      nL += 2;
    } while ((u8 *) bP < finish);
    
    deltaPtr += srcPitch;
    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}

void MotionBlur32(u8 *srcPtr, u32 srcPitch, u8 *deltaPtr,
                  u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;
  u32 colorMask = ~RGB_LOW_BITS_MASK;
  u32 lowPixelMask = RGB_LOW_BITS_MASK;
  
  nextLine = dstPtr + dstPitch;
  
  do {
    u32 *bP = (u32 *) srcPtr;
    u32 *xP = (u32 *) deltaPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;
    u32 nextPixel;
    u32 currentDelta;
    u32 nextDelta;
    
    finish = (u8 *) bP + ((width+1) << 2);
    nextPixel = *bP++;
    nextDelta = *xP++;
    
    do {
      currentPixel = nextPixel;
      currentDelta = nextDelta;
      nextPixel = *bP++;
      nextDelta = *xP++;

      u32 colorA, product, colorB;

      *(xP - 2) = currentPixel;
      colorA = currentPixel;
      colorB = currentDelta;
      
      product =   ((((colorA & colorMask) >> 1) +
                    ((colorB & colorMask) >> 1) +
                    (colorA & colorB & lowPixelMask)));
      
      *(dP) = product;
      *(dP+1) = product;
      *(nL) = product;
      *(nL+1) = product;

      *(xP - 1) = nextPixel;

      colorA = nextPixel;
      colorB = nextDelta;
      
      product = ((((colorA & colorMask) >> 1) +
                  ((colorB & colorMask) >> 1) +
                  (colorA & colorB & lowPixelMask)));
      
      *(dP + 2) = product;
      *(dP + 3) = product;
      *(nL + 2) = product;
      *(nL + 3) = product;

      nextPixel = *bP++;
      nextDelta = *xP++;
      
      dP += 4;
      nL += 4;
    } while ((u8 *) bP < finish);
    
    deltaPtr += srcPitch;
    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}
