/// All filters
#ifndef FILTERS_FILTERS_HPP
#define FILTERS_FILTERS_HPP

#include <map>
#include <string>
#include <stdexcept>
#include <iostream>

#include "interframe.hpp"
#include "../common/Types.h"

//sdl
// Function pointer type for a filter function
typedef void(*FilterFunc)(u8*, u32, u8*, u8*, u32, int, int);

//WX

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
    static FilterFunc GetFilter(std::string filterName)
    {
        std::map<std::string,filterpair>::const_iterator found = filterMap.find(filterName);
        if(found == filterMap.end()){
            //Not doing the error checking here
//             throw std::runtime_error("ERROR:  Filter not found!");
            return NULL;
        }
        return found->second.first;
    };
    ///Returns a function pointer to a 16 bit filter
    static FilterFunc GetFilter16(std::string filterName)
    {
        std::map<std::string,filterpair>::const_iterator found = filterMap.find(filterName);
        if(found == filterMap.end()){
//             throw std::runtime_error("ERROR:  Filter not found!");
            return NULL;
        }
        return found->second.second;
    };
    ///Returns the filter's scaling factor
    ///TODO:  De hardcode this
    static int GetFilterScale(std::string filterName)
    {
        if(filterName == "HQ 4x" || filterName == "Simple 4x")
            return 4;
        if(filterName == "HQ 3x" || filterName == "Simple 3x")
            return 3;
        if(filterName == "None")
            return 1;
        return 2;
    }
};

///This is the parent class of all the filters
///TODO:  Actually subclass these instead of cheating
class filter {
private:
    //No default constructor for this class
    filter();
    ///The filter's name
    std::string name;
    ///The internal filter used
    FilterFunc myFilter;
    ///The internal scale
    int myScale;
    ///The filter's width
    unsigned int width;
    ///Don't need to calculate these every time (based off width)
    unsigned int horiz_bytes;
    unsigned int horiz_bytes_out;
public:
    filter(std::string myName):
        name(myName), myFilter(filters::GetFilter(myName)), myScale(filters::GetFilterScale(myName)),
        width(0), horiz_bytes(0), horiz_bytes_out(0)
    {
//         std::cerr << name << std::endl;
    }
    std::string getName()
    {
        return name;
    }
    ///Set the number of pixels per horizontal row
    ///
    ///Always use this after initialization if using the new run function.
    void setWidth(unsigned int _width)
    {
        width = _width;

        //32 bit filter, so 8 bytes per pixel
        //  The +1 is for a 1 pixel border that the emulator spits out
        horiz_bytes = (width+1) * 4;
        //Unfortunately, the filter keeps the border on the scaled output, but DOES NOT scale it
        horiz_bytes_out = width * 4 * myScale + 4;

    }
    unsigned int getWidth()
    {
        return width;
    }
    int getScale()
    {
        return myScale;
    }
    /**
     * New Version:  Run the filter
     *
     * This one is smart.
     * It knows it's a 32 bit filter, and the input width will not change from when it is initialized.
     *
     * \param[in] srcPtr        A pointer to a 16/32 bit RGB Pixel Array
     */
    void run(u8 *srcPtr, u8 *deltaPtr, u8 *dstPtr, int height)
    {
        if(!width)
        {
             throw std::runtime_error("ERROR:  Filter width not set");
        }

        std::cerr << name <<": width: "<<width<<" height: "<<height<< std::endl;

        if(myFilter!=NULL)
        {
            myFilter(srcPtr,horiz_bytes,deltaPtr,dstPtr,horiz_bytes_out,width,height);
        }
        else
        {
            throw std::runtime_error("ERROR:  Filter does not exist!");
        }
    }
    /**
     * DEPRECATED  Run the filter
     *
     * \param[in] srcPtr        A pointer to a 16/32 bit RGB Pixel Array
     * \param[in] srcPitch     The number of bytes per single horizontal line
     */
    void run(u8 *srcPtr, u32 srcPitch, u8 *deltaPtr, u8 *dstPtr, u32 dstPitch, int width, int height)
    {
        setWidth(width);
        //Make sure the math was correct
        if( (srcPitch != horiz_bytes) || dstPitch != horiz_bytes_out )
        {
            throw std::runtime_error("ERROR:  Filter programmer is an idiot, and messed up an important calculation!");
        }
        run(srcPtr, deltaPtr, dstPtr, height);
    }
    bool exists()
    {
        if (myFilter==NULL)
            return false;
        else
            return true;
    }
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
