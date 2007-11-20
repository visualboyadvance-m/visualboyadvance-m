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

#include "hq_shared32.h"
#define __STDC_CONSTANT_MACROS


#include <stdint.h>


const uint64_t reg_blank = UINT64_C(0x0000000000000000);
const uint64_t const7    = UINT64_C(0x0000000700070007);
const uint64_t treshold  = UINT64_C(0x0000000000300706);

void Interp1(unsigned char * pc, unsigned int c1, unsigned int c2)
{
  *((int*)pc) = (c1*3+c2)/4;
}

void Interp2(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3)
{
  *((int*)pc) = (c1*2+c2+c3)/4;
}

void Interp3(unsigned char * pc, unsigned int c1, unsigned int c2)
{
  *((int*)pc) = (c1*7+c2)/8;
  *((int*)pc) = ((((c1 & 0x00FF00)*7 + (c2 & 0x00FF00) ) & 0x0007F800) +
                (((c1 & 0xFF00FF)*7 + (c2 & 0xFF00FF) ) & 0x07F807F8)) >> 3;
}

void Interp4(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3)
{
  *((int*)pc) = (c1*2+(c2+c3)*7)/16;
  *((int*)pc) = ((((c1 & 0x00FF00)*2 + ((c2 & 0x00FF00) + (c3 & 0x00FF00))*7 ) & 0x000FF000) +
                (((c1 & 0xFF00FF)*2 + ((c2 & 0xFF00FF) + (c3 & 0xFF00FF))*7 ) & 0x0FF00FF0)) >> 4;
}

void Interp5(unsigned char * pc, unsigned int c1, unsigned int c2)
{
  *((int*)pc) = (c1+c2)/2;
}

void Interp6(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3)
{
  *((int*)pc) = (c1*5+c2*2+c3)/8;
}

void Interp7(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3)
{
  *((int*)pc) = (c1*6+c2+c3)/8;
}

void Interp8(unsigned char * pc, unsigned int c1, unsigned int c2)
{
  *((int*)pc) = (c1*5+c2*3)/8;
}


bool Diff(unsigned int c1, unsigned int c2)
{
	unsigned int
		YUV1 = RGBtoYUV(c1),
		YUV2 = RGBtoYUV(c2);

	if (YUV1 == YUV2) return false; // Save some processing power

	return
		( abs32((YUV1 & Ymask) - (YUV2 & Ymask)) > trY ) ||
		( abs32((YUV1 & Umask) - (YUV2 & Umask)) > trU ) ||
		( abs32((YUV1 & Vmask) - (YUV2 & Vmask)) > trV );
}


unsigned int RGBtoYUV(unsigned int c)
{
	// Division through 3 slows down the emulation about 10% !!!
	unsigned char r, g, b, Y, u, v;
	r = (c & 0x000000FF);
	g = (c & 0x0000FF00) >> 8;
	b = (c & 0x00FF0000) >> 16;
	Y = (r + g + b) >> 2;
	u = 128 + ((r - b) >> 2);
	v = 128 + ((-r + 2*g -b)>>3);
	return (Y<<16) + (u<<8) + v;
}
