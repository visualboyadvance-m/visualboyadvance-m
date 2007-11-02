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

#include "stdafx.h"
#include <memory.h>
#include "VBA.H"
#include "MainWnd.h"
#include "UniVideoModeDlg.h"
#include "../Util.h"
#include "../Globals.h"
#include "../Util.h"
#include "../gb/gbGlobals.h"
// Link with Direct3D9
#pragma comment(lib, "D3d9.lib")
#pragma comment(lib, "D3dx9.lib")
#define DIRECT3D_VERSION 0x0900
#include <D3d9.h>
#include <D3dx9core.h>

#include "../gbafilter.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifdef MMX
extern "C" bool cpu_mmx;
extern bool detectMMX();
#endif

extern int Init_2xSaI(u32);
extern void winlog(const char *,...);
extern int systemSpeed;


// Vertex format declarations
const DWORD D3DFVF_TEXTBOXVERTEX = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
struct TEXTBOXVERTEX {
	FLOAT		x, y, z, rhw;
	D3DCOLOR	color;
};
const DWORD D3DFVF_IMAGEVERTEX = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
struct IMAGEVERTEX {
	FLOAT    x, y, z;
	D3DCOLOR color;
	FLOAT    u, v;
};



class Direct3DDisplay : public IDisplay
{
public: // Class
	Direct3DDisplay();
	virtual ~Direct3DDisplay();


public: // Interface
	virtual bool initialize();
	virtual void cleanup();
	virtual void render();
	virtual void renderMenu();
	virtual void clear();
	virtual bool changeRenderSize(int w, int h);
	virtual void resize(int w, int h);
	virtual DISPLAY_TYPE getType() { return DIRECT_3D; };
	virtual void setOption(const char *, int);
	virtual int selectFullScreenMode(GUID **);
	virtual int selectFullScreenMode2();


private: // Functions
	void restoreDeviceObjects(void);
	void invalidateDeviceObjects();
	void setPresentationType();
	bool initializeOffscreen(unsigned int w, unsigned int h);
	void updateFiltering(int);
	bool resetDevice();
	void initializeMatrices();


private: // Variables
	int                     SelectedFreq, SelectedAdapter;
	bool                    initSucessful;
	bool                    doNotRender;
	bool                    filterDisabled;
	bool					lockableBuffer;
	LPDIRECT3D9             pD3D;
	LPDIRECT3DDEVICE9       pDevice;
	LPDIRECT3DTEXTURE9      pTexture;
	LPD3DXFONT              pFont;
	D3DPRESENT_PARAMETERS   dpp;
	D3DFORMAT               screenFormat;
    D3DDISPLAYMODE			mode;

	bool                    fullscreen;
	int                     width, height; // Size of the source image to display
	IMAGEVERTEX             verts[4]; // The coordinates for our image texture
	TEXTBOXVERTEX           msgBox[4];
	int                     textureWidth;  // Size of the texture,
	int                     textureHeight; // where the source image is copied to
	bool                    keepAspectRatio;
};

Direct3DDisplay::Direct3DDisplay()
{
	initSucessful = false;
	doNotRender = true;
	pD3D = NULL;  
	pDevice = NULL;
	pTexture = NULL;
	pFont = NULL;
	screenFormat = D3DFMT_UNKNOWN;
	width = 0;
	height = 0;
	filterDisabled = false;
	keepAspectRatio = false; // theApp.d3dKeepAspectRatio;
	lockableBuffer = false;
}

Direct3DDisplay::~Direct3DDisplay()
{
	cleanup();
}

void Direct3DDisplay::setPresentationType()
{
	// Change display mode
	memset(&dpp, 0, sizeof(dpp));
	dpp.Windowed = !fullscreen;
	if (fullscreen)
		dpp.BackBufferFormat =
		(theApp.fsColorDepth == 32) ? D3DFMT_X8R8G8B8 : D3DFMT_R5G6B5;
	else
		dpp.BackBufferFormat = mode.Format;
	dpp.BackBufferCount = 3;
	dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
	dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	dpp.BackBufferWidth = fullscreen ? theApp.fsWidth : theApp.surfaceSizeX;
	dpp.BackBufferHeight = fullscreen ? theApp.fsHeight : theApp.surfaceSizeY;
	dpp.hDeviceWindow = theApp.m_pMainWnd->GetSafeHwnd();
	dpp.FullScreen_RefreshRateInHz = fullscreen ? theApp.fsFrequency : 0;
//	dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	dpp.Flags = theApp.menuToggle ? D3DPRESENTFLAG_LOCKABLE_BACKBUFFER : 0;
	if (theApp.vsync)
		dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;			// VSync
	else
		dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;	// No Sync
}

void Direct3DDisplay::cleanup()
{ // interface funtion
	if(pD3D != NULL) {
		if(pFont) {
			pFont->Release();
			pFont = NULL;
		}
		
		if(pTexture)
		{
			pTexture->Release();
			pTexture = NULL;
		}

		if(pDevice) {
			pDevice->Release();
			pDevice = NULL;
		}

		pD3D->Release();
		pD3D = NULL;
	}
	
	initSucessful = false;
	doNotRender = true;
}

bool Direct3DDisplay::initialize()
{ // interface function
	initSucessful = false;
	doNotRender = true;

	// Get emulated image's dimensions
	switch(theApp.cartridgeType)
	{
	case IMAGE_GBA:
		theApp.sizeX = 240;
		theApp.sizeY = 160;
		break;
	case IMAGE_GB:
		if (gbBorderOn)
		{
			theApp.sizeX = 256;
			theApp.sizeY = 224;
		}
		else
		{
			theApp.sizeX = 160;
			theApp.sizeY = 144;
		}
		break;
	}
	
	theApp.rect.left = 0;
	theApp.rect.top = 0;
	theApp.rect.right = theApp.sizeX;
	theApp.rect.bottom = theApp.sizeY;

  
	switch(theApp.videoOption)
	{
	case VIDEO_1X:
		theApp.surfaceSizeX = theApp.sizeX;
		theApp.surfaceSizeY = theApp.sizeY;
		fullscreen = false;
		break;
	case VIDEO_2X:
		theApp.surfaceSizeX = theApp.sizeX * 2;
		theApp.surfaceSizeY = theApp.sizeY * 2;
		fullscreen = false;
		break;
	case VIDEO_3X:
		theApp.surfaceSizeX = theApp.sizeX * 3;
		theApp.surfaceSizeY = theApp.sizeY * 3;
		fullscreen = false;
		break;
	case VIDEO_4X:
		theApp.surfaceSizeX = theApp.sizeX * 4;
		theApp.surfaceSizeY = theApp.sizeY * 4;
		fullscreen = false;
		break;
	case VIDEO_320x240:
	case VIDEO_640x480:
	case VIDEO_800x600:
	case VIDEO_1024x768:
	case VIDEO_1280x1024:
	case VIDEO_OTHER:
			float scaleX = ((float)theApp.fsWidth / theApp.sizeX);
			float scaleY = ((float)theApp.fsHeight / theApp.sizeY);
			float min = scaleX < scaleY ? scaleX : scaleY;
			if(theApp.fsMaxScale)
				min = min > theApp.fsMaxScale ? theApp.fsMaxScale : min;
			if(theApp.fullScreenStretch)
			{
				theApp.surfaceSizeX = theApp.fsWidth;
				theApp.surfaceSizeY = theApp.fsHeight;
			}
			else
			{
				theApp.surfaceSizeX = (int)(theApp.sizeX * min);
				theApp.surfaceSizeY = (int)(theApp.sizeY * min);
			}
			fullscreen = true;
			break;
	}


  theApp.dest.left = 0;
  theApp.dest.top = 0;
  theApp.dest.right = theApp.surfaceSizeX;
  theApp.dest.bottom = theApp.surfaceSizeY;

  DWORD style = WS_POPUP | WS_VISIBLE;
  DWORD styleEx = 0;
  
  if(theApp.videoOption <= VIDEO_4X)
    style |= WS_OVERLAPPEDWINDOW;
  else
    styleEx = 0;

  if(theApp.videoOption <= VIDEO_4X)
    AdjustWindowRectEx(&theApp.dest, style, TRUE, styleEx);
  else
    AdjustWindowRectEx(&theApp.dest, style, FALSE, styleEx);    

  int winSizeX = theApp.dest.right-theApp.dest.left;
  int winSizeY = theApp.dest.bottom-theApp.dest.top;

  if(theApp.videoOption > VIDEO_4X) {
    winSizeX = theApp.fsWidth;
    winSizeY = theApp.fsHeight;
  }
  int x = 0;
  int y = 0;

  if(theApp.videoOption <= VIDEO_4X) {
    x = theApp.windowPositionX;
    y = theApp.windowPositionY;
  }
  
	// Create a window
  MainWnd *pWnd = new MainWnd;
  theApp.m_pMainWnd = pWnd;

	pWnd->CreateEx(
		styleEx,
		theApp.wndClass,
		"VisualBoyAdvance",
		style,
		x, y,
		winSizeX, winSizeY,
		NULL,
		0);
	
	if (!(HWND)*pWnd)
	{
		winlog("Error creating Window %08x\n", GetLastError());
		return FALSE;
	}

  theApp.updateMenuBar();
  
  theApp.adjustDestRect();

	
	// Create an IDirect3D9 object
	pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if(pD3D == NULL)
	{
		winlog("Error creating Direct3D object\n");
		return FALSE;
	}




	// Display resolution
  pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);
  
  switch(mode.Format) {
  case D3DFMT_R8G8B8:
    systemColorDepth = 24;
    systemRedShift = 19;
    systemGreenShift = 11;
    systemBlueShift = 3;
	Init_2xSaI(32);
    break;
  case D3DFMT_X8R8G8B8:
    systemColorDepth = 32;
    systemRedShift = 19;
    systemGreenShift = 11;
    systemBlueShift = 3;
    Init_2xSaI(32);
    break;
  case D3DFMT_R5G6B5:
    systemColorDepth = 16;
    systemRedShift = 11;
    systemGreenShift = 6;
    systemBlueShift = 0;    
    Init_2xSaI(565);
    break;
  case D3DFMT_X1R5G5B5:
    systemColorDepth = 16;
    systemRedShift = 10;
    systemGreenShift = 5;
    systemBlueShift = 0;
    Init_2xSaI(555);
    break;
  default:
    systemMessage(0,"Unsupported D3D format %d", mode.Format);
    return false;
  }
  theApp.fsColorDepth = systemColorDepth;

	// Check the available pre-defined fullscreen modes and enable menu items
	unsigned int nModes, i;
	D3DDISPLAYMODE dm;

	theApp.mode320Available = false;
	theApp.mode640Available = false;
	theApp.mode800Available = false;
	theApp.mode1024Available = false;
	theApp.mode1280Available = false;

	nModes = pD3D->GetAdapterModeCount(theApp.fsAdapter, mode.Format);
	for (i = 0; i<nModes; i++)
	{
		if (D3D_OK == pD3D->EnumAdapterModes(theApp.fsAdapter, mode.Format, i, &dm) )
		{
			if ( (dm.Width == 320) && (dm.Height == 240) )
				theApp.mode320Available = true;
			if ( (dm.Width == 640) && (dm.Height == 480) )
				theApp.mode640Available = true;
			if ( (dm.Width == 800) && (dm.Height == 600) )
				theApp.mode800Available = true;
			if ( (dm.Width == 1024) && (dm.Height == 768) )
				theApp.mode1024Available = true;
			if ( (dm.Width == 1280) && (dm.Height == 1024) )
				theApp.mode1280Available = true;
		}
	}


#ifdef MMX
	if (!theApp.disableMMX)
		cpu_mmx = theApp.detectMMX();
	else
		cpu_mmx = 0;
#endif

	screenFormat = mode.Format;

	setPresentationType();


	DWORD BehaviorFlags;
	D3DCAPS9 caps;
	if (D3D_OK == pD3D->GetDeviceCaps(theApp.fsAdapter, D3DDEVTYPE_HAL, &caps)) {
		if (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
			BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		} else {
			BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
		}
		if (caps.DevCaps & D3DDEVCAPS_PUREDEVICE) {
			BehaviorFlags |= D3DCREATE_PUREDEVICE;
		}
	} else {
		winlog("Error retrieving device's D3D capabilities\n");
		return false;
	}


	HRESULT hret = pD3D->CreateDevice(theApp.fsAdapter,
		caps.DeviceType,
		pWnd->GetSafeHwnd(),
		BehaviorFlags,
		&dpp,
		&pDevice);
#ifdef _DEBUG
	switch(hret)
	{
	case D3DERR_DEVICELOST:
		winlog("Error creating Direct3DDevice (D3DERR_DEVICELOST)\n");
		return false;
		break;
	case D3DERR_INVALIDCALL:
		winlog("Error creating Direct3DDevice (D3DERR_INVALIDCALL)\n");
		return false;
		break;
	case D3DERR_NOTAVAILABLE:
		winlog("Error creating Direct3DDevice (D3DERR_NOTAVAILABLE)\n");
		return false;
		break;
	case D3DERR_OUTOFVIDEOMEMORY:
		winlog("Error creating Direct3DDevice (D3DERR_OUTOFVIDEOMEMORY)\n");
		return false;
		break;
	}
#endif

	restoreDeviceObjects();

	// Set the status message's background vertex information, that does not need to be changed in realtime
	msgBox[0].z = 0.5f;
	msgBox[0].rhw = 1.0f;
	msgBox[0].color = 0x7fffffff;
	msgBox[1].z = 0.5f;
	msgBox[1].rhw = 1.0f;
	msgBox[1].color = 0x7f7f7f7f;
	msgBox[2].z = 0.5f;
	msgBox[2].rhw = 1.0f;
	msgBox[2].color = 0x7f7f7fff;
	msgBox[3].z = 0.5f;
	msgBox[3].rhw = 1.0f;
	msgBox[3].color = 0x7f7f7f7f;
	
	// Set up the vertices of the texture
	verts[0].z = verts[1].z = verts[2].z = verts[3].z = 1.0f;
	verts[0].color = verts[1].color = verts[2].color = verts[3].color = D3DCOLOR_ARGB(0xff, 0xff, 0xff, 0xff);
	verts[1].u = verts[2].u = 1.0f;
	verts[0].u = verts[3].u = 0.0f;
	verts[0].v = verts[1].v = 0.0f;
	verts[2].v = verts[3].v = 1.0f;
	verts[0].x = verts[3].x = 0.0f;
	verts[1].x = verts[2].x = 1.0f;
	verts[0].y = verts[1].y = 0.0f;
	verts[2].y = verts[3].y = 1.0f;



	utilUpdateSystemColorMaps(theApp.filterLCD );
	theApp.updateFilter();
	theApp.updateIFB();

	pWnd->DragAcceptFiles(TRUE);

	initSucessful = true;
	doNotRender = false;
	return TRUE;  
}

bool Direct3DDisplay::initializeOffscreen(unsigned int w, unsigned int h)
{
	D3DFORMAT format = screenFormat;
	
	unsigned int correctedWidth=w, correctedHeight=h;

	// This function corrects the texture size automaticly
	if(D3D_OK  == D3DXCheckTextureRequirements(
		pDevice,
		&correctedWidth,
		&correctedHeight,
		NULL,
		0,
		&format,
		D3DPOOL_MANAGED))
	{
		if( (correctedWidth < w) || (correctedHeight < h) )
		{
			if(theApp.filterFunction)
			{
				filterDisabled = true;
				theApp.filterFunction = NULL;
				systemMessage(0, "3D card cannot support needed texture size for filter function. Disabling it");
			}
			else
				systemMessage(0, "Graphics card doesn't support needed texture size for emulation.");
		}
		else filterDisabled = false;

		if(D3D_OK == D3DXCreateTexture(
			pDevice,
			correctedWidth,
			correctedHeight,
			1,
			D3DUSAGE_DYNAMIC,
			format,
			D3DPOOL_DEFAULT,
			&pTexture) )
		{
			width = w;
			height = h;
			textureWidth = correctedWidth;
			textureHeight = correctedHeight;
			return true;
		}
		else systemMessage(0, "Texture creation failed");
	}
	return false;
}


void Direct3DDisplay::updateFiltering(int filter)
{ //TODO: use GetSampletState before changing
	if(!pDevice) {
		return;
	}

	HRESULT res;

	switch(filter)
	{
	default:
	case 0:
		// point filtering
		res = pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
		if (res != D3D_OK) {
			systemMessage(0, "Could not set point filtering mode: %d", res);
			return;
		}
		res = pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
		if (res != D3D_OK) {
			systemMessage(0, "Could not set point filtering mode: %d", res);
			return;
		}
		break;
	case 1:
		// bilinear
		res = pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		if (res != D3D_OK) {
			systemMessage(0, "Could not set bilinear filtering mode: %d", res);
			return;
		}
		res = pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		if (res != D3D_OK) {
			systemMessage(0, "Could not set bilinear filtering mode: %d", res);
			return;
		}
		// Don't wrap textures .. otherwise bottom blurs top to bottom
		pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		break;
	}
	return;
}

void Direct3DDisplay::clear()
{ // interface function
	if(!initSucessful) return;

	if (pDevice)
		pDevice->Clear( 0L, NULL, D3DCLEAR_TARGET,
#ifdef _DEBUG
		0xffff00ff //pink
#else
		0xff000000 //black
#endif
		, 1.0f, 0L );
}

void Direct3DDisplay::renderMenu()
{ // interface function
	if(!initSucessful) return;

	if(theApp.m_pMainWnd)
		theApp.m_pMainWnd->DrawMenuBar();
}

void Direct3DDisplay::render()
{ // interface function
	if(!pDevice) return;
	if(!initSucessful) return;
	if(doNotRender) return;

	unsigned int nBytesPerPixel = systemColorDepth >> 3; // This is the byte count of a Pixel
	unsigned int pitch = (theApp.filterWidth * nBytesPerPixel) + 4; // The size of a scanline in bytes

	// Test the cooperative level to see if it's okay to render
	HRESULT hr;
	hr = pDevice->TestCooperativeLevel();
	switch(hr)
	{
	case D3DERR_DEVICENOTRESET:
		resetDevice();
		break;
	case D3DERR_DEVICELOST:
		winlog("Render: D3DERR_DEVICELOST\n");
		return;
		break;
	case D3DERR_DRIVERINTERNALERROR:
		winlog("Render: D3DERR_DRIVERINTERNALERROR\n");
		cleanup();
		if(initialize()) {
			return;
		} else { // reinitialize device failed
			AfxPostQuitMessage(D3DERR_DRIVERINTERNALERROR);
		}
		break;
	}

	// Clear the screen
	if (pDevice)
		pDevice->Clear( 0L, NULL, D3DCLEAR_TARGET,
#ifdef _DEBUG
		0xffff00ff //pink
#else
		0xff000000 //black
#endif
		, 1.0f, 0L );


	if(SUCCEEDED(pDevice->BeginScene()))
	{
		D3DLOCKED_RECT locked;
	
		if( D3D_OK == pTexture->LockRect(0, &locked, NULL, D3DLOCK_DISCARD) )
		{
			if(theApp.filterFunction)
			{
				theApp.filterFunction(
					pix + pitch,
					pitch,
					(u8*)theApp.delta,
					(u8*)locked.pBits,
					locked.Pitch,
					theApp.filterWidth,
					theApp.filterHeight);
			}
			else
			{
				// Copy the image at [pix] to the locked Direct3D texture
				__asm
				{
					mov eax, theApp.sizeX ; Initialize
					mov ebx, theApp.sizeY ;
					mov edi, locked.pBits ;
					mov edx, locked.Pitch ;

					cmp systemColorDepth, 16 ; Check systemColorDepth==16bit
					jnz gbaOtherColor        ;
					sub edx, eax             ;
					sub edx, eax             ;
					mov esi, pix             ;
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
					cmp systemColorDepth, 32 ; Check systemColorDepth==32bit
					jnz gbaOtherColor2       ;
					
					lea esi, [eax*4]       ;
					sub edx, esi           ;
					mov esi, pix           ;
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
					lea eax, [eax+2*eax] ; Work like systemColorDepth==24bit
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

				//C Version of the code above
				//unsigned int i;
				//int x, y, srcPitch = (theApp.sizeX+1) * nBytesPerPixel;
				//unsigned char * src = ((unsigned char*)pix)+srcPitch;
				//unsigned char * dst = (unsigned char*)locked.pBits;
				//for (y=0;y<theApp.sizeY;y++) //Width
				//	for (x=0;x<theApp.sizeX;x++) //Height
				//		for (i=0;i<nBytesPerPixel;i++) //Byte# Of Pixel
				//			*(dst+i+(x*nBytesPerPixel)+(y*locked.Pitch)) = *(src+i+(x*nBytesPerPixel)+(y*srcPitch));
			}
			pTexture->UnlockRect(0);
			
			pDevice->SetFVF( D3DFVF_IMAGEVERTEX );
			pDevice->SetTexture( 0, pTexture );
			pDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, verts, sizeof( IMAGEVERTEX ) );

		} // SUCCEEDED(pTexture->LockRect...
		else
		{
			systemMessage(0,"Rendering error");
		}


		// Draw the screen message
		if(theApp.screenMessage)
		{
			if( ((GetTickCount() - theApp.screenMessageTime) < 3000) &&
				!theApp.disableStatusMessage && pFont )
			{
				CRect msgRect(
					64,
					dpp.BackBufferHeight	- 32,
					dpp.BackBufferWidth		- 64,
					dpp.BackBufferHeight);

				msgBox[0].x = (FLOAT)msgRect.left;
				msgBox[0].y = (FLOAT)msgRect.top;
				msgBox[1].x = (FLOAT)msgRect.right;
				msgBox[1].y = (FLOAT)msgRect.top;
				msgBox[2].x = (FLOAT)msgRect.right;
				msgBox[2].y = (FLOAT)msgRect.bottom;
				msgBox[3].x = (FLOAT)msgRect.left;
				msgBox[3].y = (FLOAT)msgRect.bottom;

				pDevice->SetFVF( D3DFVF_TEXTBOXVERTEX );
				pDevice->SetTexture( 0, NULL );
				pDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, msgBox, sizeof(TEXTBOXVERTEX));

				pFont->DrawText(NULL, theApp.screenMessageBuffer, -1, msgRect, DT_CENTER | DT_VCENTER, 0x7fff0000);
			}
			else theApp.screenMessage = false;
		}


		// Draw the speed
		if( (theApp.videoOption > VIDEO_4X) && theApp.showSpeed )
		{
			// Create the string text
			char buffer[30];
			if(theApp.showSpeed == TRUE)
				sprintf(buffer, "%3d%%", systemSpeed);
			else
				sprintf(buffer, "%3d%%(%d, %d fps)", systemSpeed,
				systemFrameSkip,
				theApp.showRenderedFrames);

			//Draw the string
			D3DCOLOR speedColor;
			theApp.showSpeedTransparent==TRUE ? speedColor = 0x7fffffff : speedColor = 0xffffffff;

			CRect speedRect(
				64,
				0,
				dpp.BackBufferWidth		- 64,
				32);

			pFont->DrawText(NULL, buffer, -1, speedRect, DT_CENTER | DT_VCENTER, speedColor);
		}

		pDevice->EndScene();
		
		pDevice->Present( NULL, NULL, NULL, NULL ); //Draw everything to the screen
	}
}


void Direct3DDisplay::invalidateDeviceObjects()
{

  if(pFont) {
	  pFont->Release();
	  pFont = NULL;
  }
}

void Direct3DDisplay::restoreDeviceObjects()
{
	// Create the font
	D3DXCreateFont( pDevice, 24, 0, FW_BOLD, 1, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, "Arial", &pFont );

	// Set texture filter
	updateFiltering(theApp.d3dFilter);
	
	// Set device settings
	pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
	pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
	pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA  );
	pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	if (theApp.menuToggle)
		pDevice->SetDialogBoxMode( TRUE );
	pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

	// Set matrices
	initializeMatrices();
}


void Direct3DDisplay::resize(int w, int h)
{ // interface function
	if(!initSucessful) return;

	if ( (w>0) && (h>0) ) {
		if(pDevice)
		{
			dpp.BackBufferWidth = w;
			dpp.BackBufferHeight = h;
			setPresentationType();
			if (resetDevice()) {
				doNotRender = false;
			}
		}
	} else {
		doNotRender = true;
	}

}


bool Direct3DDisplay::changeRenderSize(int w, int h)
{ // interface function
	if(!initSucessful) return false;

		// w and h is the size of the filtered image (So this could be 3xGBASize)
	if(w != width || h != height)
	{
		if(pTexture) {
			pTexture->Release();
			pTexture = NULL;
		}

		if(!initializeOffscreen(w, h)) {
			return false;
		}
	}
	
	if(filterDisabled && theApp.filterFunction)
		theApp.filterFunction = NULL;
	
	// Set up 2D matrices
	initializeMatrices();

	return true;
}

void Direct3DDisplay::setOption(const char *option, int value)
{ // interface function
	if(!initSucessful) return;

	if(!strcmp(option, "d3dFilter"))
		updateFiltering(theApp.d3dFilter);

	if(!strcmp(option, "vsync"))
	{
		if (pDevice)
		{
			if (theApp.vsync)
				dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;       // VSync
			else
				dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // No Sync
			resetDevice();
		}
	}

	if(!strcmp(option, "triplebuffering"))
	{
		if (theApp.tripleBuffering)
			dpp.BackBufferCount = 3;
		else
			dpp.BackBufferCount = 2;
		resetDevice();
	}

	if(!strcmp(option, "d3dKeepAspectRatio"))
		keepAspectRatio = true; //theApp.d3dKeepAspectRatio;
}

int Direct3DDisplay::selectFullScreenMode(GUID **)
{ // interface function
	int w, h, b;
	UniVideoModeDlg dlg(0, &w, &h, &b, &SelectedFreq, &SelectedAdapter);

	if (0 == dlg.DoModal())
	{
		return (b<<24) + (w<<12) + h;
	// Bits<<24  |  Width<<12  |  Height
	}
	else
	{
		return -1;
	}
}

int Direct3DDisplay::selectFullScreenMode2()
{
	return (SelectedAdapter<<16) + SelectedFreq;
}

// Reset Device and Resources
bool Direct3DDisplay::resetDevice()
{
	invalidateDeviceObjects();
	if(pTexture) {
			pDevice->SetTexture( 0, NULL);
			pTexture->Release();
			pTexture = NULL;
	}
	if (!theApp.menuToggle)
		pDevice->SetDialogBoxMode( FALSE );

	HRESULT hr = pDevice->Reset(&dpp);
	if (hr == D3D_OK)
	{
		restoreDeviceObjects();
		if(!initializeOffscreen(width, height))
			return false;

		return true;
	}
	else
	{
		switch(hr)
		{
		case D3DERR_DEVICELOST:
			winlog("Render_DeviceLost: D3DERR_DEVICELOST\n");
			break;
		case D3DERR_DRIVERINTERNALERROR:
			winlog("Render_DeviceLost: D3DERR_DRIVERINTERNALERROR\n");
			break;
		case D3DERR_INVALIDCALL:
			winlog("Render_DeviceLost: D3DERR_INVALIDCALL\n");
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			winlog("Render_DeviceLost: D3DERR_OUTOFVIDEOMEMORY\n");
			break;
		case E_OUTOFMEMORY:
			winlog("Render_DeviceLost: E_OUTOFMEMORY\n");
			break;
		}
		winlog("Failed to reset device: %08x\n", hr);
		return false;
	}
}

void Direct3DDisplay::initializeMatrices()
{ // Configure matrices to use standard orthogonal projection (2D)
	D3DXMATRIX Ortho2D;
	D3DXMATRIX Identity;
	D3DXMATRIX temp1, temp2;

	// Initialize an orthographic matrix which automaticly compensates the difference between image size and texture size
	if (!keepAspectRatio) {
		D3DXMatrixOrthoOffCenterLH(
			&Ortho2D,
			0.0f, // left
			1.0f * (FLOAT)width / (FLOAT)textureWidth, // right
			1.0f * (FLOAT)height / (FLOAT)textureHeight, // bottom
			0.0f, // top
			0.0f, 1.0f); // z
	} else {
		FLOAT l=0.0f, r=0.0f, b=0.0f, t=0.0f;
		FLOAT srcAspectRatio = (FLOAT)theApp.sizeX / (FLOAT)theApp.sizeY;
		FLOAT aspectRatio = (FLOAT)dpp.BackBufferWidth / (FLOAT)dpp.BackBufferHeight;
		FLOAT textureImageDiffX = (FLOAT)width / (FLOAT)textureWidth;
		FLOAT textureImageDiffY = (FLOAT)height / (FLOAT)textureHeight;
		aspectRatio /= srcAspectRatio;
		
		if(aspectRatio > 1.0f) {
			r = 1.0f * textureImageDiffX * aspectRatio;
			b = 1.0f * textureImageDiffY;
		} else {
			r = 1.0f * textureImageDiffX;
			b = 1.0f * textureImageDiffY * (1.0f / aspectRatio);
		}

		D3DXMatrixOrthoOffCenterLH(
			&temp1,
			l,
			r,
			b,
			t,
			0.0f, 1.0f); // z
		D3DXMatrixTranslation( // translate matrix > move image
			&temp2,
			(aspectRatio>1.0)?(  (aspectRatio - 1.0f) * 0.5f * textureImageDiffX  ):0.0f,
			(aspectRatio<1.0)?(  ((1.0f/aspectRatio) - 1.0f) * 0.5f * textureImageDiffY  ):0.0f,
			0.0f);
		D3DXMatrixMultiply(&Ortho2D, &temp2, &temp1);
	}

	D3DXMatrixIdentity(&Identity); // Identity = Do not change anything

	pDevice->SetTransform(D3DTS_PROJECTION, &Ortho2D);
	pDevice->SetTransform(D3DTS_WORLD, &Identity);
	pDevice->SetTransform(D3DTS_VIEW, &Identity);
}

IDisplay *newDirect3DDisplay()
{
  return new Direct3DDisplay();
}