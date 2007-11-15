// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#include "Util.h"
#define __STDC_CONSTANT_MACROS

#ifndef _MSC_VER
#include <stdint.h>
#else
typedef unsigned int uint32_t;
#endif


extern "C"
{
void hq3x_16(unsigned char*, unsigned char*, uint32_t, uint32_t, uint32_t, uint32_t);
void hq3x_32(unsigned char*, unsigned char*, uint32_t, uint32_t, uint32_t, uint32_t);
void hq4x_16(unsigned char*, unsigned char*, uint32_t, uint32_t, uint32_t, uint32_t);
void hq4x_32(unsigned char*, unsigned char*, uint32_t, uint32_t, uint32_t, uint32_t);

unsigned int   LUT16to32[65536];
unsigned int   RGBtoYUV[65536];
}

void InitLUTs(void)
{
  int i, j, k, r, g, b, Y, u, v;

  for (i=0; i<65536; i++)
    LUT16to32[i] = ((i & 0xF800) << 8) + ((i & 0x07E0) << 5) + ((i & 0x001F) << 3);

  for (i=0; i<32; i++)
  for (j=0; j<64; j++)
  for (k=0; k<32; k++)
  {
    r = i << 3;
    g = j << 2;
    b = k << 3;
    Y = (r + g + b) >> 2;
    u = 128 + ((r - b) >> 2);
    v = 128 + ((-r + 2*g -b)>>3);
    RGBtoYUV[ (i << 11) + (j << 5) + k ] = (Y<<16) + (u<<8) + v;
  }
}

int hq3xinited=0;
extern int realsystemRedShift, realsystemBlueShift;

void hq3x32(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char *,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres)
{
	// NOTICE!  This driver wants 16 bit, not 32 bit input!

	if (!hq3xinited)
	{
		InitLUTs();
		hq3xinited=1;
	}
	hq3x_32( pIn, pOut, Xres, Yres, dstPitch, srcPitch - (Xres *2) );
	if (realsystemRedShift == 3)
	{   // damn you opengl...
		int offset = (dstPitch - (Xres *12)) / 4;
		unsigned int *p = (unsigned int *)pOut;
		Yres *= 3;
		while(Yres--)
		{
			for(int i=0;i<Xres*3;i++)
			{
				*p = (*p & 0xFF0000) >> 16 |
				     (*p & 0x0000FF) << 16 |
					 (*p & 0x00FF00);
				 p++;
			}
			p += offset;
		}
	}
}

void hq3x16(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char *,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres)
{
	if (!hq3xinited)
	{
		InitLUTs();
		hq3xinited=1;
	}
	hq3x_16( pIn, pOut, Xres, Yres, dstPitch, srcPitch - (Xres *2));
}


void hq4x16(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char *,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres)
{
	if (!hq3xinited)
	{
		InitLUTs();
		hq3xinited=1;
	}
	hq4x_16( pIn, pOut, Xres, Yres, dstPitch, srcPitch - (Xres *2));
}

void hq4x32(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char *,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres)
{
	// NOTICE!  This driver wants 16 bit, not 32 bit input!

	if (!hq3xinited)
	{
		InitLUTs();
		hq3xinited=1;
	}
	hq4x_32( pIn, pOut, Xres, Yres, dstPitch, srcPitch - (Xres *2));
	if (realsystemRedShift == 3)
	{   // damn you opengl...
		int offset = (dstPitch - (Xres *16)) / 4;
		unsigned int *p = (unsigned int *)pOut;
		Yres *= 4;
		while(Yres--)
		{
			for(int i=0;i<Xres*4;i++)
			{
				*p = (*p & 0xFF0000) >> 16 |
				     (*p & 0x0000FF) << 16 |
					 (*p & 0x00FF00);
				 p++;
			}
			p += offset;
		}
	}
}
