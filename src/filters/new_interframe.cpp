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

void interframe_filter::run(u8 *srcPtr, int starty, int height)
{
    if(!getWidth())
    {
            throw std::runtime_error("ERROR:  Filter width not set");
    }
    this->run(srcPtr, get_horiz_bytes(), getWidth(), starty, height);
}

SmartIB::SmartIB()
{
  frm1 = (u8 *)calloc(322*242,4);
  // 1 frame ago
  frm2 = (u8 *)calloc(322*242,4);
  // 2 frames ago
  frm3 = (u8 *)calloc(322*242,4);
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

void SmartIB::run(u8 *srcPtr, u32 srcPitch, int width, int starty, int height)
{
    setWidth(width);
    //Make sure the math was correct
    if( (srcPitch != get_horiz_bytes()) )
    {
        throw std::runtime_error("ERROR:  Filter programmer is an idiot, and messed up an important calculation!");
    }

  u32 *src0 = (u32 *)srcPtr + starty * srcPitch / 4;
  u32 *src1 = (u32 *)frm1 + starty * srcPitch / 4;
  u32 *src2 = (u32 *)frm2 + starty * srcPitch / 4;
  u32 *src3 = (u32 *)frm3 + starty * srcPitch / 4;

  u32 colorMask = 0xfefefe;

  int sPitch = srcPitch >> 2;
  int pos = 0;

  for (int j = 0; j < height;  j++)
    for (int i = 0; i < sPitch; i++) {
      u32 color = src0[pos];
      src0[pos] =
        (src1[pos] != src2[pos]) &&
        (src3[pos] != color) &&
        ((color == src2[pos]) || (src1[pos] == src3[pos]))
        ? (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1)) :
        color;
      src3[pos] = color; /* oldest buffer now holds newest frame */
      pos++;
    }

  /* Swap buffers around */
  u8 *temp = frm1;
  frm1 = frm3;
  frm3 = frm2;
  frm2 = temp;
}


MotionBlurIB::MotionBlurIB()
{
  frm1 = (u8 *)calloc(322*242,4);
  // 1 frame ago
  frm2 = (u8 *)calloc(322*242,4);
  // 2 frames ago
  frm3 = (u8 *)calloc(322*242,4);
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

void MotionBlurIB::run(u8 *srcPtr, u32 srcPitch, int width, int starty, int height)
{
    setWidth(width);
    //Make sure the math was correct
    if( (srcPitch != get_horiz_bytes()) )
    {
        throw std::runtime_error("ERROR:  Filter programmer is an idiot, and messed up an important calculation!");
    }

  u32 *src0 = (u32 *)srcPtr + starty * srcPitch / 4;
  u32 *src1 = (u32 *)frm1 + starty * srcPitch / 4;

  u32 colorMask = 0xfefefe;

  int sPitch = srcPitch >> 2;
  int pos = 0;

  for (int j = 0; j < height;  j++)
    for (int i = 0; i < sPitch; i++) {
      u32 color = src0[pos];
      src0[pos] = (((color & colorMask) >> 1) +
                   ((src1[pos] & colorMask) >> 1));
      src1[pos] = color;
      pos++;
    }
}
