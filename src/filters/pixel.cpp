#include "../System.h"

extern int RGB_LOW_BITS_MASK;

void Pixelate(uint8_t *srcPtr, uint32_t srcPitch, uint8_t *deltaPtr,
          uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t *nextLine, *finish;
  uint32_t colorMask = ~(RGB_LOW_BITS_MASK | (RGB_LOW_BITS_MASK << 16));

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *xP = (uint32_t *) deltaPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;
    uint32_t nextPixel;
    uint32_t currentDelta;
    uint32_t nextDelta;

    finish = (uint8_t *) bP + ((width+2) << 1);
    nextPixel = *bP++;
    nextDelta = *xP++;

    do {
      currentPixel = nextPixel;
      currentDelta = nextDelta;
      nextPixel = *bP++;
      nextDelta = *xP++;

      if ((nextPixel != nextDelta) || (currentPixel != currentDelta)) {
        uint32_t colorA, colorB, product;

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
    } while ((uint8_t *) bP < finish);

    deltaPtr += srcPitch;
    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}

void Pixelate32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t *nextLine, *finish;
  uint32_t colorMask = ~RGB_LOW_BITS_MASK;

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    //    uint32_t *xP = (uint32_t *) deltaPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;
    uint32_t nextPixel;

    finish = (uint8_t *) bP + ((width+1) << 2);
    nextPixel = *bP++;

    do {
      currentPixel = nextPixel;
      nextPixel = *bP++;

      uint32_t colorA, colorB, product;

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
    } while ((uint8_t *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}
