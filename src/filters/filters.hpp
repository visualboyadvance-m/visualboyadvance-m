/// All filters
#ifndef FILTERS_FILTERS_HPP
#define FILTERS_FILTERS_HPP

#include <map>
#include <string>
#include <stdexcept>

#include "interframe.hpp"
#include "../common/Types.h"

//sdl
// Function pointer type for a filter function
typedef void(*FilterFunc)(u8*, u32, u8*, u8*, u32, int, int);

//WX
enum wx_filtfunc {
    // this order must match order of option enum and selector widget
    FF_NONE, FF_2XSAI, FF_SUPER2XSAI, FF_SUPEREAGLE, FF_PIXELATE,
    FF_ADVMAME, FF_BILINEAR, FF_BILINEARPLUS, FF_SCANLINES, FF_TV,
    FF_HQ2X, FF_LQ2X, FF_SIMPLE2X, FF_SIMPLE3X, FF_HQ3X, FF_SIMPLE4X,
    FF_HQ4X, FF_PLUGIN  // plugin must always be last
};
#define builtin_ff_scale(x) \
    ((x == FF_HQ4X || x == FF_SIMPLE4X) ? 4 : \
    (x == FF_HQ3X || x == FF_SIMPLE3X) ? 3 : \
     x == FF_PLUGIN ? 0 : x == FF_NONE ? 1 : 2)
enum ifbfunc {
    // this order must match order of option enum and selector widget
    IFB_NONE, IFB_SMART, IFB_MOTION_BLUR
};

typedef std::pair<FilterFunc,FilterFunc> filterpair;
typedef std::pair<std::string,filterpair> namedfilter;

///A class allowing for easy access to all the filters
class filters {
private:
    //A named map of all the filters
    static const std::map<std::string,filterpair> filterMap;
public:
    ///Returns a function pointer to a 32 bit filter
    FilterFunc GetFilter(std::string filterName)
    {
        std::map<std::string,filterpair>::const_iterator found = filterMap.find(filterName);
        if(found == filterMap.end()){
            throw std::runtime_error("ERROR:  Filter not found!");
            return NULL;
        }
        return found->second.first;
    };
    ///Returns a function pointer to a 16 bit filter
    FilterFunc GetFilter16(std::string filterName)
    {
        std::map<std::string,filterpair>::const_iterator found = filterMap.find(filterName);
        if(found == filterMap.end()){
            throw std::runtime_error("ERROR:  Filter not found!");
            return NULL;
        }
        return found->second.second;
    };
};

//These are the available filters

//wx
// src/wx/wxvbam.h:263-278
// src/wxpanel.cpp:1100-1256

//gtk
// src/gtk/filters.h
// src/gtk/filters.cpp

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

//sdl.cpp
//Don't even know if these work or not
extern bool sdlStretchInit(int colorDepth, int sizeMultiplier, int srcWidth);

extern void sdlStretch1x(u8*,u32,u8*,u8*,u32,int,int);
extern void sdlStretch2x(u8*,u32,u8*,u8*,u32,int,int);
extern void sdlStretch3x(u8*,u32,u8*,u8*,u32,int,int);
extern void sdlStretch4x(u8*,u32,u8*,u8*,u32,int,int);

#endif //FILTERS_FILTERS_HPP
