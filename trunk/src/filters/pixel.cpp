#include "../System.h"

extern int RGB_LOW_BITS_MASK;

void Pixelate(u8 *srcPtr, u32 srcPitch, u8 *deltaPtr,
          u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;
  u32 colorMask = ~(RGB_LOW_BITS_MASK | (RGB_LOW_BITS_MASK << 16));

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

      if ((nextPixel != nextDelta) || (currentPixel != currentDelta)) {
        u32 colorA, colorB, product;

        *(xP - 2) = currentPixel;
#ifdef WORDS_BIGENDIAN
        colorA = currentPixel >> 16;
        colorB = currentPixel & 0xffff;
#else
        colorA = currentPixel & 0xffff;
        colorB = currentPixel >> 16;
#endif
        product = (((colorA & colorMask) >> 1) & colorMask) >> 1;

#ifdef WORDS_BIGENDIAN
        *(nL) = (product << 16) | (product);
        *(dP) = (colorA << 16) | product;
#else
        *(nL) = product | (product << 16);
        *(dP) = colorA | (product << 16);
#endif

#ifdef WORDS_BIGENDIAN
        colorA = nextPixel >> 16;
#else
        colorA = nextPixel & 0xffff;
#endif
        product = (((colorB & colorMask) >> 1) & colorMask) >> 1;
#ifdef WORDS_BIGENDIAN
        *(nL + 1) = (product << 16) | (product);
        *(dP + 1) = (colorB << 16) | (product);
#else
        *(nL + 1) = (product) | (product << 16);
        *(dP + 1) = (colorB) | (product << 16);
#endif
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

void Pixelate32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;
  u32 colorMask = ~RGB_LOW_BITS_MASK;

  nextLine = dstPtr + dstPitch;

  do {
    u32 *bP = (u32 *) srcPtr;
    //    u32 *xP = (u32 *) deltaPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;
    u32 nextPixel;

    finish = (u8 *) bP + ((width+1) << 2);
    nextPixel = *bP++;

    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;

      u32 colorA, colorB, product;

      colorA = currentPixel;
      colorB = nextPixel;

      product = (((colorA & colorMask) >> 1) & colorMask) >> 1;
      *(nL) = product;
      *(nL+1) = product;
      *(dP) = colorA;
      *(dP+1) = product;

      nextPixel = *bP++;
      colorA = nextPixel;
      product = (((colorB & colorMask) >> 1) & colorMask) >> 1;
      *(nL + 2) = product;
      *(nL + 3) = product;
      *(dP + 2) = colorB;
      *(dP + 3) = product;

      dP += 4;
      nL += 4;
    } while ((u8 *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}
