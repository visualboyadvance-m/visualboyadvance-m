#include "stdafx.h"
#include "VBA.h"
#include "rpi.h"

extern void SuperEagle(u8*,u32,u8*,u8*,u32,int,int);

static HINSTANCE rpiDLL = NULL;
static RENDPLUG_Output fnOutput = NULL;
static RENDPLUG_GetInfo fnGetInfo = NULL;
static RENDER_PLUGIN_INFO MyPlugInfo;
static RENDER_PLUGIN_OUTP MyPlugOutput;
static int nScaleFactor;

extern int systemRedShift, systemGreenShift, systemBlueShift;
extern int realsystemRedShift, realsystemGreenShift, realsystemBlueShift;
extern int realsystemColorDepth;
u8 *pBuffer16 = NULL;
u32 Buffer16Size = 0;

bool rpiInit(const char *sPluginName)
{
	rpiCleanup();

	char sBuffer[256];
	char *ptr;

	GetModuleFileName(NULL, sBuffer, sizeof(sBuffer));
	ptr = strrchr(sBuffer, '\\');
	if (ptr)
		*ptr = '\0';
	strcat(sBuffer, "\\plugins\\");
	strcat(sBuffer, sPluginName);

  	rpiDLL = LoadLibrary(sBuffer);
  	if (!rpiDLL)
  		return false;

	fnGetInfo = (RENDPLUG_GetInfo) GetProcAddress(rpiDLL, "RenderPluginGetInfo");
	fnOutput = (RENDPLUG_Output) GetProcAddress(rpiDLL, "RenderPluginOutput");
	if (fnGetInfo == NULL || fnOutput == NULL)
	{
		FreeLibrary(rpiDLL);
		rpiDLL = NULL;
		return false;
	}

	RENDER_PLUGIN_INFO *pRPI = fnGetInfo();
	if (pRPI == NULL)
	{
		FreeLibrary(rpiDLL);
		rpiDLL = NULL;
		return false;
	}

	memcpy(&MyPlugInfo, pRPI, sizeof(MyPlugInfo));

	unsigned long Flags = MyPlugInfo.Flags & 0x0000F0000;

	if (Flags == RPI_OUT_SCL2)
	{
		nScaleFactor = 2;
	}
	else if (Flags == RPI_OUT_SCL3)
	{
		nScaleFactor = 3;
	}
	else if (Flags == RPI_OUT_SCL4)
	{
		nScaleFactor = 4;
	}
	else
	{
		nScaleFactor = 2;
	}

	return true;
}

void rpiFilter(u8 *srcPtr, u32 srcPitch, u8 *deltaPtr, u8 *dstPtr, u32 dstPitch, int width, int height)
{
	u8 *pBuff;

	if (realsystemColorDepth == 32)
	{
		// Kega filters are 16 bit only.  Assumes we've forced 16 bit input
		ASSERT(systemColorDepth == 16);
		u32 bufferNeeded = dstPitch * (height + nScaleFactor) * nScaleFactor;
		if (Buffer16Size < bufferNeeded)
		{
			Buffer16Size = bufferNeeded;
			if (pBuffer16)
				free(pBuffer16);
			pBuffer16 =  (u8 *)malloc(Buffer16Size);
		}
		pBuff = pBuffer16;
	}
	else
		pBuff = dstPtr;

	MyPlugOutput.Size = sizeof(MyPlugOutput);
	MyPlugOutput.Flags = MyPlugInfo.Flags;
	MyPlugOutput.SrcPtr = srcPtr;
	MyPlugOutput.SrcPitch = srcPitch;
	MyPlugOutput.SrcW = width;
	// Without this funky math on the height value, the RPI filter isn't fully
	// rendering the frame.  I don't like passing in values that seem
	// to be greater than the buffer size, but it's the only way to get
	// proper results.
	MyPlugOutput.SrcH = height+(nScaleFactor/2);
	MyPlugOutput.DstPtr = pBuff;
	MyPlugOutput.DstPitch = dstPitch;
	MyPlugOutput.DstW = width * nScaleFactor;
	MyPlugOutput.DstH = (height+(nScaleFactor/2)) * nScaleFactor;
	MyPlugOutput.OutW = width * nScaleFactor;
	MyPlugOutput.OutH = (height+(nScaleFactor/2)) * nScaleFactor;

	fnOutput(&MyPlugOutput);

	if (realsystemColorDepth == 32)
	{
		register int i,j;
		int rshiftDiff = realsystemRedShift - systemRedShift;
		int gshiftDiff = realsystemGreenShift - systemGreenShift;
		int bshiftDiff = realsystemBlueShift - systemBlueShift;

		u16 *pI, *pICur;
		u32 *pO, *pOCur;

		pI = pICur = (u16 *)pBuff;
		pO = pOCur = (u32 *)dstPtr;

		if (rshiftDiff >= 0)
		{
			for(j=0;j<height*nScaleFactor;j++)
			{
				for(i=0;i<width*nScaleFactor;i++)
				{
					*(pOCur++) = ((*pICur & 0xF800) << rshiftDiff) |
						      ((*pICur & 0x07E0) << gshiftDiff) |
						      ((*pICur & 0x001F) << bshiftDiff);
					*(pICur++);
				}
				pI = pICur = (u16 *)((char *)pI + dstPitch);
				pO = pOCur = (u32 *)((char *)pO + dstPitch);
			}
		}
		else
		{
			// red shift is negative.  That means we're most likely swapping RGB to BGR
			// shift operators don't support negative values.

			rshiftDiff = -rshiftDiff;
			for(j=0;j<height*nScaleFactor;j++)
			{
				for(i=0;i<width*nScaleFactor;i++)
				{
					*(pOCur++) = ((*pICur & 0xF800) >> rshiftDiff) |
						      ((*pICur & 0x07E0) << gshiftDiff) |
						      ((*pICur & 0x001F) << bshiftDiff);
					*(pICur++);
				}
				pI = pICur = (u16 *)((char *)pI + dstPitch);
				pO = pOCur = (u32 *)((char *)pO + dstPitch);
			}

		}
	}
}

int rpiScaleFactor()
{
	return nScaleFactor;
}

void rpiCleanup()
{
	if (rpiDLL != NULL)
	{
		FreeLibrary(rpiDLL);
		rpiDLL = NULL;
	}
	if (pBuffer16)
	{
		free(pBuffer16);
		pBuffer16 = NULL;
		Buffer16Size = 0;
	}
}
