#define __STDC_CONSTANT_MACROS

#include <stdint.h>

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

//16 bit input, see below for 32 bit input
void hq3x32(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char *,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres)
{
	if (!hq3xinited)
	{
		InitLUTs();
		hq3xinited=1;
	}
	hq3x_32( pIn, pOut, Xres, Yres, dstPitch, srcPitch - (Xres *2) );
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

//16 bit input, see below for 32 bit input
void hq4x32(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char *,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres)
{
	if (!hq3xinited)
	{
		InitLUTs();
		hq3xinited=1;
	}
	hq4x_32( pIn, pOut, Xres, Yres, dstPitch, srcPitch - (Xres *2));
}

static inline void convert32bpp_16bpp(unsigned char *pIn, unsigned int width)
{
  for (unsigned int i = 0; i < width; i+=4)
  {
    unsigned int p4 = ((unsigned int)pIn[i+2] << 16) | (unsigned int) (pIn[i+1] << 8) | pIn[i+0];
    unsigned short p2 = ((p4 >> 8)&0xF800) | ((p4 >> 5)&0x07E0) | ((p4 >> 3)&0x001F);
    pIn[i/2] = (p2 >> 0);
    pIn[i/2+1] = (p2 >> 8);
  }
}

void hq3x32_32(unsigned char *pIn,  unsigned int srcPitch, unsigned char *, unsigned char *pOut, unsigned int dstPitch, int Xres, int Yres)
{
  convert32bpp_16bpp(pIn, srcPitch*Yres);
  hq3x32(pIn, srcPitch/2, 0, pOut, dstPitch, Xres, Yres);
}

void hq4x32_32(unsigned char *pIn,  unsigned int srcPitch, unsigned char *, unsigned char *pOut, unsigned int dstPitch, int Xres, int Yres)
{
  convert32bpp_16bpp(pIn, srcPitch*Yres);
  hq4x32(pIn, srcPitch/2, 0, pOut, dstPitch, Xres, Yres);
}
