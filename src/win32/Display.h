// -*- C++ -*-
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

#pragma once

enum DISPLAY_TYPE {
  DIRECT_DRAW = 0,
  DIRECT_3D = 1,
  OPENGL = 2
};

class IDisplay {
 public:
  IDisplay() {};
  virtual ~IDisplay() {};

  virtual bool initialize() = 0;
  virtual void cleanup() = 0;
  virtual void render() = 0;
  virtual void checkFullScreen() {};
  virtual void clear() = 0;
  virtual bool changeRenderSize(int w, int h) { return true; };
  virtual void resize(int w, int h) {};
  virtual void setOption(const char *option, int value) {};
  virtual DISPLAY_TYPE getType() = 0;
  virtual bool isSkinSupported() { return false; }
  virtual int selectFullScreenMode(GUID **) = 0;
};

inline void copyImage( void *source, void *destination, unsigned int width, unsigned int height, unsigned int destinationPitch, unsigned int colorDepth )
{
	// fast, iterative C version
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

	// compact but slow C version
	//unsigned int nBytesPerPixel = colorDepth>>3;
	//unsigned int i, x, y, srcPitch = (width+1) * nBytesPerPixel;
	//unsigned char * src = ((unsigned char*)source)+srcPitch;
	//unsigned char * dst = (unsigned char*)destination;
	//for (y=0;y<height;y++) //Width
		//for (x=0;x<width;x++) //Height
			//for (i=0;i<nBytesPerPixel;i++) //Byte# Of Pixel
				//*(dst+i+(x*nBytesPerPixel)+(y*destinationPitch)) = *(src+i+(x*nBytesPerPixel)+(y*srcPitch));
}
