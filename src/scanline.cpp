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

void Scanlines (u8 *srcPtr, u32 srcPitch, u8 *,
                u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;
  
  nextLine = dstPtr + dstPitch;
  
  do {
    u32 *bP = (u32 *) srcPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;
    u32 nextPixel;
    
    finish = (u8 *) bP + ((width+2) << 1);
    nextPixel = *bP++;
    
    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;
      u32 colorA, colorB;
      
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
    } while ((u8 *) bP < finish);
    
    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}

void Scanlines32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                 u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;
  
  nextLine = dstPtr + dstPitch;
  
  do {
    u32 *bP = (u32 *) srcPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;
    u32 nextPixel;
    
    finish = (u8 *) bP + ((width+1) << 2);
    nextPixel = *bP++;
    
    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;
      
      u32 colorA, colorB;
        
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
    } while ((u8 *) bP < finish);
    
    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}

void ScanlinesTV(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                 u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;
  u32 colorMask = ~(RGB_LOW_BITS_MASK | (RGB_LOW_BITS_MASK << 16));
  
  nextLine = dstPtr + dstPitch;
  
  do {
    u32 *bP = (u32 *) srcPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;
    u32 nextPixel;
    
    finish = (u8 *) bP + ((width+2) << 1);
    nextPixel = *bP++;
    
    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;
      
      u32 colorA, colorB;

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
    } while ((u8 *) bP < finish);
    
    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}

void ScanlinesTV32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                   u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;
  u32 colorMask = ~RGB_LOW_BITS_MASK;
  
  nextLine = dstPtr + dstPitch;
  
  do {
    u32 *bP = (u32 *) srcPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;
    u32 nextPixel;
    
    finish = (u8 *) bP + ((width+1) << 2);
    nextPixel = *bP++;
    
    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;
      
      u32 colorA, colorB, temp;

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
    } while ((u8 *) bP < finish);
    
    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}
