//Credits:
//  Kawaks' Mr. K for the code
//  Incorporated into vba by Anthony Di Franco
//  Converted to C++ by Arthur Moore
#include "../System.h"
#include <stdlib.h>
#include <memory.h>
#include <stdexcept>

#include "new_interframe.hpp"

SmartIB::SmartIB(unsigned int _width,unsigned int _height): filter_base(_width,_height)
{
  frm1 = (u32 *)calloc(_width*_height,4);
  // 1 frame ago
  frm2 = (u32 *)calloc(_width*_height,4);
  // 2 frames ago
  frm3 = (u32 *)calloc(_width*_height,4);
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

void SmartIB::run(u32 *srcPtr,u32 *dstPtr)
{
    u32 *src1 = frm1;
    u32 *src2 = frm2;
    u32 *src3 = frm3;

    u32 colorMask = 0xfefefe;

    for (unsigned int i = 0; i < getWidth()*getHeight();  i++)
    {
        u32 color = srcPtr[i];
        dstPtr[i] =
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


MotionBlurIB::MotionBlurIB(unsigned int _width,unsigned int _height): filter_base(_width,_height)
{
    //Buffer to hold last frame
    frm1 = (u32 *)calloc(_width*_height,4);
}

MotionBlurIB::~MotionBlurIB()
{
  if(frm1)
    free(frm1);
  frm1=NULL;
}

void MotionBlurIB::run(u32 *srcPtr,u32 *dstPtr)
{
    u32 *src1 = frm1;

    u32 colorMask = 0xfefefe;

    for (unsigned int i = 0; i < getWidth()*getHeight();  i++)
    {
        u32 color = srcPtr[i];
        dstPtr[i] = (((color & colorMask) >> 1) +
                  ((src1[i] & colorMask) >> 1));
        src1[i] = color;
    }
}
