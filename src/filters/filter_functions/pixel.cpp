
#include <cstdint>
typedef uint8_t u8;
typedef uint32_t u32;

extern int RGB_LOW_BITS_MASK;

void Pixelate32(u8 *srcPtr, u32 srcPitch,
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
