/// All filters
#ifndef FILTERS_FILTERS_HPP
#define FILTERS_FILTERS_HPP

#include "interframe.hpp"

// most 16-bit filters require space in src rounded up to u32
// those that take delta take 1 src line of pixels, rounded up to u32 size
// initial value appears to be all-0xff

//pixel.cpp
void Pixelate(u8 *srcPtr, u32 srcPitch, u8 *deltaPtr, u8 *dstPtr, u32 dstPitch, int width, int height);
void Pixelate32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */, u8 *dstPtr, u32 dstPitch, int width, int height);

//scanline.cpp
void Scanlines (u8 *srcPtr, u32 srcPitch, u8 *, u8 *dstPtr, u32 dstPitch, int width, int height);
void Scanlines32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */, u8 *dstPtr, u32 dstPitch, int width, int height);
// "TV" here means each pixel is faded horizontally & vertically rather than
// inserting black scanlines
// this depends on RGB_LOW_BITS_MASK (included from interframe.hpp
//extern int RGB_LOW_BITS_MASK;
void ScanlinesTV(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */, u8 *dstPtr, u32 dstPitch, int width, int height);
void ScanlinesTV32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */, u8 *dstPtr, u32 dstPitch, int width, int height);

//simpleFilter.cpp
// the simple ones could greatly benefit from correct usage of preprocessor..
// in any case, they are worthless, since all renderers do "simple" or
// better by default
int Init_2xSaI(u32 BitFormat);
void Simple2x16(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height);
void Simple2x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height);
void Simple3x16(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height);
void Simple3x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height);
void Simple4x16(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height);
void Simple4x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height);

//bilinear.cpp
//These convert to rgb24 in internal buffers first, and then back again
/*static void fill_rgb_row_16(u16 *from, int src_width, u8 *row, int width);
static void fill_rgb_row_32(u32 *from, int src_width, u8 *row, int width);*/
void Bilinear(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height);
void Bilinear32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height);
void BilinearPlus(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                  u8 *dstPtr, u32 dstPitch, int width, int height);
void BilinearPlus32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                    u8 *dstPtr, u32 dstPitch, int width, int height);

//admame.cpp
void AdMame2x(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height);
void AdMame2x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height);

//2xSal.cpp
// next 3*2 use Init_2xSaI(555|565) and do not take into account
int Init_2xSaI(u32 BitFormat);
// endianness or bit shift variables in init.
// next 4*2 may be MMX-accelerated
void _2xSaI (u8 *srcPtr, u32 srcPitch, u8 *deltaPtr,
             u8 *dstPtr, u32 dstPitch, int width, int height);
void _2xSaI32 (u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
               u8 *dstPtr, u32 dstPitch, int width, int height);
//This one was commented out in other source files
void Scale_2xSaI (u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                  u8 *dstPtr, u32 dstPitch,
                  u32 dstWidth, u32 dstHeight, int width, int height);
void Super2xSaI (u8 *srcPtr, u32 srcPitch,
                 u8 *deltaPtr, u8 *dstPtr, u32 dstPitch,
                 int width, int height);
void Super2xSaI32 (u8 *srcPtr, u32 srcPitch,
                   u8 * /* deltaPtr */, u8 *dstPtr, u32 dstPitch,
                   int width, int height);
void SuperEagle (u8 *srcPtr, u32 srcPitch, u8 *deltaPtr,
                 u8 *dstPtr, u32 dstPitch, int width, int height);
void SuperEagle32 (u8 *srcPtr, u32 srcPitch, u8 *deltaPtr,
                   u8 *dstPtr, u32 dstPitch, int width, int height);
//hq2x.cpp
//These require calling hq2x_init first and whenever bpp changes
/*static void hq2x_16_def(u16* dst0, u16* dst1, const u16* src0, const u16* src1, const u16* src2, unsigned count);
static void hq2x_32_def(u32* dst0, u32* dst1, const u32* src0, const u32* src1, const u32* src2, unsigned count);
static void lq2x_16_def(u16* dst0, u16* dst1, const u16* src0, const u16* src1, const u16* src2, unsigned count);
static void lq2x_32_def(u32* dst0, u32* dst1, const u32* src0, const u32* src1, const u32* src2, unsigned count);*/
void hq2x_init(unsigned bits_per_pixel);
void hq2x(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
          u8 *dstPtr, u32 dstPitch, int width, int height);
void hq2x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
            u8 *dstPtr, u32 dstPitch, int width, int height);
void lq2x(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
          u8 *dstPtr, u32 dstPitch, int width, int height);
void lq2x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
            u8 *dstPtr, u32 dstPitch, int width, int height);

//hq/asm/hq3x32.cpp
//16 bit input, see below for 32 bit input
// note: 16-bit input for asm version only!
void hq3x32(unsigned char * pIn,  unsigned int srcPitch,
            unsigned char *,
            unsigned char * pOut, unsigned int dstPitch,
            int Xres, int Yres);
void hq4x32(unsigned char * pIn,  unsigned int srcPitch,
            unsigned char *,
            unsigned char * pOut, unsigned int dstPitch,
            int Xres, int Yres);
void hq3x16(unsigned char * pIn,  unsigned int srcPitch,
            unsigned char *,
            unsigned char * pOut, unsigned int dstPitch,
            int Xres, int Yres);
void hq4x16(unsigned char * pIn,  unsigned int srcPitch,
            unsigned char *,
            unsigned char * pOut, unsigned int dstPitch,
            int Xres, int Yres);
// this takes 32-bit input
// (by converting to 16-bit first in asm version
void hq3x32_32(unsigned char *pIn,  unsigned int srcPitch, unsigned char *, unsigned char *pOut, unsigned int dstPitch, int Xres, int Yres);
void hq4x32_32(unsigned char *pIn,  unsigned int srcPitch, unsigned char *, unsigned char *pOut, unsigned int dstPitch, int Xres, int Yres);

#endif //FILTERS_FILTERS_HPP
