#pragma once

//---------------------------------------------------------------------------------------------------------------------------
// hq2x plugin example - Steve Snake 2004.
// This plugin uses (modified) code by Maxim Stepin - see "hq2x16.asm" for info
// The original code and description of the algorithm can be found at:
// http://www.hiend3d.com/hq2x.html
// Modified by suanyuan
//---------------------------------------------------------------------------------------------------------------------------
#if defined(_WIN32)
#include	<windows.h>
#else
#define HMODULE void *
#endif

//---------------------------------------------------------------------------------------------------------------------------
typedef struct
{
	unsigned long	Size;
	unsigned long	Flags;
	void			*SrcPtr;
	unsigned long	SrcPitch;
	unsigned long	SrcW;
	unsigned long	SrcH;
	void			*DstPtr;
	unsigned long	DstPitch;
	unsigned long	DstW;
	unsigned long	DstH;
	unsigned long	OutW;
	unsigned long	OutH;
} RENDER_PLUGIN_OUTP;

//---------------------------------------------------------------------------------------------------------------------------

typedef	void	(*RENDPLUG_Output)(RENDER_PLUGIN_OUTP *);

//---------------------------------------------------------------------------------------------------------------------------

typedef struct
{
	char			Name[60];
	unsigned long	Flags;
	HMODULE			Handle;
	RENDPLUG_Output	Output;
} RENDER_PLUGIN_INFO;

//---------------------------------------------------------------------------------------------------------------------------

typedef	RENDER_PLUGIN_INFO	*(*RENDPLUG_GetInfo)(void);

//---------------------------------------------------------------------------------------------------------------------------

#define	RPI_VERSION		0x02

#define	RPI_MMX_USED	0x000000100
#define	RPI_MMX_REQD	0x000000200
#define	RPI_555_SUPP	0x000000400
#define	RPI_565_SUPP	0x000000800
#define	RPI_888_SUPP	0x000001000

#define RPI_DST_WIDE	0x000008000

#define RPI_OUT_SCL1	0x000010000
#define	RPI_OUT_SCL2	0x000020000
#define	RPI_OUT_SCL3	0x000030000
#define	RPI_OUT_SCL4	0x000040000

#define RPI_OUT_SCLMSK	0x0000f0000
#define RPI_OUT_SCLSH	16

//---------------------------------------------------------------------------------------------------------------------------

int rpiScaleFactor();
bool rpiInit(const char *sPluginName);
void rpiFilter(u8 *srcPtr, u32 srcPitch, u8 *deltaPtr, u8 *dstPtr, u32 dstPitch, int width, int height);
void rpiCleanup();
