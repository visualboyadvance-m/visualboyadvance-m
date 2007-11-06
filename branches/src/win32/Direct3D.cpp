// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2005-2006 VBA development team

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

#include "MainWnd.h"

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../Text.h"
#include "../Util.h"
#include "../gb/gbGlobals.h"

// Direct3D
#define DIRECT3D_VERSION 0x0900
#include <d3d9.h>      // main include file
#include <D3dx9core.h> // required for font rednering
#include <Dxerr.h>     // contains debug functions

extern int Init_2xSaI(u32); // initializes all pixel filters
extern int systemSpeed;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifdef MMX
extern "C" bool cpu_mmx;
extern bool detectMMX();
#endif


class Direct3DDisplay : public IDisplay {
private:
	LPDIRECT3D9           pD3D;
	LPDIRECT3DDEVICE9     pDevice;
	D3DPRESENT_PARAMETERS dpp;
	D3DFORMAT             screenFormat;
	LPDIRECT3DSURFACE9    emulatedImage;
	D3DTEXTUREFILTERTYPE  filter;
	int                   width;
	int                   height;
	RECT                  destRect;
	bool                  failed;
	ID3DXFont             *pFont;

	void createFont();
	void destroyFont();
	void createSurface();
	void destroySurface();
	void calculateDestRect();
	bool resetDevice();

public:
	Direct3DDisplay();
	virtual ~Direct3DDisplay();
	virtual DISPLAY_TYPE getType() { return DIRECT_3D; };

	virtual bool initialize();
	virtual void cleanup();
	virtual void clear();
	virtual void render();

	virtual void renderMenu();
	virtual bool changeRenderSize( int w, int h );
	virtual void resize( int w, int h );
	virtual void setOption( const char *option, int value );
	virtual int  selectFullScreenMode( GUID ** );  
};


Direct3DDisplay::Direct3DDisplay()
{
	pD3D = NULL;  
	pDevice = NULL;
	screenFormat = D3DFMT_X8R8G8B8;
	width = 0;
	height = 0;
	failed = false;
	pFont = NULL;
	emulatedImage = NULL;
	filter = D3DTEXF_POINT;
}


Direct3DDisplay::~Direct3DDisplay()
{
	cleanup();
}


void Direct3DDisplay::cleanup()
{
	destroyFont();
	destroySurface();

	if( pDevice ) {
		pDevice->Release();
		pDevice = NULL;
	}

	if( pD3D ) {
		pD3D->Release();
		pD3D = NULL;
	}
}


bool Direct3DDisplay::initialize()
{
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


	switch(theApp.videoOption)
	{
	case VIDEO_1X:
		theApp.surfaceSizeX = theApp.sizeX;
		theApp.surfaceSizeY = theApp.sizeY;
		break;
	case VIDEO_2X:
		theApp.surfaceSizeX = theApp.sizeX * 2;
		theApp.surfaceSizeY = theApp.sizeY * 2;
		break;
	case VIDEO_3X:
		theApp.surfaceSizeX = theApp.sizeX * 3;
		theApp.surfaceSizeY = theApp.sizeY * 3;
		break;
	case VIDEO_4X:
		theApp.surfaceSizeX = theApp.sizeX * 4;
		theApp.surfaceSizeY = theApp.sizeY * 4;
		break;
	case VIDEO_320x240:
	case VIDEO_640x480:
	case VIDEO_800x600:
	case VIDEO_OTHER:
		float scaleX = ((float)theApp.fsWidth / theApp.sizeX);
		float scaleY = ((float)theApp.fsHeight / theApp.sizeY);
		float min = (scaleX < scaleY) ? scaleX : scaleY;
		if(theApp.fullScreenStretch) {
			theApp.surfaceSizeX = theApp.fsWidth;
			theApp.surfaceSizeY = theApp.fsHeight;
		} else {
			theApp.surfaceSizeX = (int)(theApp.sizeX * min);
			theApp.surfaceSizeY = (int)(theApp.sizeY * min);
		}
		break;
	}
	
	theApp.rect.left = 0;
	theApp.rect.top = 0;
	theApp.rect.right = theApp.sizeX;
	theApp.rect.bottom = theApp.sizeY;

	theApp.dest.left = 0;
	theApp.dest.top = 0;
	theApp.dest.right = theApp.surfaceSizeX;
	theApp.dest.bottom = theApp.surfaceSizeY;
	

	DWORD style = WS_POPUP | WS_VISIBLE;
	DWORD styleEx = 0;
	
	if(theApp.videoOption <= VIDEO_4X) {
		style |= WS_OVERLAPPEDWINDOW;
	} else {
		styleEx = 0;
	}
	
	if(theApp.videoOption <= VIDEO_4X) {
		AdjustWindowRectEx(&theApp.dest, style, TRUE, styleEx);
	} else {
		AdjustWindowRectEx(&theApp.dest, style, FALSE, styleEx);    
	}
	
	int winSizeX = theApp.dest.right-theApp.dest.left;
	int winSizeY = theApp.dest.bottom-theApp.dest.top;
	
	if(theApp.videoOption > VIDEO_4X) {
		winSizeX = theApp.fsWidth;
		winSizeY = theApp.fsHeight;
	}

	int x = 0, y = 0;
	
	if(theApp.videoOption <= VIDEO_4X) {
		x = theApp.windowPositionX;
		y = theApp.windowPositionY;
	}
	
	
	// Create a window
	MainWnd *pWnd = new MainWnd;
	theApp.m_pMainWnd = pWnd;

	pWnd->CreateEx(styleEx,
		theApp.wndClass,
		_T("VisualBoyAdvance"),
		style,
		x,y,winSizeX,winSizeY,
		NULL,
		0);

	if (!(HWND)*pWnd) {
		DXTRACE_ERR_MSGBOX( _T("Error creating window"), 0 );
		return FALSE;
	}
	pWnd->DragAcceptFiles(TRUE);
	theApp.updateMenuBar();
	theApp.adjustDestRect();


	// load Direct3D v9
	pD3D = Direct3DCreate9( D3D_SDK_VERSION );

	if(pD3D == NULL) {
		DXTRACE_ERR_MSGBOX( _T("Error creating Direct3D object"), 0 );
		return FALSE;
	}

	theApp.mode320Available = FALSE;
	theApp.mode640Available = FALSE;
	theApp.mode800Available = FALSE;

	D3DDISPLAYMODE mode;
	pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);
	screenFormat = mode.Format;

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
	  DXTRACE_ERR_MSGBOX( _T("Unsupport D3D format"), 0 );
	  return false;
	}
	theApp.fsColorDepth = systemColorDepth;
	utilUpdateSystemColorMaps();


#ifdef MMX
	if(!theApp.disableMMX) {
		cpu_mmx = theApp.detectMMX();
	} else {
		cpu_mmx = 0;
	}
#endif


	theApp.updateFilter();
	theApp.updateIFB();


	// create device
	ZeroMemory(&dpp, sizeof(dpp));
	dpp.Windowed = TRUE;
	dpp.BackBufferFormat = mode.Format;
	dpp.BackBufferCount = theApp.tripleBuffering ? 2 : 1;
	dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	dpp.BackBufferWidth = 0; // use width of hDeviceWindow
	dpp.BackBufferHeight = 0; // use height of hDeviceWindow
	dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	dpp.PresentationInterval = theApp.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

	HRESULT hret = pD3D->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		pWnd->GetSafeHwnd(),
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&dpp,
		&pDevice);
	if( FAILED( hret ) ) {
		DXTRACE_ERR_MSGBOX( _T("Error creating Direct3D device"), hret );
		return false;
	}

	createFont();
	createSurface();
	calculateDestRect();

	setOption( _T("d3dFilter"), theApp.d3dFilter );

	if(failed) return false;

	return TRUE;  
}


void Direct3DDisplay::renderMenu()
{
	checkFullScreen();
	if(theApp.m_pMainWnd) {
		theApp.m_pMainWnd->DrawMenuBar();
	}
}


void Direct3DDisplay::clear()
{
	if( pDevice ) {
		pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0x00,0x00,0x00), 0.0f, 0 );
	}
}


void Direct3DDisplay::render()
{
	if( failed ) return;
	if(!pDevice) return;
	if( FAILED( pDevice->TestCooperativeLevel() ) ) return;
	
	clear();
	
	pDevice->BeginScene();
	
	// copy pix to emulatedImage and apply pixel filter if selected
	HRESULT hr;
	D3DLOCKED_RECT lr;
	if( FAILED( hr = emulatedImage->LockRect( &lr, NULL, D3DLOCK_DISCARD ) ) ) {
		DXTRACE_ERR_MSGBOX( _T("Can not lock back buffer"), hr );
		return;
	} else {
		if( !theApp.filterFunction ) {
			copyImage( pix, lr.pBits, theApp.sizeX, theApp.sizeY, lr.Pitch, systemColorDepth );
		} else {
			u32 pitch = theApp.filterWidth * (systemColorDepth>>3) + 4;
			theApp.filterFunction( pix + pitch,
				pitch,
				(u8*)theApp.delta,
				(u8*)lr.pBits,
				lr.Pitch,
				theApp.filterWidth,
				theApp.filterHeight);
		}
		emulatedImage->UnlockRect();
	}

	// copy emulatedImage to pBackBuffer and scale with or without aspect ratio
	LPDIRECT3DSURFACE9 pBackBuffer;
	pDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
	if( theApp.fullScreenStretch ) {
		pDevice->StretchRect( emulatedImage, NULL, pBackBuffer, NULL, filter );
	} else {
		pDevice->StretchRect( emulatedImage, NULL, pBackBuffer, &destRect, filter );
	}
	pBackBuffer->Release();
	pBackBuffer = NULL;

	D3DCOLOR color;
	RECT r;
	r.left = 4;
	r.right = dpp.BackBufferWidth - 4;

	if( theApp.screenMessage ) {
		color = theApp.showSpeedTransparent ? D3DCOLOR_ARGB(0x7F, 0xFF, 0x00, 0x00) : D3DCOLOR_ARGB(0xFF, 0xFF, 0x00, 0x00);
		if( ( ( GetTickCount() - theApp.screenMessageTime ) < 3000 ) && !theApp.disableStatusMessage && pFont ) {
			r.top = dpp.BackBufferHeight - 20;
			r.bottom = dpp.BackBufferHeight - 4;
			pFont->DrawText( NULL, theApp.screenMessageBuffer, -1, &r, 0, color );
		} else {
			theApp.screenMessage = false;
		}
	}

	if( theApp.showSpeed && ( theApp.videoOption > VIDEO_4X ) ) {
		color = theApp.showSpeedTransparent ? D3DCOLOR_ARGB(0x7F, 0x00, 0x00, 0xFF) : D3DCOLOR_ARGB(0xFF, 0x00, 0x00, 0xFF);
		char buffer[30];
		if( theApp.showSpeed == 1 ) {
			sprintf( buffer, "%3d%%", systemSpeed );
		} else {
			sprintf( buffer, "%3d%%(%d, %d fps)", systemSpeed, systemFrameSkip, theApp.showRenderedFrames );
		}
		
		r.top = 4;
		r.bottom = 20;
		pFont->DrawText( NULL, buffer, -1, &r, 0, color );
	}

	pDevice->EndScene();

	pDevice->Present( NULL, NULL, NULL, NULL );

	return;
}


bool Direct3DDisplay::changeRenderSize( int w, int h )
{
	if( (w != width) || (h != height) ) {
		width = w; height = h;
		if( pDevice ) {
			destroySurface();
			createSurface();
			calculateDestRect();
		}
	}
	return true;
}


void Direct3DDisplay::resize( int w, int h )
{
	if( (w != dpp.BackBufferWidth) || (h != dpp.BackBufferHeight) ) {
		dpp.BackBufferWidth = (UINT)w;
		dpp.BackBufferHeight = (UINT)h;
		resetDevice();
		calculateDestRect();
	}
}


int Direct3DDisplay::selectFullScreenMode( GUID ** )
{
	HRESULT hr;
	D3DDISPLAYMODE dm;
	if( FAILED( hr = pDevice->GetDisplayMode( 0, &dm ) ) ) {
		DXTRACE_ERR_MSGBOX( _T("pDevice->GetDisplayMode failed"), hr );
		return false;
	}

	UINT bitsPerPixel;
	switch( dm.Format )
	{
	case D3DFMT_A2R10G10B10:
	case D3DFMT_X8R8G8B8:
		bitsPerPixel = 32;
		break;
	case D3DFMT_X1R5G5B5:
	case D3DFMT_R5G6B5:
		bitsPerPixel = 16;
		break;
	}

	return (bitsPerPixel << 24) | (dm.Width << 12) | dm.Height;
}


void Direct3DDisplay::createFont()
{
	if( !pFont ) {
		HRESULT hr = D3DXCreateFont(
			pDevice,
			14,
			0,
			FW_BOLD,
			1,
			FALSE,
			DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			DEFAULT_PITCH || FF_DONTCARE,
			_T("Arial"),
			&pFont );
		if( FAILED( hr ) ) {
			DXTRACE_ERR_MSGBOX( _T("createFont failed"), hr );
		}
	}
}


void Direct3DDisplay::destroyFont()
{
	if( pFont ) {
		pFont->Release();
		pFont = NULL;
	}
}


void Direct3DDisplay::createSurface()
{
	if( !emulatedImage ) {
		HRESULT hr = pDevice->CreateOffscreenPlainSurface(
			width, height,
			dpp.BackBufferFormat,
			D3DPOOL_DEFAULT,
			&emulatedImage,
			NULL );
		if( FAILED( hr ) ) {
			DXTRACE_ERR_MSGBOX( _T("createSurface failed"), hr );
		}
	}
}


void Direct3DDisplay::destroySurface()
{
	if( emulatedImage ) {
		emulatedImage->Release();
		emulatedImage = NULL;
	}
}


void Direct3DDisplay::calculateDestRect()
{
	float scaleX = (float)dpp.BackBufferWidth / (float)width;
	float scaleY = (float)dpp.BackBufferHeight / (float)height;
	float min = (scaleX < scaleY) ? scaleX : scaleY;
	if( theApp.fsMaxScale && (min > theApp.fsMaxScale) ) {
		min = (float)theApp.fsMaxScale;
	}
	destRect.left = 0;
	destRect.top = 0;
	destRect.right = (LONG)(width * min);
	destRect.bottom = (LONG)(height * min);
	if( destRect.right != dpp.BackBufferWidth ) {
		LONG diff = (dpp.BackBufferWidth - destRect.right) / 2;
		destRect.left += diff;
		destRect.right += diff;
	}
	if( destRect.bottom != dpp.BackBufferHeight ) {
		LONG diff = (dpp.BackBufferHeight - destRect.bottom) / 2;
		destRect.top += diff;
		destRect.bottom += diff;
	}
}


void Direct3DDisplay::setOption( const char *option, int value )
{
	if( !_tcscmp( option, _T("vsync") ) ) {
		dpp.PresentationInterval = value ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
		resetDevice();
	}

	if( !_tcscmp( option, _T("tripleBuffering") ) ) {
		dpp.BackBufferCount = value ? 2 : 1;
		resetDevice();
	}

	if( !_tcscmp( option, _T("d3dFilter") ) ) {
		switch( value )
		{
		case 0: //point
			filter = D3DTEXF_POINT;
			break;
		case 1: //linear
			filter = D3DTEXF_LINEAR;
			break;
		}
	}

	if( !_tcscmp( option, _T("maxScale") ) ) {
		calculateDestRect();
	}
}


bool Direct3DDisplay::resetDevice()
{
	if( !pDevice ) return false;

	HRESULT hr;
	destroyFont();
	destroySurface();
	if( FAILED( hr = pDevice->Reset( &dpp ) ) ) {
		//DXTRACE_ERR_MSGBOX( _T("pDevice->Reset failed"), hr );
		failed = true;
		return false;
	}
	createFont();
	createSurface();
	failed = false;
	return true;
}


IDisplay *newDirect3DDisplay()
{
	return new Direct3DDisplay();
}
