// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2005-2006 VBA development team
// Copyright (C) 2007-2008 VBA-M development team

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

#include <memory.h>

// Direct3D
#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif
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
	bool                  initialized;
	LPDIRECT3D9           pD3D;
	LPDIRECT3DDEVICE9     pDevice;
	D3DDISPLAYMODE		  mode;
	D3DPRESENT_PARAMETERS dpp;
	D3DFORMAT             screenFormat;
	LPDIRECT3DTEXTURE9    emulatedImage[2];
	unsigned char         mbCurrentTexture; // current texture for motion blur
	bool                  mbTextureEmpty;
	unsigned int          width, height;
	unsigned int          textureSize;
	RECT                  destRect;
	bool                  failed;
	ID3DXFont             *pFont;
	bool                  rectangleFillsScreen;

	struct VERTEX {
		FLOAT x, y, z, rhw; // screen coordinates
		FLOAT tx, ty;       // texture coordinates
	} Vertices[4];
	// Vertices order:
	// 1 3
	// 0 2

	struct TRANSP_VERTEX {
		FLOAT x, y, z, rhw;
		D3DCOLOR color;
		FLOAT tx, ty;
	} transpVertices[4];

	void createFont();
	void destroyFont();
	bool clearTexture( LPDIRECT3DTEXTURE9 texture, size_t textureHeight );
	void createTexture( unsigned int textureWidth, unsigned int textureHeight );
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

	virtual bool changeRenderSize( int w, int h );
	virtual void resize( int w, int h );
	virtual void setOption( const char *option, int value );
	virtual int  selectFullScreenMode( GUID ** );
};


Direct3DDisplay::Direct3DDisplay()
{
	initialized = false;
	pD3D = NULL;
	pDevice = NULL;
	screenFormat = D3DFMT_X8R8G8B8;
	width = 0;
	height = 0;
	textureSize = 0;
	failed = false;
	pFont = NULL;
	emulatedImage[0] = NULL;
	emulatedImage[1] = NULL;
	mbCurrentTexture = 0;
	mbTextureEmpty = true;
	rectangleFillsScreen = false;
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
	dpp.Flags = 0;
	dpp.PresentationInterval = theApp.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	// D3DPRESENT_INTERVAL_ONE means VSync ON


#ifdef _DEBUG
	// make debugging full screen easier
	if( dpp.Windowed == FALSE ) {
		dpp.Windowed = TRUE;
		dpp.BackBufferFormat = D3DFMT_UNKNOWN;
		dpp.BackBufferCount = 0;
		dpp.FullScreen_RefreshRateInHz = 0;
		dpp.Flags = 0;
	}

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

	// load Direct3D v9
	pD3D = Direct3DCreate9( D3D_SDK_VERSION );

	if(pD3D == NULL) {
		DXTRACE_ERR_MSGBOX( _T("Error creating Direct3D object"), 0 );
		return false;
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
		theApp.m_pMainWnd->GetSafeHwnd(),
		D3DCREATE_FPU_PRESERVE |
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&dpp,
		&pDevice);
	if( FAILED( hret ) ) {
		DXTRACE_ERR_MSGBOX( _T("Error creating Direct3D device"), hret );
		return false;
	}

	createFont();
	// width and height will be set from a prior call to changeRenderSize() before initialize()
	createTexture( width, height );
	calculateDestRect();
	setOption( _T("d3dFilter"), theApp.d3dFilter );
	setOption( _T("motionBlur"), theApp.d3dMotionBlur );

	if(failed) return false;

	initialized = true;

#ifdef _DEBUG
	TRACE( _T("} Finished Direct3D renderer initialization\n\n") );
#endif

	return TRUE;
}


void Direct3DDisplay::clear()
{
	if( pDevice ) {
#ifdef _DEBUG
		pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0xFF,0x00,0xFF), 0.0f, 0 );
#else
		pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0x00,0x00,0x00), 0.0f, 0 );
#endif
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

	if( !rectangleFillsScreen ) {
		// performance: clear only when you must
		clear();
	}

	pDevice->BeginScene();

	// copy pix to emulatedImage and apply pixel filter if selected
	D3DLOCKED_RECT lr;
	const RECT target = { 0, 0, width, height };

	if( FAILED( hr = emulatedImage[ mbCurrentTexture ]->LockRect( 0, &lr, &target, 0 ) ) ) {
		DXTRACE_ERR_MSGBOX( _T("Can not lock texture"), hr );
		return;
	} else {
		unsigned short pitch = theApp.sizeX * ( systemColorDepth >> 3 ) + 4;
		if( theApp.filterFunction ) {
			// pixel filter enabled
			theApp.filterFunction(
				pix + pitch,
				pitch,
				(u8*)theApp.delta,
				(u8*)lr.pBits,
				lr.Pitch,
				theApp.sizeX,
				theApp.sizeY
				);
		} else {
			// pixel filter disabled
			switch( systemColorDepth )
			{
			case 32:
				cpyImg32(
					(unsigned char *)lr.pBits,
					lr.Pitch,
					pix + pitch,
					pitch,
					theApp.sizeX,
					theApp.sizeY
					);
				break;
			case 16:
				cpyImg16(
					(unsigned char *)lr.pBits,
					lr.Pitch,
					pix + pitch,
					pitch,
					theApp.sizeX,
					theApp.sizeY
					);
				break;
			}
		}
		emulatedImage[ mbCurrentTexture ]->UnlockRect( 0 );
	}


	if( !theApp.d3dMotionBlur ) {
		// draw the current frame to the screen
		pDevice->SetTexture( 0, emulatedImage[ mbCurrentTexture ] );
		pDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_TEX1 );
		pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, Vertices, sizeof(VERTEX) );
	} else {
		// Motion Blur enabled
		if( !mbTextureEmpty ) {
			// draw previous frame to the screen
			pDevice->SetTexture( 0, emulatedImage[ mbCurrentTexture ^ 1 ] );
			pDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_TEX1 );
			pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, Vertices, sizeof(VERTEX) );
			// draw the current frame with transparency to the screen
			pDevice->SetTexture( 0, emulatedImage[ mbCurrentTexture ] );
			pDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 );
			pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, transpVertices, sizeof(TRANSP_VERTEX) );
		} else {
			mbTextureEmpty = false;
			// draw the current frame to the screen
			pDevice->SetTexture( 0, emulatedImage[ mbCurrentTexture ] );
			pDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_TEX1 );
			pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, Vertices, sizeof(VERTEX) );
		}
		mbCurrentTexture ^= 1; // switch current texture
	}


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
		width = (unsigned int)w;
		height = (unsigned int)h;
		if( pDevice ) {
			destroyTexture();
			createTexture( width, height );
			calculateDestRect();
		}
	}
	return true;
}


void Direct3DDisplay::resize( int w, int h )
{
	if( !initialized ) {
		return;
	}

	if( (w != dpp.BackBufferWidth) ||
		(h != dpp.BackBufferHeight) ||
		(theApp.videoOption > VIDEO_4X) ) {
		resetDevice();
		calculateDestRect();
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


// fill texture completely with black
bool Direct3DDisplay::clearTexture( LPDIRECT3DTEXTURE9 texture, size_t textureHeight )
{
	D3DLOCKED_RECT lr;
	HRESULT hr;

	if( FAILED( hr = texture->LockRect( 0, &lr, NULL, 0 ) ) ) {
		DXTRACE_ERR_MSGBOX( _T("Can not lock texture"), hr );
		return false;
	} else {
		memset( lr.pBits, 0x00, lr.Pitch * textureHeight );
		texture->UnlockRect( 0 );
		return true;
	}
}


// when either textureWidth or textureHeight is 0, last texture size will be used
void Direct3DDisplay::createTexture( unsigned int textureWidth, unsigned int textureHeight )
{
	if( ( textureWidth != 0 ) && ( textureWidth != 0 ) ) {
		// calculate next possible square texture size
		textureSize = 1;
		unsigned int reqSizeMin = ( textureWidth > textureHeight ) ? textureWidth : textureHeight;
		while( textureSize < reqSizeMin ) {
			textureSize <<= 1; // multiply by 2
		}
	} else {
		// do not recalculate texture size

		if( textureSize == 0 ) {
			DXTRACE_MSG( _T("Error: createTexture: textureSize == 0") );
			return;
		}
	}


	if( !emulatedImage[0] ) {
		HRESULT hr = pDevice->CreateTexture(
			textureSize, textureSize,
			1, // 1 level, no mipmaps
			D3DUSAGE_DYNAMIC, // dynamic textures can be locked
			dpp.BackBufferFormat,
			D3DPOOL_DEFAULT,
			&emulatedImage[0],
			NULL );

		if( FAILED( hr ) ) {
			DXTRACE_ERR_MSGBOX( _T("createTexture(0) failed"), hr );
			return;
		}

		// initialize whole texture with black since we might see
		// the initial noise when using bilinear texture filtering
		clearTexture( emulatedImage[0], textureSize );
	}

	if( !emulatedImage[1] && theApp.d3dMotionBlur ) {
		HRESULT hr = pDevice->CreateTexture(
			textureSize, textureSize,
			1,
			D3DUSAGE_DYNAMIC,
			dpp.BackBufferFormat,
			D3DPOOL_DEFAULT,
			&emulatedImage[1],
			NULL );

		if( FAILED( hr ) ) {
			DXTRACE_ERR_MSGBOX( _T("createTexture(1) failed"), hr );
			return;
		}

		clearTexture( emulatedImage[1], textureSize );
		mbTextureEmpty = true;
	}
}


void Direct3DDisplay::destroyTexture()
{
	if( emulatedImage[0] ) {
		emulatedImage[0]->Release();
		emulatedImage[0] = NULL;
	}

	if( emulatedImage[1] ) {
		emulatedImage[1]->Release();
		emulatedImage[1] = NULL;
	}
}


void Direct3DDisplay::calculateDestRect()
{
	if( theApp.fullScreenStretch ) {
		rectangleFillsScreen = true; // no clear() necessary
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

		if( ( destRect.left == 0 ) &&
			( destRect.top == 0 ) &&
			( destRect.right == dpp.BackBufferWidth ) &&
			( destRect.bottom == dpp.BackBufferHeight ) ) {
				rectangleFillsScreen = true;
		} else {
				rectangleFillsScreen = false;
		}
	}

	FLOAT textureX = (FLOAT)width / (FLOAT)textureSize;
	FLOAT textureY = (FLOAT)height / (FLOAT)textureSize;

	// configure triangles
	Vertices[0].x = (FLOAT)destRect.left - 0.5f;
	// -0.5f is necessary in order to match texture alignment to display pixels
	Vertices[0].y = (FLOAT)destRect.bottom - 0.5f;
	Vertices[0].z = 0.0f;
	Vertices[0].rhw = 1.0f;
	Vertices[0].tx = 0.0f;
	Vertices[0].ty = textureY;
	Vertices[1].x = (FLOAT)destRect.left - 0.5f;
	Vertices[1].y = (FLOAT)destRect.top - 0.5f;
	Vertices[1].z = 0.0f;
	Vertices[1].rhw = 1.0f;
	Vertices[1].tx = 0.0f;
	Vertices[1].ty = 0.0f;
	Vertices[2].x = (FLOAT)destRect.right - 0.5f;
	Vertices[2].y = (FLOAT)destRect.bottom - 0.5f;
	Vertices[2].z = 0.0f;
	Vertices[2].rhw = 1.0f;
	Vertices[2].tx = textureX;
	Vertices[2].ty = textureY;
	Vertices[3].x = (FLOAT)destRect.right - 0.5f;
	Vertices[3].y = (FLOAT)destRect.top - 0.5f;
	Vertices[3].z = 0.0f;
	Vertices[3].rhw = 1.0f;
	Vertices[3].tx = textureX;
	Vertices[3].ty = 0.0f;

	if( theApp.d3dMotionBlur ) {
		// configure semi-transparent triangles
		D3DCOLOR semiTrans = D3DCOLOR_ARGB( 0x7F, 0xFF, 0xFF, 0xFF );
		transpVertices[0].x = Vertices[0].x;
		transpVertices[0].y = Vertices[0].y;
		transpVertices[0].z = Vertices[0].z;
		transpVertices[0].rhw = Vertices[0].rhw;
		transpVertices[0].color = semiTrans;
		transpVertices[0].tx = Vertices[0].tx;
		transpVertices[0].ty = Vertices[0].ty;
		transpVertices[1].x = Vertices[1].x;
		transpVertices[1].y = Vertices[1].y;
		transpVertices[1].z = Vertices[1].z;
		transpVertices[1].rhw = Vertices[1].rhw;
		transpVertices[1].color = semiTrans;
		transpVertices[1].tx = Vertices[1].tx;
		transpVertices[1].ty = Vertices[1].ty;
		transpVertices[2].x = Vertices[2].x;
		transpVertices[2].y = Vertices[2].y;
		transpVertices[2].z = Vertices[2].z;
		transpVertices[2].rhw = Vertices[2].rhw;
		transpVertices[2].color = semiTrans;
		transpVertices[2].tx = Vertices[2].tx;
		transpVertices[2].ty = Vertices[2].ty;
		transpVertices[3].x = Vertices[3].x;
		transpVertices[3].y = Vertices[3].y;
		transpVertices[3].z = Vertices[3].z;
		transpVertices[3].rhw = Vertices[3].rhw;
		transpVertices[3].color = semiTrans;
		transpVertices[3].tx = Vertices[3].tx;
		transpVertices[3].ty = Vertices[3].ty;
	}
}


void Direct3DDisplay::setOption( const char *option, int value )
{
	if( !_tcscmp( option, _T("vsync") ) ) {
		// value of theApp.vsync has already been changed by the menu handler
		// 'value' has the same value as theApp.vsync
		resetDevice();
	}

	if( !_tcscmp( option, _T("tripleBuffering") ) ) {
		// value of theApp.tripleBuffering has already been changed by the menu handler
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

	if( !_tcscmp( option, _T("motionBlur") ) ) {
		switch( value )
		{
		case 0:
			mbCurrentTexture = 0;
			pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
			break;
		case 1:
			// enable vertex alpha blending
			pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			pDevice->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1 );
			pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			// apply vertex alpha values to texture
			pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
			calculateDestRect();
			createTexture( 0, 0 ); // create the second texture
			break;
		}
	}
}


bool Direct3DDisplay::resetDevice()
{
	if( !pDevice ) return false;

	HRESULT hr;
	if( pFont ) {
		// prepares font for reset
		pFont->OnLostDevice();
	}
	destroyTexture();
	prepareDisplayMode();

	if( FAILED( hr = pDevice->Reset( &dpp ) ) ) {
		DXTRACE_ERR( _T("pDevice->Reset failed\n"), hr );
		failed = true;
		return false;
	}

	if( pFont ) {
		// re-aquires font resources
		pFont->OnResetDevice();
	}
	createTexture( 0, 0 );
	setOption( _T("d3dFilter"), theApp.d3dFilter );
	setOption( _T("motionBlur"), theApp.d3dMotionBlur );
	failed = false;
	return true;
}


IDisplay *newDirect3DDisplay()
{
	return new Direct3DDisplay();
}

#endif