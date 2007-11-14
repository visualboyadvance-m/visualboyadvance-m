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

#define SIZE_PIXEL 4 // 32bit = 4 bytes

#define abs32(value) (value & 0x7FFFFFFF)

#define PIXEL00_1M  Interp1( pOut, c[5], c[1]       );
#define PIXEL00_1U  Interp1( pOut, c[5], c[2]       );
#define PIXEL00_1L  Interp1( pOut, c[5], c[4]       );
#define PIXEL00_2   Interp2( pOut, c[5], c[4], c[2] );
#define PIXEL00_4   Interp4( pOut, c[5], c[4], c[2] );
#define PIXEL00_5   Interp5( pOut, c[4], c[2]       );
#define PIXEL00_C   *((unsigned int*)(pOut)) = c[5];

#define PIXEL01_1   Interp1( pOut+SIZE_PIXEL, c[5], c[2] );
#define PIXEL01_3   Interp3( pOut+SIZE_PIXEL, c[5], c[2] );
#define PIXEL01_6   Interp1( pOut+SIZE_PIXEL, c[2], c[5] );
#define PIXEL01_C   *((unsigned int*)(pOut+4)) = c[5];

#define PIXEL02_1M  Interp1( pOut+SIZE_PIXEL+SIZE_PIXEL, c[5], c[3]       );
#define PIXEL02_1U  Interp1( pOut+SIZE_PIXEL+SIZE_PIXEL, c[5], c[2]       );
#define PIXEL02_1R  Interp1( pOut+SIZE_PIXEL+SIZE_PIXEL, c[5], c[6]       );
#define PIXEL02_2   Interp2( pOut+SIZE_PIXEL+SIZE_PIXEL, c[5], c[2], c[6] );
#define PIXEL02_4   Interp4( pOut+SIZE_PIXEL+SIZE_PIXEL, c[5], c[2], c[6] );
#define PIXEL02_5   Interp5( pOut+SIZE_PIXEL+SIZE_PIXEL, c[2], c[6]       );
#define PIXEL02_C   *((unsigned int*)(pOut+SIZE_PIXEL+SIZE_PIXEL)) = c[5];

#define PIXEL10_1   Interp1( pOut+dstPitch, c[5], c[4] );
#define PIXEL10_3   Interp3( pOut+dstPitch, c[5], c[4] );
#define PIXEL10_6   Interp1( pOut+dstPitch, c[4], c[5] );
#define PIXEL10_C   *((unsigned int*)(pOut+dstPitch)) = c[5];

#define PIXEL11     *((unsigned int*)(pOut+dstPitch+SIZE_PIXEL)) = c[5];

#define PIXEL12_1   Interp1( pOut+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[5], c[6] );
#define PIXEL12_3   Interp3( pOut+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[5], c[6] );
#define PIXEL12_6   Interp1( pOut+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[6], c[5] );
#define PIXEL12_C   *((unsigned int*)(pOut+dstPitch+SIZE_PIXEL+SIZE_PIXEL)) = c[5];

#define PIXEL20_1M  Interp1( pOut+dstPitch+dstPitch, c[5], c[7]       );
#define PIXEL20_1D  Interp1( pOut+dstPitch+dstPitch, c[5], c[8]       );
#define PIXEL20_1L  Interp1( pOut+dstPitch+dstPitch, c[5], c[4]       );
#define PIXEL20_2   Interp2( pOut+dstPitch+dstPitch, c[5], c[8], c[4] );
#define PIXEL20_4   Interp4( pOut+dstPitch+dstPitch, c[5], c[8], c[4] );
#define PIXEL20_5   Interp5( pOut+dstPitch+dstPitch, c[8], c[4]       );
#define PIXEL20_C   *((unsigned int*)(pOut+dstPitch+dstPitch)) = c[5];

#define PIXEL21_1   Interp1( pOut+dstPitch+dstPitch+SIZE_PIXEL, c[5], c[8] );
#define PIXEL21_3   Interp3( pOut+dstPitch+dstPitch+SIZE_PIXEL, c[5], c[8] );
#define PIXEL21_6   Interp1( pOut+dstPitch+dstPitch+SIZE_PIXEL, c[8], c[5] );
#define PIXEL21_C   *((unsigned int*)(pOut+dstPitch+dstPitch+SIZE_PIXEL)) = c[5];

#define PIXEL22_1M  Interp1( pOut+dstPitch+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[5], c[9]       );
#define PIXEL22_1D  Interp1( pOut+dstPitch+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[5], c[8]       );
#define PIXEL22_1R  Interp1( pOut+dstPitch+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[5], c[6]       );
#define PIXEL22_2   Interp2( pOut+dstPitch+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[5], c[6], c[8] );
#define PIXEL22_4   Interp4( pOut+dstPitch+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[5], c[6], c[8] );
#define PIXEL22_5   Interp5( pOut+dstPitch+dstPitch+SIZE_PIXEL+SIZE_PIXEL, c[6], c[8]       );
#define PIXEL22_C   *((unsigned int*)(pOut+dstPitch+dstPitch+SIZE_PIXEL+SIZE_PIXEL)) = c[5];

const  int	Ymask = 0x00FF0000;
const  int	Umask = 0x0000FF00;
const  int	Vmask = 0x000000FF;
const  int	trY   = 0x00300000;
const  int	trU   = 0x00000700;
const  int	trV   = 0x00000006;

void Interp1(unsigned char * pc, unsigned int c1, unsigned int c2);
void Interp2(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3);
void Interp3(unsigned char * pc, unsigned int c1, unsigned int c2);
void Interp4(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3);
void Interp5(unsigned char * pc, unsigned int c1, unsigned int c2);
bool Diff(unsigned int c1, unsigned int c2);
unsigned int RGBtoYUV(unsigned int c);
