//Credits:
//  Kawaks' Mr. K for the code
//  Incorporated into vba by Anthony Di Franco
//  Converted to C++ by Arthur Moore
#include "../System.h"
#include <stdlib.h>
#include <memory.h>
#include <stdexcept>

#include "new_interframe.hpp"

void interframe_filter::setWidth(unsigned int _width)
{
    width = _width;

    //32 bit filter, so 4 bytes per pixel
    //  The +1 is for a 1 pixel border that the emulator spits out
    horiz_bytes = (width+1) * 4;
    //Unfortunately, the filter keeps the border on the scaled output, but DOES NOT scale it
//     horiz_bytes_out = width * 4 * myScale + 4;

}


SmartIB::SmartIB()
{
  frm1 = (u32 *)calloc(322*242,4);
  // 1 frame ago
  frm2 = (u32 *)calloc(322*242,4);
  // 2 frames ago
  frm3 = (u32 *)calloc(322*242,4);
  // 3 frames ago
}

SmartIB::~SmartIB()
{
  //\HACK to prevent double freeing.  (It looks like this is not being called in a thread safe manner!!!)

  if(frm1)
    free(frm1);
  if( frm2 && (frm1 != frm2) )
    free(frm2);
  if( frm3 && (frm1 != frm3) && (frm2 != frm3) )
    free(frm3);
  frm1 = frm2 = frm3 = NULL;
}

void SmartIB::run(u8 *srcPtr, unsigned int num_threads,unsigned int thread_number)
{
    //Actual width needs to take into account the +1 border
    unsigned int width = getWidth() +1;
    
    //Height to process (for multithreading)
    unsigned int band_height = getHeight() / num_threads;
    //First pixel to operate on (for multithreading)
    u32 offset = band_height * thread_number * width;

    u32 *src0 = reinterpret_cast<u32 *>(srcPtr) + offset;
    u32 *src1 = frm1 + offset;
    u32 *src2 = frm2 + offset;
    u32 *src3 = frm3 + offset;
    
    u32 colorMask = 0xfefefe;
    
    for (unsigned int i = 0; i < width*band_height;  i++)
    {
        u32 color = src0[i];
        src0[i] =
            (src1[i] != src2[i]) &&
            (src3[i] != color) &&
            ((color == src2[i]) || (src1[i] == src3[i]))
                ? (((color & colorMask) >> 1) + ((src1[i] & colorMask) >> 1)) :
                color;
        src3[i] = color; /* oldest buffer now holds newest frame */
    }

    /* Swap buffers around */
    u32 *temp = frm1;
    frm1 = frm3;
    frm3 = frm2;
    frm2 = temp;
}


MotionBlurIB::MotionBlurIB()
{
  frm1 = (u32 *)calloc(322*242,4);
  // 1 frame ago
  frm2 = (u32 *)calloc(322*242,4);
  // 2 frames ago
  frm3 = (u32 *)calloc(322*242,4);
  // 3 frames ago
}

MotionBlurIB::~MotionBlurIB()
{
  //\HACK to prevent double freeing.  (It looks like this is not being called in a thread safe manner!!!)

  if(frm1)
    free(frm1);
  if( frm2 && (frm1 != frm2) )
    free(frm2);
  if( frm3 && (frm1 != frm3) && (frm2 != frm3) )
    free(frm3);
  frm1 = frm2 = frm3 = NULL;
}

void MotionBlurIB::run(u8 *srcPtr, unsigned int num_threads,unsigned int thread_number)
{
    //Actual width needs to take into account the +1 border
    unsigned int width = getWidth() +1;
    
    //Height to process (for multithreading)
    unsigned int band_height = getHeight() / num_threads;
    
    //First pixel to operate on (for multithreading)
    u32 offset = band_height * thread_number * width;

    u32 *src0 = reinterpret_cast<u32 *>(srcPtr) + offset;
    u32 *src1 = frm1 + offset;

    u32 colorMask = 0xfefefe;

    for (unsigned int i = 0; i < width*band_height;  i++)
    {
        u32 color = src0[i];
        src0[i] = (((color & colorMask) >> 1) +
                  ((src1[i] & colorMask) >> 1));
        src1[i] = color;
    }
}
