#include "stdafx.h"
#include "rpi.h"

extern void SuperEagle(u8*,u32,u8*,u8*,u32,int,int);

static HINSTANCE rpiDLL = NULL;
static RENDPLUG_Output fnOutput = NULL;
static RENDPLUG_GetInfo fnGetInfo = NULL;
static RENDER_PLUGIN_INFO MyPlugInfo;
static RENDER_PLUGIN_OUTP MyPlugOutput;
static int nScaleFactor;

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
	MyPlugOutput.Size = sizeof(MyPlugOutput);
	MyPlugOutput.Flags = MyPlugInfo.Flags;
	MyPlugOutput.SrcPtr = srcPtr;
	MyPlugOutput.SrcPitch = srcPitch;
	MyPlugOutput.SrcW = width;
	MyPlugOutput.SrcH = height;
	MyPlugOutput.DstPtr = dstPtr;
	MyPlugOutput.DstPitch = dstPitch;
	MyPlugOutput.DstW = width * nScaleFactor;
	MyPlugOutput.DstH = height * nScaleFactor;
	MyPlugOutput.OutW = width * nScaleFactor;
	MyPlugOutput.OutH = height * nScaleFactor;

	fnOutput(&MyPlugOutput);
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
}
