#include "../System.h"

void Simple2x16(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;

  nextLine = dstPtr + dstPitch;

  do {
    u32 *bP = (u32 *) srcPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;

    finish = (u8 *) bP + ((width+2) << 1);
    currentPixel = *bP++;

    do {
#ifdef WORDS_BIGENDIAN
      u32 color = currentPixel >> 16;
#else
      u32 color = currentPixel & 0xffff;
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
    } while ((u8 *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}

void Simple2x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *nextLine, *finish;

  nextLine = dstPtr + dstPitch;

  do {
    u32 *bP = (u32 *) srcPtr;
    u32 *dP = (u32 *) dstPtr;
    u32 *nL = (u32 *) nextLine;
    u32 currentPixel;

    finish = (u8 *) bP + ((width+1) << 2);
    currentPixel = *bP++;

    do {
      u32 color = currentPixel;

      *(dP) = color;
      *(dP+1) = color;
      *(nL) = color;
      *(nL + 1) = color;

      currentPixel = *bP++;

      dP += 2;
      nL += 2;
    } while ((u8 *) bP < finish);

    srcPtr += srcPitch;
    dstPtr += dstPitch << 1;
    nextLine += dstPitch << 1;
  }
  while (--height);
}


void Simple3x16(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height)
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



void Simple3x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height)
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

void Simple4x16(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height)
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



void Simple4x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height)
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
