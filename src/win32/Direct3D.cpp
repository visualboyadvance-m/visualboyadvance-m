// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2005-2006 VBA development team
// Copyright (C) 2007 VBA-M team

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

#ifndef NO_D3D

#pragma comment( lib, "d3d9.lib" )
#pragma comment( lib, "d3dx9.lib" )
#pragma comment( lib, "dxerr9.lib" )

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
#include <D3dx9core.h> // required for font rendering
#include <Dxerr.h>     // contains debug functions

extern int Init_2xSaI(u32); // initializes all pixel filters
extern int systemSpeed;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
extern void log(const char *,...);
#endif

#ifdef MMX
extern "C" bool cpu_mmx;
extern bool detectMMX();
#endif

extern int winVideoModeSelect(CWnd *, GUID **);

class Direct3DDisplay : public IDisplay {
private:
	bool                  initializing;
	LPDIRECT3D9           pD3D;
	LPDIRECT3DDEVICE9     pDevice;
	D3DDISPLAYMODE		  mode;
	D3DPRESENT_PARAMETERS dpp;
	D3DFORMAT             screenFormat;
	LPDIRECT3DTEXTURE9    emulatedImage;
	int                   width;
	int                   height;
	RECT                  destRect;
	bool                  failed;
	ID3DXFont             *pFont;

	struct VERTEX {
		FLOAT x, y, z, rhw; // screen coordinates
		FLOAT tx, ty;       // texture coordinates
	} Vertices[4];
	// Vertices order:
	// 1 3
	// 0 2

	void createFont();
	void destroyFont();
	void createTexture();
	void destroyTexture();
	void calculateDestRect();
	bool resetDevice();
	void prepareDisplayMode();

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
	initializing = false;
	pD3D = NULL;
	pDevice = NULL;
	screenFormat = D3DFMT_X8R8G8B8;
	width = 0;
	height = 0;
	failed = false;
	pFont = NULL;
	emulatedImage = NULL;
}


Direct3DDisplay::~Direct3DDisplay()
{
	cleanup();
}

void Direct3DDisplay::prepareDisplayMode()
{
	// Change display mode
	memset(&dpp, 0, sizeof(dpp));
	dpp.Windowed = !( theApp.videoOption >= VIDEO_320x240 );
	if( !dpp.Windowed ) {
		dpp.BackBufferFormat = (theApp.fsColorDepth == 32) ? D3DFMT_X8R8G8B8 : D3DFMT_R5G6B5;
	} else {
		dpp.BackBufferFormat = mode.Format;
	}
	dpp.BackBufferCount = theApp.tripleBuffering ? 2 : 1;
	dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
	dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	dpp.BackBufferWidth = !dpp.Windowed ? theApp.fsWidth : theApp.surfaceSizeX;
	dpp.BackBufferHeight = !dpp.Windowed ? theApp.fsHeight : theApp.surfaceSizeY;
	dpp.hDeviceWindow = theApp.m_pMainWnd->GetSafeHwnd();
	dpp.FullScreen_RefreshRateInHz = dpp.Windowed ? 0 : theApp.fsFrequency;
	if( ( dpp.Windowed == FALSE ) && theApp.menuToggle ) {
		dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	} else {
		dpp.Flags = 0;
	}
	dpp.PresentationInterval = theApp.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	// D3DPRESENT_INTERVAL_ONE means VSync ON

	if( theApp.vsync && ( dpp.Windowed == FALSE ) && theApp.menuToggle ) {
		// VSync will be disabled when the menu is opened in full screen mode
		dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	}


#ifdef _DEBUG
	TRACE( _T("prepareDisplayMode:\n") );
	TRACE( _T("%i x %i @ %iHz:\n"), dpp.BackBufferWidth, dpp.BackBufferHeight, dpp.FullScreen_RefreshRateInHz );
	TRACE( _T("Buffer Count: %i\n"), dpp.BackBufferCount+1 );
	TRACE( _T("VSync: %i\n"), dpp.PresentationInterval==D3DPRESENT_INTERVAL_ONE );
	TRACE( _T("LOCKABLE_BACKBUFFER: %i\n\n"), dpp.Flags==D3DPRESENTFLAG_LOCKABLE_BACKBUFFER );
#endif
}


void Direct3DDisplay::cleanup()
{
	destroyFont();
	destroyTexture();

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
#ifdef _DEBUG
	TRACE( _T("Initializing Direct3D renderer {\n") );
#endif

	initializing = true;

	switch( theApp.cartridgeType )
	{
	case IMAGE_GBA:
		theApp.sizeX = 240;
		theApp.sizeY = 160;
		break;
	case IMAGE_GB:
		if(gbBorderOn) {
			theApp.sizeX = 256;
			theApp.sizeY = 224;
		} else {
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
	case VIDEO_1024x768:
	case VIDEO_1280x1024:
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

	if( !((HWND)*pWnd) ) {
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
	pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);

	theApp.mode320Available = FALSE;
	theApp.mode640Available = FALSE;
	theApp.mode800Available = FALSE;
	theApp.mode1024Available = FALSE;
	theApp.mode1280Available = FALSE;

	unsigned int nModes, i;
	D3DDISPLAYMODE dm;

	nModes = pD3D->GetAdapterModeCount(theApp.fsAdapter, mode.Format);
	for( i = 0; i<nModes; i++ )
	{
		if( D3D_OK == pD3D->EnumAdapterModes(theApp.fsAdapter, mode.Format, i, &dm) )
		{
			if( (dm.Width == 320) && (dm.Height == 240) )
				theApp.mode320Available = true;
			if( (dm.Width == 640) && (dm.Height == 480) )
				theApp.mode640Available = true;
			if( (dm.Width == 800) && (dm.Height == 600) )
				theApp.mode800Available = true;
			if( (dm.Width == 1024) && (dm.Height == 768) )
				theApp.mode1024Available = true;
			if( (dm.Width == 1280) && (dm.Height == 1024) )
				theApp.mode1280Available = true;
		}
	}


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
	prepareDisplayMode();

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
	createTexture();
	setOption( _T("d3dFilter"), theApp.d3dFilter );
	calculateDestRect();

	initializing = false;

	if(failed) return false;

#ifdef _DEBUG
	TRACE( _T("} Finished Direct3D renderer initialization\n\n") );
#endif

	return TRUE;
}


void Direct3DDisplay::renderMenu()
{
	//checkFullScreen();
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

	// Multi-Tasking fix
	HRESULT hr = pDevice->TestCooperativeLevel();
	if( FAILED( hr ) ) {
		switch( hr ) {
		case D3DERR_DEVICELOST:
			// The device has been lost but cannot be reset at this time.
			// Therefore, rendering is not possible.
			return;
		case D3DERR_DEVICENOTRESET:
			// The device has been lost but can be reset at this time.
			resetDevice();
			return;
		default:
			DXTRACE_ERR( _T("ERROR: D3D device has serious problems"), hr );
			return;
		}
	}

	clear();

	pDevice->BeginScene();

	// copy pix to emulatedImage and apply pixel filter if selected
	D3DLOCKED_RECT lr;
	
	if( FAILED( hr = emulatedImage->LockRect( 0, &lr, NULL, D3DLOCK_DISCARD ) ) ) {
		DXTRACE_ERR_MSGBOX( _T("Can not lock texture"), hr );
		return;
	} else {
		if( !theApp.filterFunction ) {
			copyImage(
				pix,
				lr.pBits,
				(systemColorDepth == 32) ? theApp.sizeX
				: theApp.sizeX + 1, // TODO: workaround results in one pixel black border at right side
				theApp.sizeY,
				lr.Pitch,
				systemColorDepth
				);
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
		emulatedImage->UnlockRect( 0 );
	}

	// set emulatedImage as active texture
	pDevice->SetTexture( 0, emulatedImage );

	// render textured triangles
	pDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_TEX1 );
	pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, Vertices, sizeof(VERTEX) );

	// render speed and status messages
	D3DCOLOR color;
	RECT r;
	r.left = 0;
	r.top = 0;
	r.right = dpp.BackBufferWidth - 1;
	r.bottom = dpp.BackBufferHeight - 1;

	if( theApp.showSpeed && ( theApp.videoOption > VIDEO_4X ) ) {
		color = theApp.showSpeedTransparent ? D3DCOLOR_ARGB(0x7F, 0x00, 0x00, 0xFF) : D3DCOLOR_ARGB(0xFF, 0x00, 0x00, 0xFF);
		char buffer[30];
		if( theApp.showSpeed == 1 ) {
			sprintf( buffer, "%3d%%", systemSpeed );
		} else {
			sprintf( buffer, "%3d%%(%d, %d fps)", systemSpeed, systemFrameSkip, theApp.showRenderedFrames );
		}

		pFont->DrawText( NULL, buffer, -1, &r, DT_CENTER | DT_TOP, color );
	}

	if( theApp.screenMessage ) {
		color = theApp.showSpeedTransparent ? D3DCOLOR_ARGB(0x7F, 0xFF, 0x00, 0x00) : D3DCOLOR_ARGB(0xFF, 0xFF, 0x00, 0x00);
		if( ( ( GetTickCount() - theApp.screenMessageTime ) < 3000 ) && !theApp.disableStatusMessage && pFont ) {
			pFont->DrawText( NULL, theApp.screenMessageBuffer, -1, &r, DT_CENTER | DT_BOTTOM, color );
		} else {
			theApp.screenMessage = false;
		}
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
			destroyTexture();
			createTexture();
			calculateDestRect();
		}
	}
	return true;
}


void Direct3DDisplay::resize( int w, int h )
{
	if( initializing ) {
		return;
	}

	if( (w != dpp.BackBufferWidth) || (h != dpp.BackBufferHeight) ) {
		resetDevice();
		calculateDestRect();
	}
	if( theApp.videoOption > VIDEO_4X ) {
		resetDevice();
	}
}


int Direct3DDisplay::selectFullScreenMode( GUID **pGUID )
{
  return winVideoModeSelect(theApp.m_pMainWnd, pGUID);
}


void Direct3DDisplay::createFont()
{
	if( !pFont ) {
		HRESULT hr = D3DXCreateFont(
			pDevice,
			dpp.BackBufferHeight/20, // dynamic font size
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


void Direct3DDisplay::createTexture()
{
	if( !emulatedImage ) {
		HRESULT hr = pDevice->CreateTexture(
			width, height, 1, // 1 level, no mipmaps
			D3DUSAGE_DYNAMIC, dpp.BackBufferFormat,
			D3DPOOL_DEFAULT, // anything else won't work
			&emulatedImage,
			NULL );

		if( FAILED( hr ) ) {
			DXTRACE_ERR_MSGBOX( _T("createTexture failed"), hr );
		}
	}
}


void Direct3DDisplay::destroyTexture()
{
	if( emulatedImage ) {
		emulatedImage->Release();
		emulatedImage = NULL;
	}
}


void Direct3DDisplay::calculateDestRect()
{
	if( theApp.fullScreenStretch ) {
		destRect.left = 0;
		destRect.top = 0;
		destRect.right = dpp.BackBufferWidth;   // for some reason there'l be a black
		destRect.bottom = dpp.BackBufferHeight; // border line when using -1 at the end
	} else {
		// use aspect ratio
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



	// configure triangles
	Vertices[0].x = (FLOAT)destRect.left;
	Vertices[0].y = (FLOAT)destRect.bottom;
	Vertices[0].z = 0.5f;
	Vertices[0].rhw = 1.0f;
	Vertices[0].tx = 0.0f;
	Vertices[0].ty = 1.0f;
	Vertices[1].x = (FLOAT)destRect.left;
	Vertices[1].y = (FLOAT)destRect.top;
	Vertices[1].z = 0.5f;
	Vertices[1].rhw = 1.0f;
	Vertices[1].tx = 0.0f;
	Vertices[1].ty = 0.0f;
	Vertices[2].x = (FLOAT)destRect.right;
	Vertices[2].y = (FLOAT)destRect.bottom;
	Vertices[2].z = 0.5f;
	Vertices[2].rhw = 1.0f;
	Vertices[2].tx = 1.0f;
	Vertices[2].ty = 1.0f;
	Vertices[3].x = (FLOAT)destRect.right;
	Vertices[3].y = (FLOAT)destRect.top;
	Vertices[3].z = 0.5f;
	Vertices[3].rhw = 1.0f;
	Vertices[3].tx = 1.0f;
	Vertices[3].ty = 0.0f;
}


void Direct3DDisplay::setOption( const char *option, int value )
{
	if( !_tcscmp( option, _T("vsync") ) ) {
		// theApp.vsync has already been changed by the menu handler
		// 'value' has the same value as theApp.vsync
		resetDevice();
	}

	if( !_tcscmp( option, _T("tripleBuffering") ) ) {
		// theApp.tripleBuffering has already been changed by the menu handler
		// 'value' has the same value as theApp.tripleBuffering
		resetDevice();
	}

	if( !_tcscmp( option, _T("d3dFilter") ) ) {
		switch( value )
		{
		case 0: //point
			pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
			pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
			break;
		case 1: //linear
			pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
			pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
			break;
		}
	}

	if( !_tcscmp( option, _T("maxScale") ) ) {
		calculateDestRect();
	}

	if( !_tcscmp( option, _T("fullScreenStretch") ) ) {
		calculateDestRect();
	}
}


bool Direct3DDisplay::resetDevice()
{
	if( !pDevice ) return false;

	HRESULT hr;
	if( pFont ) {
		// prepares font for rest
		pFont->OnLostDevice();
	}
	destroyTexture();
	prepareDisplayMode();

	if( dpp.Windowed == FALSE ) {
		// SetDialogBoxMode needs D3DPRESENTFLAG_LOCKABLE_BACKBUFFER
		if( FAILED( hr = pDevice->SetDialogBoxMode( theApp.menuToggle ? TRUE : FALSE ) ) ) {
			DXTRACE_ERR( _T("can not switch to dialog box mode"), hr );
		}
	}

	if( FAILED( hr = pDevice->Reset( &dpp ) ) ) {
		DXTRACE_ERR( _T("pDevice->Reset failed\n"), hr );
		failed = true;
		return false;
	}

	if( pFont ) {
		// re-aquires font resources
		pFont->OnResetDevice();
	}
	createTexture();
	setOption( _T("d3dFilter"), theApp.d3dFilter );
	failed = false;
	return true;
}


IDisplay *newDirect3DDisplay()
{
	return new Direct3DDisplay();
}

#endif