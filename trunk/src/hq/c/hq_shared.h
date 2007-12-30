/*
    VisualBoyAdvance - a Game Boy & Game Boy Advance emulator

    Copyright (C) 2008 VBA-M development team


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

	hq filter by Maxim Stepin ( http://hiend3d.com )
*/


// we only need the 32bit version because our YUV format has 32bits
#define abs_32( value )  ( value & 0x7FFFFFFF )


inline bool Diff( unsigned int YUV1, unsigned int YUV2 )
{
	if( YUV1 == YUV2 ) return false; // Save some processing power

	return
		( abs_32((YUV1 & 0x00FF0000) - (YUV2 & 0x00FF0000)) > 0x00300000 ) ||
		( abs_32((YUV1 & 0x0000FF00) - (YUV2 & 0x0000FF00)) > 0x00000700 ) ||
		( abs_32((YUV1 & 0x000000FF) - (YUV2 & 0x000000FF)) > 0x00000006 );
}



// ===============
// 32bit routines:
// ===============

// ( c1*3 + c2 ) / 4
// hq3x, hq4x
#define Interp1_32( pc, c1, c2 ) \
( \
	*( (unsigned int *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x00FF00 ) * 3 ) + \
			( (c2) & 0x00FF00 ) \
		) & 0x0003FC00 ) \
		+ \
		( ( \
			( ( (c1) & 0xFF00FF ) * 3 ) + \
			( (c2) & 0xFF00FF ) \
		) & 0x03FC03FC ) \
	) >> 2 \
)


// ( c1*2 + c2 + c3 ) / 4
// hq3x, hq4x
#define Interp2_32( pc, c1, c2, c3 ) \
( \
	*( (unsigned int *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x00FF00 ) * 2 ) + \
			( (c2) & 0x00FF00 ) + \
			( (c3) & 0x00FF00 ) \
		) & 0x0003FC00 ) \
		+ \
		( ( \
			( ( (c1) & 0xFF00FF ) * 2 ) + \
			( (c2) & 0xFF00FF ) + \
			( (c3) & 0xFF00FF ) \
		) & 0x03FC03FC ) \
	) >> 2 \
)


// ( c1*7 + c2 ) / 8
// hq3x, hq4x
#define Interp3_32( pc, c1, c2 ) \
( \
	*( (unsigned int *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x00FF00 ) * 7 ) + \
			( (c2) & 0x00FF00 ) \
		) & 0x0007F800 ) \
		+ \
		( ( \
			( ( (c1) & 0xFF00FF ) * 7 ) + \
			( (c2) & 0xFF00FF ) \
		) & 0x07F807F8 ) \
	) >> 3 \
)


// ( c1*2 + (c2+c3)*7 ) / 16
// hq3x, not used by hq4x
#define Interp4_32( pc, c1, c2, c3 ) \
( \
	*( (unsigned int *)(pc) ) = \
	( \
	( ( ( ( (c1) & 0x00FF00 ) * 2 ) + ( ( ( (c2) & 0x00FF00 ) + ( (c3) & 0x00FF00 ) ) * 7 ) ) & 0x000FF000 ) + \
	( ( ( ( (c1) & 0xFF00FF ) * 2 ) + ( ( ( (c2) & 0xFF00FF ) + ( (c3) & 0xFF00FF ) ) * 7 ) ) & 0x0FF00FF0 ) \
	) >> 4 \
)


// ( c1 + c2 ) / 2
// hq3x, hq4x
#define Interp5_32( pc, c1, c2 ) \
( \
	*( (unsigned int *)(pc) ) = \
	( \
		( ( \
			( (c1) & 0x00FF00 ) + \
			( (c2) & 0x00FF00 ) \
		) & 0x0001FE00 ) \
		+ \
		( ( \
			( (c1) & 0xFF00FF ) + \
			( (c2) & 0xFF00FF ) \
		) & 0x01FE01FE ) \
	) >> 1 \
)


// ( c1*5 + c2*2 + c3 ) / 8
// hq4x
#define Interp6_32( pc, c1, c2, c3 ) \
( \
	*( (unsigned int *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x00FF00 ) * 5 ) + \
			( ( (c2) & 0x00FF00 ) * 2 ) + \
			( (c3) & 0x00FF00 ) \
		) & 0x0007F800 ) \
		+ \
		( ( \
			( ( (c1) & 0xFF00FF ) * 5 ) + \
			( ( (c2) & 0xFF00FF ) * 2 ) + \
			( (c3) & 0xFF00FF ) \
		) & 0x07F807F8 ) \
	) >> 3 \
)


// ( c1*6 + c2 + c3 ) / 8
// hq4x
#define Interp7_32( pc, c1, c2, c3 ) \
( \
	*( (unsigned int *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x00FF00 ) * 6 ) + \
			( (c2) & 0x00FF00 ) + \
			( (c3) & 0x00FF00 ) \
		) & 0x0007F800 ) \
		+ \
		( ( \
			( ( (c1) & 0xFF00FF ) * 6 ) + \
			( (c2) & 0xFF00FF ) + \
			( (c3) & 0xFF00FF ) \
		) & 0x07F807F8 ) \
	) >> 3 \
)


// ( c1*5 + c2*3 ) / 8
// hq4x
#define Interp8_32( pc, c1, c2 ) \
( \
	*( (unsigned int *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x00FF00 ) * 5 ) + \
			( ( (c2) & 0x00FF00 ) * 3 ) \
		) & 0x0007F800 ) \
		+ \
		( ( \
			( ( (c1) & 0xFF00FF ) * 5 ) + \
			( ( (c2) & 0xFF00FF ) * 3 ) \
		) & 0x07F807F8 ) \
	) >> 3 \
)


// 32 bit input color
// 0x00YYUUVV return value
inline unsigned int RGBtoYUV_32( unsigned int c )
{
	// Division through 3 slows down the emulation about 10% !!!

	register unsigned char r, g, b;
	r = ( c & 0x000000FF );
	g = ( c & 0x0000FF00 ) >> 8;
	b = ( c & 0x00FF0000 ) >> 16;
	return ( (r + g + b) << 14 ) +
		( ( r - b + 512 ) << 4 ) +
		( ( 2*g - r - b ) >> 3 ) + 128;

	// unoptimized:
	//unsigned char r, g, b, Y, u, v;
	//r = (c & 0x000000FF);
	//g = (c & 0x0000FF00) >> 8;
	//b = (c & 0x00FF0000) >> 16;
	//Y = (r + g + b) >> 2;
	//u = 128 + ((r - b) >> 2);
	//v = 128 + ((-r + 2*g -b)>>3);
	//return (Y<<16) + (u<<8) + v;
}



// ===============
// 16bit routines:
// ===============

// ( c1*3 + c2 ) / 4
// hq3x, hq4x
#define Interp1_16( pc, c1, c2 ) \
( \
	*( (unsigned short *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x07E0 ) * 3 ) + \
			( (c2) & 0x07E0 ) \
		) & 0x00001F80 ) \
		+ \
		( ( \
			( ( (c1) & 0xF81F ) * 3 ) + \
			( (c2) & 0xF81F ) \
		) & 0x0003E07C ) \
	) >> 2 \
)


// ( c1*2 + c2 + c3 ) / 4
// hq3x, hq4x
#define Interp2_16( pc, c1, c2, c3 ) \
( \
	*( (unsigned short *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x07E0 ) * 2 ) + \
			( (c2) & 0x07E0 ) + \
			( (c3) & 0x07E0 ) \
		) & 0x00001F80 ) \
		+ \
		( ( \
			( ( (c1) & 0xF81F ) * 2 ) + \
			( (c2) & 0xF81F ) + \
			( (c3) & 0xF81F ) \
		) & 0x0003E07C ) \
	) >> 2 \
)


// ( c1*7 + c2 ) / 8
// hq3x, hq4x
#define Interp3_16( pc, c1, c2 ) \
( \
	*( (unsigned short *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x07E0 ) * 7 ) + \
			( (c2) & 0x07E0 ) \
		) & 0x00003F00 ) \
		+ \
		( ( \
			( ( (c1) & 0xF81F ) * 7 ) + \
			( (c2) & 0xF81F ) \
		) & 0x0007C0F8 ) \
	) >> 3 \
)


// ( c1*2 + (c2+c3)*7 ) / 16
// hq3x, not used by hq4x
#define Interp4_16( pc, c1, c2, c3 ) \
( \
	*( (unsigned short *)(pc) ) = \
	( \
	( ( ( ( (c1) & 0x07E0 ) * 2 ) + ( ( ( (c2) & 0x07E0 ) + ( (c3) & 0x07E0 ) ) * 7 ) ) & 0x00007E00 ) + \
	( ( ( ( (c1) & 0xF81F ) * 2 ) + ( ( ( (c2) & 0xF81F ) + ( (c3) & 0xF81F ) ) * 7 ) ) & 0x000F81F0 ) \
	) >> 4 \
)


// ( c1 + c2 ) / 2
// hq3x, hq4x
#define Interp5_16( pc, c1, c2 ) \
( \
	*( (unsigned short *)(pc) ) = \
	( \
		( ( \
			( (c1) & 0x07E0 ) + \
			( (c2) & 0x07E0 ) \
		) & 0x00000FC0 ) \
		+ \
		( ( \
			( (c1) & 0xF81F ) + \
			( (c2) & 0xF81F ) \
		) & 0x0001F03E ) \
	) >> 1 \
)


// ( c1*5 + c2*2 + c3 ) / 8
// hq4x
#define Interp6_16( pc, c1, c2, c3 ) \
( \
	*( (unsigned short *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x07E0 ) * 5 ) + \
			( ( (c2) & 0x07E0 ) * 2 ) + \
			( (c3) & 0x07E0 ) \
		) & 0x00003F00 ) \
		+ \
		( ( \
			( ( (c1) & 0xF81F ) * 5 ) + \
			( ( (c2) & 0xF81F ) * 2 ) + \
			( (c3) & 0xF81F ) \
		) & 0x0007C0F8 ) \
	) >> 3 \
)


// ( c1*6 + c2 + c3 ) / 8
// hq4x
#define Interp7_16( pc, c1, c2, c3 ) \
( \
	*( (unsigned short *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x07E0 ) * 6 ) + \
			( (c2) & 0x07E0 ) + \
			( (c3) & 0x07E0 ) \
		) & 0x00003F00 ) \
		+ \
		( ( \
			( ( (c1) & 0xF81F ) * 6 ) + \
			( (c2) & 0xF81F ) + \
			( (c3) & 0xF81F ) \
		) & 0x0007C0F8 ) \
	) >> 3 \
)


// ( c1*5 + c2*3 ) / 8
// hq4x
#define Interp8_16( pc, c1, c2 ) \
( \
	*( (unsigned short *)(pc) ) = \
	( \
		( ( \
			( ( (c1) & 0x07E0 ) * 5 ) + \
			( ( (c2) & 0x07E0 ) * 3 ) \
		) & 0x00003F00 ) \
		+ \
		( ( \
			( ( (c1) & 0xF81F ) * 5 ) + \
			( ( (c2) & 0xF81F ) * 3 ) \
		) & 0x0007C0F8 ) \
	) >> 3 \
)


// 16 bit input color
// 0x00YYUUVV return value
inline unsigned int RGBtoYUV_16( unsigned short c )
{
	// Division through 3 slows down the emulation about 10% !!!

	register unsigned char r, g, b;
	r = ( c & 0xF800 ) >> 8;
	g = ( c & 0x07E0 ) >> 3;
	b = ( c & 0x001F ) << 3;

	return ( (r + g + b) << 14 ) +
		( ( r - b + 512 ) << 4 ) +
		( ( 2*g - r - b ) >> 3 ) + 128;
}
