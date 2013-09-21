#pragma once

#include <memory.h>

enum DISPLAY_TYPE {
  DIRECT_3D = 0,
  OPENGL = 1
};

class IDisplay {
 public:
  IDisplay() {};
  virtual ~IDisplay() {};

  struct VIDEO_MODE
  {
	  unsigned int adapter;
	  unsigned int width;
	  unsigned int height;
	  unsigned int bitDepth;
	  unsigned int frequency;
  };

  virtual bool initialize() = 0;
  virtual void cleanup() = 0;
  virtual void render() = 0;
  virtual void clear() = 0;
  virtual bool changeRenderSize(int w, int h) { return true; };
  virtual void resize(int w, int h) {};
  virtual void setOption(const char *option, int value) {};
  virtual DISPLAY_TYPE getType() = 0;
  virtual bool selectFullScreenMode( VIDEO_MODE &mode ) = 0;
};


inline void cpyImg32( unsigned char *dst, unsigned int dstPitch, unsigned char *src, unsigned int srcPitch, unsigned short width, unsigned short height )
{
	// fast, iterative C version
	// copies an width*height array of visible pixels from src to dst
	// srcPitch and dstPitch are the number of garbage bytes after a scanline
	register unsigned short lineSize = width<<2;

	while( height-- ) {
		memcpy( dst, src, lineSize );
		src += srcPitch;
		dst += dstPitch;
	}
}


inline void cpyImg32bmp( unsigned char *dst, unsigned char *src, unsigned int srcPitch, unsigned short width, unsigned short height )
{
	// dst will be an upside down bitmap with 24bit colors
	// pix must contain 32bit colors (XRGB)
	unsigned short srcLineSize = width<<2;
	dst += height * width * 3; // move to the last scanline
	register unsigned char r, g, b;

	while( height-- ) {
		unsigned short x = width;
		src += srcLineSize;
		while( x-- ) {
			--src; // ignore one of 4 bytes
			b = *--src;
			g = *--src;
			r = *--src;
			*--dst = b;
			*--dst = g;
			*--dst = r;
		}
		src += srcPitch;
	}
}


inline void cpyImg16( unsigned char *dst, unsigned int dstPitch, unsigned char *src, unsigned int srcPitch, unsigned short width, unsigned short height )
{
	register unsigned short lineSize = width<<1;

	while( height-- ) {
		memcpy( dst, src, lineSize );
		src += srcPitch;
		dst += dstPitch;
	}
}


inline void cpyImg16bmp( unsigned char *dst, unsigned char *src, unsigned int srcPitch, unsigned short width, unsigned short height )
{
	// dst will be an upside down bitmap with 16bit colors
	register unsigned short lineSize = width<<1;
	dst += ( height - 1 ) * lineSize; // move to the last scanline

	while( height-- ) {
		memcpy( dst, src, lineSize );
		src += srcPitch;
		dst -= lineSize;
	}
}
