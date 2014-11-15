#ifndef FILTERS_H
#define FILTERS_H

// FIXME: these should be in a system header included by all users and all
// files which define these functions
// most 16-bit filters require space in src rounded up to u32
// those that take delta take 1 src line of pixels, rounded up to u32 size
// initial value appears to be all-0xff

#include "../filters/filters.hpp"

// next 3*2 use Init_2xSaI(555|565) and do not take into account
int Init_2xSaI(u32 BitFormat);
// endianness or bit shift variables in init.
// next 4*2 may be MMX-accelerated
void _2xSaI32(u8 *src, u32 spitch, u8 *delta, u8 *dst, u32 dstp, int w, int h);
void _2xSaI(u8 *src, u32 spitch, u8 *delta, u8 *dst, u32 dstp, int w, int h);
// void Scale_2xSaI(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);
void Super2xSaI32(u8 *src, u32 spitch, u8 *delta, u8 *dst, u32 dstp, int w, int h);
void Super2xSaI(u8 *src, u32 spitch, u8 *delta, u8 *dst, u32 dstp, int w, int h);
void SuperEagle32(u8 *src, u32 spitch, u8 *delta, u8 *dst, u32 dstp, int w, int h);
void SuperEagle(u8 *src, u32 spitch, u8 *delta, u8 *dst, u32 dstp, int w, int h);
void AdMame2x32(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);
void AdMame2x(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);

// note: 16-bit input for asm version only!
void hq3x32(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);
// this takes 32-bit input
// (by converting to 16-bit first in asm version)
void hq3x32_32(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);
void hq3x16(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);
// note: 16-bit input for asm version only!
void hq4x32(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);
// this takes 32-bit input
// (by converting to 16-bit first in asm version)
void hq4x32_32(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);
void hq4x16(u8 *src, u32 spitch, u8 *, u8 *dst, u32 dstp, int w, int h);

#endif /* FILTERS_H */
