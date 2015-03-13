/// All filters
#ifndef FILTERS_FILTERS_HPP
#define FILTERS_FILTERS_HPP

#include <map>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cstring>  //For memcpy

#include "../common/Types.h"
#include "filter_base.hpp"

//sdl
// Function pointer type for a filter function
typedef void(*FilterFunc)(u8*, u32, u8*, u32, int, int);

typedef std::pair<std::string,FilterFunc> namedfilter;

///A class allowing for easy access to all the filters
class filters {
private:
    //A named map of all the filters
    static const std::map<std::string,FilterFunc> filterMap;
public:
    ///Returns a function pointer to a 32 bit filter
    static FilterFunc GetFilter(std::string filterName)
    {
        std::map<std::string,FilterFunc>::const_iterator found = filterMap.find(filterName);
        if(found == filterMap.end()){
            //Not doing the error checking here
//             throw std::runtime_error("ERROR:  Filter not found!");
            return NULL;
        }
        return found->second;
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
class filter : public filter_base
{
private:
    //No default constructor for this class
    filter();
    ///The filter's name
    std::string name;
    ///The internal filter used
    FilterFunc myFilter;
    ///The internal scale
    int myScale;
    //Don't need to calculate these every time (based off width)
    ///The number of pixels per horizontal row
    unsigned int horiz_bytes;
    ///The number of pixels per output horizontal row
    unsigned int horiz_bytes_out;
public:
    filter(std::string myName,unsigned int _width,unsigned int _height):
        filter_base(_width,_height),
        name(myName), myFilter(filters::GetFilter(myName)),
        myScale(filters::GetFilterScale(myName)),
        horiz_bytes(_width * 4), horiz_bytes_out(_width * 4 * filters::GetFilterScale(myName))
    {
//         std::cerr << name << std::endl;
    }
    std::string getName()
    {
        return name;
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
     * \param[in] srcPtr        A pointer to the input 32 bit RGB Pixel Array
     * \param[in] dstPtr        A pointer to the output 32 bit RGB Pixel Array
     */
    void run(u32 *srcPtr, u32 *dstPtr)
    {
        if(myFilter!=NULL)
        {
            myFilter(reinterpret_cast<u8 *>(srcPtr),horiz_bytes,reinterpret_cast<u8 *>(dstPtr),horiz_bytes_out,getWidth(),getHeight());
        }
        else
        {
            //If the filter doesn't exist, then we still need to get the data to the output buffer
            std::memcpy(dstPtr,srcPtr, horiz_bytes_out*getHeight()*myScale);
        }
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
void Pixelate32(u8 *srcPtr, u32 srcPitch, u8 *dstPtr, u32 dstPitch, int width, int height);

//scanline.cpp
void Scanlines32(u8 *srcPtr, u32 srcPitch, u8 *dstPtr, u32 dstPitch, int width, int height);
// "TV" here means each pixel is faded horizontally & vertically rather than
// inserting black scanlines
void ScanlinesTV32(u8 *srcPtr, u32 srcPitch, u8 *dstPtr, u32 dstPitch, int width, int height);

//simpleFilter.cpp
// the simple ones could greatly benefit from correct usage of preprocessor..
// in any case, they are worthless, since all renderers do "simple" or
// better by default
int Init_2xSaI(u32 BitFormat);
void Simple2x32(u8 *srcPtr, u32 srcPitch,
                u8 *dstPtr, u32 dstPitch, int width, int height);
void Simple3x32(u8 *srcPtr, u32 srcPitch,
                u8 *dstPtr, u32 dstPitch, int width, int height);
void Simple4x32(u8 *srcPtr, u32 srcPitch,
                u8 *dstPtr, u32 dstPitch, int width, int height);

//bilinear.cpp
//These convert to rgb24 in internal buffers first, and then back again
/*static void fill_rgb_row_16(u16 *from, int src_width, u8 *row, int width);
static void fill_rgb_row_32(u32 *from, int src_width, u8 *row, int width);*/
void Bilinear32(u8 *srcPtr, u32 srcPitch,
                u8 *dstPtr, u32 dstPitch, int width, int height);
void BilinearPlus32(u8 *srcPtr, u32 srcPitch,
                    u8 *dstPtr, u32 dstPitch, int width, int height);

//admame.cpp
void AdMame2x32(u8 *srcPtr, u32 srcPitch,
                u8 *dstPtr, u32 dstPitch, int width, int height);

//2xSal.cpp
// next 3*2 use Init_2xSaI(555|565) and do not take into account
int Init_2xSaI(u32 BitFormat);
// endianness or bit shift variables in init.
// next 4*2 may be MMX-accelerated
void _2xSaI32 (u8 *srcPtr, u32 srcPitch,
               u8 *dstPtr, u32 dstPitch, int width, int height);
//This one was commented out in other source files
void Super2xSaI32 (u8 *srcPtr, u32 srcPitch,
                   u8 *dstPtr, u32 dstPitch,
                   int width, int height);
void SuperEagle32 (u8 *srcPtr, u32 srcPitch,
                   u8 *dstPtr, u32 dstPitch, int width, int height);
//hq2x.cpp
//These require calling hq2x_init first and whenever bpp changes
void hq2x_init(unsigned bits_per_pixel);
void hq2x32(u8 *srcPtr, u32 srcPitch,
            u8 *dstPtr, u32 dstPitch, int width, int height);
void lq2x32(u8 *srcPtr, u32 srcPitch,
            u8 *dstPtr, u32 dstPitch, int width, int height);

//hq/asm/hq3x32.cpp
//16 bit input, see below for 32 bit input
// note: 16-bit input for asm version only!
void hq3x32(unsigned char * pIn,  unsigned int srcPitch,
            unsigned char * pOut, unsigned int dstPitch,
            int Xres, int Yres);
void hq4x32(unsigned char * pIn,  unsigned int srcPitch,
            unsigned char * pOut, unsigned int dstPitch,
            int Xres, int Yres);
// this takes 32-bit input
// (by converting to 16-bit first in asm version
void hq3x32_32(unsigned char *pIn,  unsigned int srcPitch, unsigned char *pOut, unsigned int dstPitch, int Xres, int Yres);
void hq4x32_32(unsigned char *pIn,  unsigned int srcPitch, unsigned char *pOut, unsigned int dstPitch, int Xres, int Yres);

//sdl.cpp
//Don't even know if these work or not
extern bool sdlStretchInit(int sizeMultiplier, int srcWidth);

extern void sdlStretch1x(u8*,u32,u8*,u32,int,int);
extern void sdlStretch2x(u8*,u32,u8*,u32,int,int);
extern void sdlStretch3x(u8*,u32,u8*,u32,int,int);
extern void sdlStretch4x(u8*,u32,u8*,u32,int,int);

/**
    * Convert a 32 bit image to a 24 bit one
    *
    * This centralizes a decent bit of code.
    * NOTE:  This takes width and height, and ASSUMES their accurate!!!!!
    *
    * \param[in] src    A pointer to the input 32 bit RGB Pixel Array
    * \param[in] dst    A pointer to the output 24 bit RGB Pixel Array
    * \param[in] width  The image width (in pixels)
    * \param[in] height The height width (in pixels)
    */
void convert32To24(u32* src,u8* dst,unsigned int width, unsigned int height);

#endif //FILTERS_FILTERS_HPP
