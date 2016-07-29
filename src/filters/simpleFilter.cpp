#include "../System.h"

void Simple2x16(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
              uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t *nextLine, *finish;

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;

    finish = (uint8_t *) bP + ((width+2) << 1);
    currentPixel = *bP++;

    do {
#ifdef WORDS_BIGENDIAN
      uint32_t color = currentPixel >> 16;
#else
      uint32_t color = currentPixel & 0xffff;
#endif

      color = color | (color << 16);

      *(dP) = color;
      *(nL) = color;

#ifdef WORDS_BIGENDIAN
      color = currentPixel & 0xffff;
#else
      color = currentPixel >> 16;
#endif
      color = color| (color << 16);
      *(dP + 1) = color;
      *(nL + 1) = color;

      currentPixel = *bP++;

      dP += 2;
      nL += 2;
    } while ((uint8_t *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}

void Simple2x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t *nextLine, *finish;

  nextLine = dstPtr + dstPitch;

  do {
    uint32_t *bP = (uint32_t *) srcPtr;
    uint32_t *dP = (uint32_t *) dstPtr;
    uint32_t *nL = (uint32_t *) nextLine;
    uint32_t currentPixel;

    finish = (uint8_t *) bP + ((width+1) << 2);
    currentPixel = *bP++;

    do {
      uint32_t color = currentPixel;

      *(dP) = color;
      *(dP+1) = color;
      *(nL) = color;
      *(nL + 1) = color;

      currentPixel = *bP++;

      dP += 2;
      nL += 2;
    } while ((uint8_t *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}


void Simple3x16(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
              uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#define magnification	3
#define colorBytes 2 // 16 bit colors = 2 byte colors

	// Generic Simple magnification filter
	int x, y; // Source Position Counter
	unsigned int dx, dy; // Destination pixel's pixels
	unsigned short col; // Source color

	srcPitch = (srcPitch / colorBytes) - width; // This is the part of the source pitch in pixels that is more than the source image
	dstPitch = dstPitch / colorBytes;

	unsigned short *src, *dst, *dst2;
	src = (unsigned short *)srcPtr; // Since everything is time-critical this should be better than converting the pointers x*y times
	dst = (unsigned short *)dstPtr;

	for (y = 0; y < height; y++) // Line
	{
		for (x = 0; x < width; x++) // Pixel in Line
		{
			col = *src;

      dst2 = dst;
			*dst2 = col;
			for (dy = 0; dy < magnification; dy++)
			{
				for (dx = 0; dx < magnification; dx++)
				{
					*dst2 = col;
					dst2++;
				}
				dst2+=dstPitch;
				dst2-=magnification;
			}

			src++;
			dst+=magnification;
		}
		src+=srcPitch;
		dst+=dstPitch * magnification;
		dst-=width * magnification;
	}
#undef magnification
#undef colorBytes
}



void Simple3x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#define magnification	3
#define colorBytes 4 // 32 bit colors = 4 byte colors

	// Generic Simple magnification filter
	int x, y; // Source Position Counter
	unsigned int dx, dy; // Destination pixel's pixels
	unsigned int col; // Source color

	srcPitch = (srcPitch / colorBytes) - width; // This is the part of the source pitch in pixels that is more than the source image
	dstPitch = dstPitch / colorBytes;

	unsigned int *src, *dst, *dst2;
	src = (unsigned int *)srcPtr; // Since everything is time-critical this should be better than converting the pointers x*y times
	dst = (unsigned int *)dstPtr;

	for (y = 0; y < height; y++) // Line
	{
		for (x = 0; x < width; x++) // Pixel in Line
		{
			col = *src;

      dst2 = dst;
			*dst2 = col;
			for (dy = 0; dy < magnification; dy++)
			{
				for (dx = 0; dx < magnification; dx++)
				{
					*dst2 = col;
					dst2++;
				}
				dst2+=dstPitch;
				dst2-=magnification;
			}

			src++;
			dst+=magnification;
		}
		src+=srcPitch;
		dst+=dstPitch * magnification;
		dst-=width * magnification;
	}
#undef magnification
#undef colorBytes
}

void Simple4x16(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
              uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#define magnification	4
#define colorBytes 2 // 16 bit colors = 2 byte colors

	// Generic Simple magnification filter
	int x, y; // Source Position Counter
	unsigned int dx, dy; // Destination pixel's pixels
	unsigned short col; // Source color

	srcPitch = (srcPitch / colorBytes) - width; // This is the part of the source pitch in pixels that is more than the source image
	dstPitch = dstPitch / colorBytes;

	unsigned short *src, *dst, *dst2;
	src = (unsigned short *)srcPtr; // Since everything is time-critical this should be better than converting the pointers x*y times
	dst = (unsigned short *)dstPtr;

	for (y = 0; y < height; y++) // Line
	{
		for (x = 0; x < width; x++) // Pixel in Line
		{
			col = *src;

      dst2 = dst;
			*dst2 = col;
			for (dy = 0; dy < magnification; dy++)
			{
				for (dx = 0; dx < magnification; dx++)
				{
					*dst2 = col;
					dst2++;
				}
				dst2+=dstPitch;
				dst2-=magnification;
			}

			src++;
			dst+=magnification;
		}
		src+=srcPitch;
		dst+=dstPitch * magnification;
		dst-=width * magnification;
	}
#undef magnification
#undef colorBytes
}



void Simple4x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
#define magnification	4
#define colorBytes 4 // 32 bit colors = 4 byte colors

	// Generic Simple magnification filter
	int x, y; // Source Position Counter
	unsigned int dx, dy; // Destination pixel's pixels
	unsigned int col; // Source color

	srcPitch = (srcPitch / colorBytes) - width; // This is the part of the source pitch in pixels that is more than the source image
	dstPitch = dstPitch / colorBytes;

	unsigned int *src, *dst, *dst2;
	src = (unsigned int *)srcPtr; // Since everything is time-critical this should be better than converting the pointers x*y times
	dst = (unsigned int *)dstPtr;

	for (y = 0; y < height; y++) // Line
	{
		for (x = 0; x < width; x++) // Pixel in Line
		{
			col = *src;

      dst2 = dst;
			*dst2 = col;
			for (dy = 0; dy < magnification; dy++)
			{
				for (dx = 0; dx < magnification; dx++)
				{
					*dst2 = col;
					dst2++;
				}
				dst2+=dstPitch;
				dst2-=magnification;
			}

			src++;
			dst+=magnification;
		}
		src+=srcPitch;
		dst+=dstPitch * magnification;
		dst-=width * magnification;
	}
#undef magnification
#undef colorBytes
}
