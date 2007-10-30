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
#include <d3d9.h>
#include <d3dx9.h>
#include "vba.h"
#include "MainWnd.h"
#include "UniVideoModeDlg.h"
#include "../Util.h"
#include "../Globals.h"
#include "../Util.h"
#include "../gb/gbGlobals.h"

#include "../gbafilter.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifdef MMX
extern "C" bool cpu_mmx;
extern bool detectMMX();
#endif

extern int Init_2xSaI(u32);
extern void winlog(const char *,...);
extern int systemSpeed;


// Textured Vertex
typedef struct _D3DTLVERTEX {
  float sx; /* Screen coordinates */
  float sy;
  float sz;
  float rhw; /* Reciprocal of homogeneous w */
  D3DCOLOR color; /* Vertex color */
  float tu; /* Texture coordinates */
  float tv;
	_D3DTLVERTEX() { }
  _D3DTLVERTEX(
		const D3DVECTOR& v,
		float _rhw,
		D3DCOLOR _color,
		float _tu, float _tv)
  {	sx = v.x; sy = v.y; sz = v.z;
		rhw = _rhw;
		color = _color; 
		tu = _tu; tv = _tv; }
} D3DTLVERTEX, *LPD3DTLVERTEX;
#define D3DFVF_TLVERTEX D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1


// Simple Vertex
struct D3DVERTEX_SIMPLE
{
	FLOAT		x, y, z, rhw;
	D3DCOLOR	color;
};
#define D3DFVF_SIMPLE D3DFVF_XYZRHW | D3DFVF_DIFFUSE


class Direct3DDisplay : public IDisplay
{
private:
	HINSTANCE             d3dDLL;
	LPDIRECT3D9           pD3D;
	LPDIRECT3DDEVICE9     pDevice;
	LPDIRECT3DTEXTURE9    pTexture;
	D3DPRESENT_PARAMETERS dpp;
	D3DFORMAT             screenFormat;
	int                   width, height; // Size of the source image to display
	bool                  filterDisabled;
	ID3DXFont             *pFont;
	bool                  failed;
	D3DTLVERTEX           verts[4];      // The coordinates for our texture
	D3DVERTEX_SIMPLE      msgBox[4];
	int                   textureWidth;  // Size of the texture,
	int                   textureHeight; // where the source image is copied to
	int                   SelectedFreq, SelectedAdapter;
	bool                  fullscreen;
	
	void restoreDeviceObjects();
	void invalidateDeviceObjects();
	bool initializeOffscreen(unsigned int w, unsigned int h);
	void updateFiltering(int);
	void updateVSync(void);

public:
	Direct3DDisplay();
	virtual ~Direct3DDisplay();

	virtual bool initialize();
	virtual void cleanup();
	virtual void render();
	virtual void checkFullScreen();
	virtual void renderMenu();
	virtual void clear();
	virtual bool changeRenderSize(int w, int h);
	virtual void resize(int w, int h);
	virtual DISPLAY_TYPE getType() { return DIRECT_3D; };
	virtual void setOption(const char *, int);
	virtual int selectFullScreenMode(GUID **);  
	virtual int selectFullScreenMode2();
};

Direct3DDisplay::Direct3DDisplay()
{
  d3dDLL = NULL;
  pD3D = NULL;  
  pDevice = NULL;
  pTexture = NULL;
  pFont = NULL;
  screenFormat = D3DFMT_R5G6B5;
  width = 0;
  height = 0;
  filterDisabled = false;
  failed = false;
}

Direct3DDisplay::~Direct3DDisplay()
{
  cleanup();
}

void Direct3DDisplay::cleanup()
{
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

    if(d3dDLL != NULL) {
      FreeLibrary(d3dDLL);
      d3dDLL = NULL;
    }
  }
}

bool Direct3DDisplay::initialize()
{
	// Get emulated image's dimensions
	switch (theApp.cartridgeType)
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

	
	// Load DirectX DLL
	d3dDLL = LoadLibrary("D3D9.DLL");
  LPDIRECT3D9 (WINAPI *D3DCreate)(UINT);
  if(d3dDLL != NULL)
	{
		D3DCreate = (LPDIRECT3D9 (WINAPI *)(UINT))
			GetProcAddress(d3dDLL, "Direct3DCreate9");
		
		if(D3DCreate == NULL)
		{
			theApp.directXMessage("Direct3DCreate9");
			return FALSE;
		}
	}
	else
	{
		theApp.directXMessage("D3D9.DLL");
		return FALSE;
	}

	pD3D = D3DCreate(D3D_SDK_VERSION);

	if(pD3D == NULL)
	{
		winlog("Error creating Direct3D object\n");
		return FALSE;
	}




	// Display resolution
  D3DDISPLAYMODE mode;
  pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);
  
  switch(mode.Format) {
  case D3DFMT_R8G8B8:
    systemColorDepth = 24;
    systemRedShift = 19;
    systemGreenShift = 11;
    systemBlueShift = 3;
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

	// Check the available fullscreen modes and enable menu items
	unsigned int nModes, i;
	D3DDISPLAYMODE dm;

	theApp.mode320Available = false;
	theApp.mode640Available = false;
	theApp.mode800Available = false;

	nModes = pD3D->GetAdapterModeCount(theApp.fsAdapter, D3DFMT_R5G6B5);
	for (i = 0; i<nModes; i++)
	{
		if (D3D_OK == pD3D->EnumAdapterModes(theApp.fsAdapter, D3DFMT_R5G6B5, i, &dm) )
		{
			if ( (dm.Width == 320) && (dm.Height == 240) )
				theApp.mode320Available = true;
			if ( (dm.Width == 640) && (dm.Height == 480) )
				theApp.mode640Available = true;
			if ( (dm.Width == 800) && (dm.Height == 600) )
				theApp.mode800Available = true;
		}
	}


#ifdef MMX
	if(!theApp.disableMMX)
		cpu_mmx = theApp.detectMMX();
	else
		cpu_mmx = 0;
#endif

	screenFormat = mode.Format;

	// Change display mode
	ZeroMemory(&dpp, sizeof(dpp));
	dpp.Windowed = !fullscreen;
	if (fullscreen)
		dpp.BackBufferFormat =
		(theApp.fsColorDepth == 32) ? D3DFMT_X8R8G8B8 : D3DFMT_R5G6B5;
	else
		dpp.BackBufferFormat = mode.Format;
	dpp.BackBufferCount = 1;
	dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
	dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	dpp.BackBufferWidth = fullscreen ? theApp.fsWidth : theApp.surfaceSizeX;
	dpp.BackBufferHeight = fullscreen ? theApp.fsHeight : theApp.surfaceSizeY;
	dpp.hDeviceWindow = pWnd->GetSafeHwnd();
	dpp.FullScreen_RefreshRateInHz = fullscreen ? theApp.fsFrequency : 0;
	dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	if (theApp.vsync)
		dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;			// VSync
	else
		dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;	// No Sync

	HRESULT hret = pD3D->CreateDevice(theApp.fsAdapter,
		D3DDEVTYPE_HAL,
		pWnd->GetSafeHwnd(),
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&dpp,
		&pDevice);
	if(hret != D3D_OK)
	{
		winlog("Error creating Direct3DDevice %08x\n", hret);
		return false;
	}
	pDevice->SetDialogBoxMode(TRUE); // !!! Enable menu and windows !!!

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

	utilUpdateSystemColorMaps(theApp.filterLCD );
	theApp.updateFilter();
	theApp.updateIFB();

	if(failed)
		return false;

	pWnd->DragAcceptFiles(TRUE);

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
			D3DX_DEFAULT,
			0,
			format,
			D3DPOOL_MANAGED,
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
{
  switch(filter) {
  default:
  case 0:
    // point filtering
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    break;
  case 1:
    // bilinear
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    break;
  }
}

void Direct3DDisplay::clear()
{
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
{
  checkFullScreen();
  if(theApp.m_pMainWnd)
    theApp.m_pMainWnd->DrawMenuBar();
}

void Direct3DDisplay::checkFullScreen()
{
	//if(tripleBuffering)
		//pDirect3D->FlipToGDISurface();
}

void Direct3DDisplay::render()
{
	unsigned int nBytesPerPixel = systemColorDepth / 8; //This is the byte count of a Pixel
	unsigned int pitch = (theApp.filterWidth * nBytesPerPixel) + 4;
	HRESULT hr;

	if(!pDevice) return;

    
	// Test the cooperative level to see if it's okay to render
	hr = pDevice->TestCooperativeLevel();
	if(hr != D3D_OK)
	{
		switch (hr)
		{
		case D3DERR_DEVICELOST:
			break;
		case D3DERR_DEVICENOTRESET:
			invalidateDeviceObjects();
			hr = pDevice->Reset(&dpp);
			if( hr == D3D_OK )
			{
				restoreDeviceObjects();
			}
#ifdef _DEBUG
			else
				switch (hr)
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
#endif
				break;
		case D3DERR_DRIVERINTERNALERROR:
			winlog("Render: D3DERR_DRIVERINTERNALERROR\n");
			theApp.ExitInstance();
			break;
		}
		return;
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

				// C Version of the code above
				//int x,y,i;
				//int srcPitch = (theApp.sizeX+1) * nBytesPerPixel;
				//unsigned char * src = ((unsigned char*)pix)+srcPitch;
				//unsigned char * dst = (unsigned char*)locked.pBits;
				//for (y=0;y<theApp.sizeY;y++) //Width
				//	for (x=0;x<theApp.sizeX;x++) //Height
				//		for (i=0;i<nBytesPerPixel;i++) //Byte# Of Pixel
				//			*(dst+i+(x*nBytesPerPixel)+(y*locked.Pitch)) = *(src+i+(x*nBytesPerPixel)+(y*srcPitch));
			}

			pTexture->UnlockRect(0);
			
			// Set the edges of the texture
			POINT p1, p2;
			p1.x = theApp.dest.left;
			p1.y = theApp.dest.top;
			p2.x = theApp.dest.right;
			p2.y = theApp.dest.bottom;
			theApp.m_pMainWnd->ScreenToClient(&p1);
			theApp.m_pMainWnd->ScreenToClient(&p2);

			FLOAT left, right, top, bottom;
			left = (FLOAT)(p1.x);
			top = (FLOAT)(p1.y);
			right = (FLOAT)(p2.x);
			bottom = (FLOAT)(p2.y);

			right *= (FLOAT)textureWidth/theApp.rect.right;
			bottom *= (FLOAT)textureHeight/theApp.rect.bottom;

			
			verts[0] = D3DTLVERTEX(D3DXVECTOR3( left, top, 0.0f), 1.0f, 0xffffffff, 0.0f, 0.0f );
			verts[1] = D3DTLVERTEX(D3DXVECTOR3( right, top, 0.0f), 1.0f, 0xffffffff, 1.0f, 0.0f );
			verts[2] = D3DTLVERTEX(D3DXVECTOR3( right, bottom, 0.0f), 1.0f, 0xffffffff, 1.0f, 1.0f );
			verts[3] = D3DTLVERTEX(D3DXVECTOR3( left, bottom, 0.0f), 1.0f, 0xffffffff, 0.0f, 1.0f );

			pDevice->SetFVF( D3DFVF_TLVERTEX );
			pDevice->SetTexture( 0, pTexture );
			pDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, verts, sizeof(D3DTLVERTEX) );
			pTexture->UnlockRect(0);
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

				pDevice->SetFVF( D3DFVF_SIMPLE );
				pDevice->SetTexture( 0, NULL );
				pDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, msgBox, sizeof(D3DVERTEX_SIMPLE));

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
  if(pFont)
    pFont->Release();
  pFont = NULL;
}

void Direct3DDisplay::restoreDeviceObjects()
{
	updateFiltering(theApp.d3dFilter);
	
	// Enable transparent vectors
	pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
	pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
	pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA  );
	pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );


	// Create the font
	D3DXCreateFont( pDevice, 24, 0, FW_BOLD, 1, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, "Arial", &pFont );
}


void Direct3DDisplay::resize(int w, int h)
{
	if(pDevice)
	{
		dpp.BackBufferWidth = w;
		dpp.BackBufferHeight = h;
		invalidateDeviceObjects();
		HRESULT hr = pDevice->Reset(&dpp);
		if( hr == D3D_OK )
			restoreDeviceObjects();
		else
			systemMessage(0, "Failed device reset %08x", hr);
	}
}


bool Direct3DDisplay::changeRenderSize(int w, int h)
{
	// w and h is the size of the filtered image (So this could be 3xGBASize)
  if(w != width || h != height)
	{
    if(pTexture)
		{
      pTexture->Release();
      pTexture = NULL;
		}

    if(!initializeOffscreen(w, h)) {
      failed = true;
      return false;
    }
  }

  if(filterDisabled && theApp.filterFunction)
    theApp.filterFunction = NULL;

  return true;
}

void Direct3DDisplay::setOption(const char *option, int value)
{
  if(!strcmp(option, "d3dFilter"))
    updateFiltering(value);

	if(!strcmp(option, "d3dVSync"))
		updateVSync();
}

int Direct3DDisplay::selectFullScreenMode(GUID **)
{
	//int newScreenWidth, newScreenHeight;
	//int newScreenBitsPerPixel;

	//D3DDISPLAYMODE dm;
	//pDevice->GetDisplayMode( 0, &dm );

	//newScreenWidth = dm.Width;
	//newScreenHeight = dm.Height;

	//switch (dm.Format)
	//{
	//case D3DFMT_A2R10G10B10:
	//case D3DFMT_A8R8G8B8:
	//case D3DFMT_X8R8G8B8:
	//	newScreenBitsPerPixel = 32;
	//	break;
	//case D3DFMT_A1R5G5B5:
	//case D3DFMT_X1R5G5B5:
	//case D3DFMT_R5G6B5:
	//	newScreenBitsPerPixel = 16;
	//	break;
	//}
	//
	//return (newScreenBitsPerPixel << 24) | (newScreenWidth << 12) | newScreenHeight;
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


void Direct3DDisplay::updateVSync(void)
{
	if (pDevice)
	{
		if (theApp.vsync)
			dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;       // VSync
		else
			dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // No Sync

		invalidateDeviceObjects();
		HRESULT hr = pDevice->Reset(&dpp);
		if (hr == D3D_OK)
			restoreDeviceObjects();
		else
			systemMessage(0, "Failed to change VSync option %08x", hr);
	}
}

IDisplay *newDirect3DDisplay()
{
  return new Direct3DDisplay();
}