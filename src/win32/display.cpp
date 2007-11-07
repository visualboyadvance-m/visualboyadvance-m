// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

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

#include "stdafx.h"
#include "Display.h"

void copyImage( void *source, void *destination, unsigned int width, unsigned int height, unsigned int destinationPitch, unsigned int colorDepth )
{
#ifdef ASM

	// Copy the image at [source] to the locked Direct3D texture
	__asm
	{
		mov eax, width            ; Initialize
		mov ebx, height           ;
		mov edi, destination      ;
		mov edx, destinationPitch ;

		cmp colorDepth, 16       ; Check colorDepth==16bit
		jnz gbaOtherColor        ;
		sub edx, eax             ;
		sub edx, eax             ;
		mov esi, source          ;
		lea esi,[esi+2*eax+4]    ;
		shr eax, 1               ;
gbaLoop16bit:
		mov ecx, eax     ;
		rep movsd        ;
		add esi, 4       ;
		add edi, edx     ;
		dec ebx          ;
		jnz gbaLoop16bit ;
		jmp gbaLoopEnd   ;
gbaOtherColor:
		cmp colorDepth, 32     ; Check colorDepth==32bit
		jnz gbaOtherColor2     ;

		lea esi, [eax*4]       ;
		sub edx, esi           ;
		mov esi, source        ;
		lea esi, [esi+4*eax+4] ;
gbaLoop32bit:
		mov ecx, eax     ;
		rep movsd        ; ECX times: Move DWORD at [ESI] to [EDI] | ESI++ EDI++
		add esi, 4       ;
		add edi, edx     ;
		dec ebx          ;
		jnz gbaLoop32bit ;
		jmp gbaLoopEnd   ;
gbaOtherColor2:
		lea eax, [eax+2*eax] ; Works like colorDepth==24bit
		sub edx, eax         ;
gbaLoop24bit:
		mov ecx, eax     ;
		shr ecx, 2       ;
		rep movsd        ;
		add edi, edx     ;
		dec ebx          ;
		jnz gbaLoop24bit ;
gbaLoopEnd:
	}

#else // #ifdef ASM

	// optimized C version
	register unsigned int lineSize;
	register unsigned char *src, *dst;
	switch(colorDepth)
	{
	case 16:
		lineSize = width<<1;
		src = ((unsigned char*)source) + lineSize + 4;
		dst = (unsigned char*)destination;
		do {
			MoveMemory( dst, src, lineSize );
				src+=lineSize;
				dst+=lineSize;
			src += 2;
			dst += (destinationPitch - lineSize);
		} while ( --height);
		break;
	case 32:
		lineSize = width<<2;
		src = ((unsigned char*)source) + lineSize + 4;
		dst = (unsigned char*)destination;
		do {
			MoveMemory( dst, src, lineSize );
				src+=lineSize;
				dst+=lineSize;
			src += 4;
			dst += (destinationPitch - lineSize);
		} while ( --height);
		break;
	}

	// very compatible but slow C version
	//unsigned int nBytesPerPixel = colorDepth>>3;
	//unsigned int i, x, y, srcPitch = (width+1) * nBytesPerPixel;
	//unsigned char * src = ((unsigned char*)source)+srcPitch;
	//unsigned char * dst = (unsigned char*)destination;
	//for (y=0;y<height;y++) //Width
		//for (x=0;x<width;x++) //Height
			//for (i=0;i<nBytesPerPixel;i++) //Byte# Of Pixel
				//*(dst+i+(x*nBytesPerPixel)+(y*destinationPitch)) = *(src+i+(x*nBytesPerPixel)+(y*srcPitch));

#endif
}

