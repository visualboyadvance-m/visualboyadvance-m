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
#include "xBRZ/xbrz.h"

//sdl
// Function pointer type for a filter function
typedef void(*FilterFunc)(u8*, u32, u8*, u32, int, int);

///This is the parent class of all the filters
///TODO:  Actually subclass these instead of cheating
class raw_filter : public filter_base
{
private:
    //No default constructor for this class
    raw_filter();
    ///The filter's name
    std::string name;
    ///The internal filter used
    FilterFunc myFilter;
    //Don't need to calculate these every time (based off width)
    ///The number of pixels per horizontal row
    unsigned int horiz_bytes;
    ///The number of pixels per output horizontal row
    unsigned int horiz_bytes_out;
public:
    raw_filter(std::string _name,FilterFunc _myFilter,unsigned int _scale,unsigned int _width,unsigned int _height):
        filter_base(_width,_height),
        name(_name), myFilter(_myFilter),
        horiz_bytes(_width * 4), horiz_bytes_out(_width * 4 * _scale)
    {
        this->setScale(_scale);
//         std::cerr << name << std::endl;
    }
    std::string getName() {return name;}
    bool exists() {return true;}
    ///Run the filter pointed to by the internal FilterFunc
    void run(u32 *srcPtr, u32 *dstPtr)
    {
        if(myFilter==NULL)
        {
            throw std::runtime_error("ERROR:  Filter not properly initialized!!!");
        }
        myFilter(reinterpret_cast<u8 *>(srcPtr),horiz_bytes,reinterpret_cast<u8 *>(dstPtr),horiz_bytes_out,getWidth(),getHeight());
    }
};

//Custom filter classes
///XBR filter
///Supports 2-5 X scaling
class xbr : public filter_base
{
private:
    //Must enter width and height at filter initialization
    xbr();
public:
    xbr(unsigned int _width,unsigned int _height,unsigned int scale = 5): filter_base(_width,_height) {this->setScale(scale);}
    std::string getName() {return "XBR "+std::to_string(this->getScale())+"x";}
    bool exists() {return true;}
    void run(u32 *srcPtr,u32 *dstPtr)
    {
        xbrz::scale(this->getScale(),
           srcPtr, dstPtr, getWidth(), getHeight(),
           xbrz::ColorFormat::ARGB);
    }
};

typedef std::pair<std::string,FilterFunc> namedfilter;

class filter_factory
{
private:
    //A named map of all the (original) filters
    static const std::map<std::string,FilterFunc> filterMap;
public:
    ///Returns an instance of a filter of filterName
    ///If the filter doesn't exist, it returns a dummy filter instead
    static filter_base * createFilter(std::string filterName,unsigned int width,unsigned int height)
    {
        std::map<std::string,FilterFunc>::const_iterator found = filterMap.find(filterName);
        //If we found the filter:
        if(found != filterMap.end()){
            return new raw_filter(filterName,found->second,GetFilterScale(filterName),width,height);
        }

        if("XBR 2x" == filterName)
        {
            return new xbr(width,height,2);
        }
        if("XBR 3x" == filterName)
        {
            return new xbr(width,height,3);
        }
        if("XBR 4x" == filterName)
        {
            return new xbr(width,height,4);
        }
        if("XBR 5x" == filterName)
        {
            return new xbr(width,height,5);
        }

        //If nothing found, just return a default filter
        return new filter_base(width,height);
    }

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

//These are the available filters

//wx
// src/wxpanel.cpp

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
    * NOTE:  This takes width and height, and ASSUMES they are accurate!!!!!
    *
    * \param[in] src    A pointer to the input 32 bit RGB Pixel Array
    * \param[in] dst    A pointer to the output 24 bit RGB Pixel Array
    * \param[in] width  The image width (in pixels)
    * \param[in] height The height width (in pixels)
    */
void convert32To24(u32* src,u8* dst,unsigned int width, unsigned int height);

/**
 * Get a pointer to the first pixel of a row inside an image
 *
 * NOTE:  If width or vertical_offset is too large, this function WILL produce an invalid pointer
 *
 * \param[in] image_pointer     A pointer to the start of an image
 * \param[in] width             The image's width
 * \param[in] vertical_offset   How many rows from the start of the image
 * \param[in] scale             How much larger the output image will be
 * \return                      A pointer to the first pixel in the chosen row
 */
inline u32 * GetVerticalOffset(u32 * image_pointer,unsigned int width,unsigned int vertical_offset,unsigned int scale=1)
{
    return image_pointer + (width * vertical_offset*scale*scale);
}

#endif //FILTERS_FILTERS_HPP
