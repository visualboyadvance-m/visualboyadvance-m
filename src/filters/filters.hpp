/// All filters
#ifndef FILTERS_FILTERS_HPP
#define FILTERS_FILTERS_HPP

#include "interframe.hpp"

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

#endif //FILTERS_FILTERS_HPP
