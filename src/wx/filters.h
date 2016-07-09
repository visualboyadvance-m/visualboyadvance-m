#ifndef FILTERS_H
#define FILTERS_H

// FIXME: these should be in a system header included by all users and all
// files which define these functions
// most 16-bit filters require space in src rounded up to u32
// those that take delta take 1 src line of pixels, rounded up to u32 size
// initial value appears to be all-0xff
void Pixelate32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Pixelate(u8* src, u32 spitch, u8* delta, u8* dst, u32 dstp, int w, int h);
// next 3*2 use Init_2xSaI(555|565) and do not take into account
int Init_2xSaI(u32 BitFormat);
// endianness or bit shift variables in init.
// next 4*2 may be MMX-accelerated
void _2xSaI32(u8* src, u32 spitch, u8* delta, u8* dst, u32 dstp, int w, int h);
void _2xSaI(u8* src, u32 spitch, u8* delta, u8* dst, u32 dstp, int w, int h);
// void Scale_2xSaI(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);
void Super2xSaI32(u8* src, u32 spitch, u8* delta, u8* dst, u32 dstp, int w, int h);
void Super2xSaI(u8* src, u32 spitch, u8* delta, u8* dst, u32 dstp, int w, int h);
void SuperEagle32(u8* src, u32 spitch, u8* delta, u8* dst, u32 dstp, int w, int h);
void SuperEagle(u8* src, u32 spitch, u8* delta, u8* dst, u32 dstp, int w, int h);
void AdMame2x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void AdMame2x(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
// next 4 convert to rgb24 in internal buffers first, and then back again
void Bilinear32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Bilinear(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void BilinearPlus32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void BilinearPlus(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Scanlines32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Scanlines(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void ScanlinesTV32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
// "TV" here means each pixel is faded horizontally & vertically rather than
// inserting black scanlines
// this depends on RGB_LOW_BITS_MASK
extern int RGB_LOW_BITS_MASK;
void ScanlinesTV(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
// next 2 require calling hq2x_init first and whenever bpp changes
void hq2x_init(unsigned bpp);
void hq2x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void hq2x(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void lq2x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void lq2x(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
// the simple ones could greatly benefit from correct usage of preprocessor..
// in any case, they are worthless, since all renderers do "simple" or
// better by default
void Simple2x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Simple2x(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Simple3x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Simple3x(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Simple4x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void Simple4x(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
// note: 16-bit input for asm version only!
void hq3x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
// this takes 32-bit input
// (by converting to 16-bit first in asm version)
void hq3x32_32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void hq3x16(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
// note: 16-bit input for asm version only!
void hq4x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
// this takes 32-bit input
// (by converting to 16-bit first in asm version)
void hq4x32_32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void hq4x16(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);

void xbrz2x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void xbrz3x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void xbrz4x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void xbrz5x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);
void xbrz6x32(u8* src, u32 spitch, u8*, u8* dst, u32 dstp, int w, int h);

// call ifc to ignore previous frame / when starting new
void InterframeCleanup();
// all 4 are MMX-accelerated if enabled
void SmartIB(u8* src, u32 spitch, int width, int height);
void SmartIB32(u8* src, u32 spitch, int width, int height);
void MotionBlurIB(u8* src, u32 spitch, int width, int height);
void MotionBlurIB32(u8* src, u32 spitch, int width, int height);
void SmartIB(u8* src, u32 spitch, int width, int starty, int height);
void SmartIB32(u8* src, u32 spitch, int width, int starty, int height);
void MotionBlurIB(u8* src, u32 spitch, int width, int starty, int height);
void MotionBlurIB32(u8* src, u32 spitch, int width, int starty, int height);

#endif /* FILTERS_H */
