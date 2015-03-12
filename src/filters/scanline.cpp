#include "../System.h"

extern int RGB_LOW_BITS_MASK;

void Scanlines32(u8 *srcPtr, u32 srcPitch,
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

void ScanlinesTV32(u8 *srcPtr, u32 srcPitch,
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
