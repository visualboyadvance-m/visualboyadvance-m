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

#include "MainWnd.h"

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../Text.h"
#include "../Util.h"
#include "../gb/gbGlobals.h"

#include <cmath>

// OpenGL
#include <gl/GL.h> // main include file
typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALFARPROC)( int );

extern int Init_2xSaI(u32);
extern void winlog(const char *,...);
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


class OpenGLDisplay : public IDisplay {
private:
	HDC hDC;
	HGLRC hglrc;
	GLuint texture;
	int width;
	int height;
	float size;
	u8 *filterData;
	RECT destRect;
	bool failed;

	void initializeMatrices( int w, int h );
	bool initializeTexture( int w, int h );
	void updateFiltering( int value );
	void setVSync( int interval = 1 );
	void calculateDestRect( int w, int h );

public:
	OpenGLDisplay();
	virtual ~OpenGLDisplay();
	virtual DISPLAY_TYPE getType() { return OPENGL; };
	
	virtual bool initialize();
	virtual void cleanup();
	virtual void render();
	virtual void renderMenu();
	virtual void clear();
	virtual bool changeRenderSize( int w, int h );
	virtual void resize( int w, int h );
	virtual void setOption( const char *, int );
	virtual int  selectFullScreenMode( GUID ** );
};


OpenGLDisplay::OpenGLDisplay()
{
	hDC = NULL;
	hglrc = NULL;
	texture = 0;
	width = 0;
	height = 0;
	size = 0.0f;
	filterData = (u8 *)malloc(4*4*256*240);
	failed = false;
}


OpenGLDisplay::~OpenGLDisplay()
{
	cleanup();
}


void OpenGLDisplay::cleanup()
{
	if(texture != 0) {
		glDeleteTextures(1, &texture);
		texture = 0;
	}

	if(hglrc != NULL) {
		wglDeleteContext(hglrc);
		wglMakeCurrent(NULL, NULL);
		hglrc = NULL;
	}
	
	if(hDC != NULL) {
		ReleaseDC(*theApp.m_pMainWnd, hDC);
		hDC = NULL;
	}
	
	if(filterData) {
		free(filterData);
		filterData = NULL;
	}
	
	width = 0;
	height = 0;
	size = 0.0f;
}


bool OpenGLDisplay::initialize()
{
	switch( theApp.cartridgeType )
	{
	case IMAGE_GBA:
		theApp.sizeX = 240;
		theApp.sizeY = 160;
		break;
	case IMAGE_GB:
		if ( gbBorderOn )
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
		{
			if( theApp.fullScreenStretch ) {
				theApp.surfaceSizeX = theApp.fsWidth;
				theApp.surfaceSizeY = theApp.fsHeight;
			} else {
				float scaleX = (float)theApp.fsWidth / (float)theApp.sizeX;
				float scaleY = (float)theApp.fsHeight / (float)theApp.sizeY;
				float min = ( scaleX < scaleY ) ? scaleX : scaleY;
				if( theApp.fsMaxScale )
					min = ( min > (float)theApp.fsMaxScale ) ? (float)theApp.fsMaxScale : min;
				theApp.surfaceSizeX = (int)((float)theApp.sizeX * min);
				theApp.surfaceSizeY = (int)((float)theApp.sizeY * min);
			}
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

	if( theApp.videoOption <= VIDEO_4X )
		style |= WS_OVERLAPPEDWINDOW;
	else
		styleEx = 0;

	if( theApp.videoOption <= VIDEO_4X )
		AdjustWindowRectEx( &theApp.dest, style, TRUE, styleEx );
	else
		AdjustWindowRectEx( &theApp.dest, style, FALSE, styleEx );    

	int winSizeX = theApp.dest.right - theApp.dest.left;
	int winSizeY = theApp.dest.bottom - theApp.dest.top;
	int x = 0, y = 0;

	if( theApp.videoOption <= VIDEO_4X ) {
		x = theApp.windowPositionX;
		y = theApp.windowPositionY;
	} else {
		winSizeX = theApp.fsWidth;
		winSizeY = theApp.fsHeight;
	}

	// Create a window
	MainWnd *pWnd = new MainWnd;
	theApp.m_pMainWnd = pWnd;

	pWnd->CreateEx(
		styleEx,
		theApp.wndClass,
		"VisualBoyAdvance",
		style,
		x,y,winSizeX,winSizeY,
		NULL,
		0 );
	
	if (!(HWND)*pWnd) {
		winlog("Error creating Window %08x\n", GetLastError());
		return FALSE;
	}
	
	theApp.updateMenuBar();
	
	theApp.adjustDestRect();
	
	theApp.mode320Available = FALSE;
	theApp.mode640Available = FALSE;
	theApp.mode800Available = FALSE;

	CDC *dc = pWnd->GetDC();
	HDC hDC = dc->GetSafeHdc();

	PIXELFORMATDESCRIPTOR pfd = { 
		sizeof(PIXELFORMATDESCRIPTOR),
		1,                     // version number 
		PFD_DRAW_TO_WINDOW |   // support window 
		PFD_SUPPORT_OPENGL |   // support OpenGL 
		PFD_DOUBLEBUFFER,      // double buffered 
		PFD_TYPE_RGBA,         // RGBA type 
		24,                    // 24-bit color depth 
		0, 0, 0, 0, 0, 0,      // color bits ignored 
		0,                     // no alpha buffer 
		0,                     // shift bit ignored 
		0,                     // no accumulation buffer 
		0, 0, 0, 0,            // accum bits ignored 
		32,                    // 32-bit z-buffer     
		0,                     // no stencil buffer 
		0,                     // no auxiliary buffer 
		PFD_MAIN_PLANE,        // main layer 
		0,                     // reserved 
		0, 0, 0                // layer masks ignored 
	};
	
	int  iPixelFormat; 
	if( !(iPixelFormat = ChoosePixelFormat( hDC, &pfd )) ) {
		winlog( "Failed ChoosePixelFormat\n" );
		return false;
	}
	
	// obtain detailed information about
	// the device context's first pixel format
	if( !( DescribePixelFormat(
		hDC,
		iPixelFormat,
		sizeof(PIXELFORMATDESCRIPTOR),
		&pfd ) ) )
	{
		winlog( "Failed DescribePixelFormat\n" );
		return false;
	}
	
	if( !SetPixelFormat( hDC, iPixelFormat, &pfd ) ) {
		winlog( "Failed SetPixelFormat\n" );
		return false;
	}
	
	if( !( hglrc = wglCreateContext( hDC ) ) ) {
		winlog( "Failed wglCreateContext\n" );
		return false;
	}
	
	if( !wglMakeCurrent(hDC, hglrc) ) {
		winlog( "Failed wglMakeCurrent\n" );
		return false;
	}
	
	pWnd->ReleaseDC( dc );
	
	// setup 2D gl environment
	glPushAttrib( GL_ENABLE_BIT );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glEnable( GL_TEXTURE_2D );

	initializeMatrices( theApp.surfaceSizeX, theApp.surfaceSizeY );

	setVSync( theApp.vsync );

#ifdef MMX
	if(!theApp.disableMMX)
		cpu_mmx = theApp.detectMMX();
	else
		cpu_mmx = 0;
#endif

	systemRedShift = 3;
	systemGreenShift = 11;
	systemBlueShift = 19;
	systemColorDepth = 32;
	theApp.fsColorDepth = 32;
	
	Init_2xSaI(32);
	
	utilUpdateSystemColorMaps();
	theApp.updateFilter();
	theApp.updateIFB();
	
	if(failed)
		return false;
	
	pWnd->DragAcceptFiles(TRUE);

	return TRUE;  
}


void OpenGLDisplay::clear()
{
	glClear( GL_COLOR_BUFFER_BIT );
}


void OpenGLDisplay::renderMenu()
{
	checkFullScreen();
	if( theApp.m_pMainWnd )
		theApp.m_pMainWnd->DrawMenuBar();
}


void OpenGLDisplay::render()
{
	clear();

	int pitch = theApp.filterWidth * 4 + 4;
	u8 *data = pix + ( theApp.sizeX + 1 ) * 4;
	
	// apply pixel filter
	if(theApp.filterFunction) {
		data = filterData;
		theApp.filterFunction(
			pix + pitch,
			pitch,
			(u8*)theApp.delta,
			(u8*)filterData,
			theApp.filterWidth * 4 * 2,
			theApp.filterWidth,
			theApp.filterHeight);
	}

	// Texturemap complete texture to surface
	// so we have free scaling and antialiasing
	int mult;
	if( theApp.filterFunction ) {
		glPixelStorei( GL_UNPACK_ROW_LENGTH, theApp.sizeX << 1 );
		mult = 2;
	} else {
		glPixelStorei( GL_UNPACK_ROW_LENGTH, theApp.sizeX + 1 );
		mult = 1;
	}
	
	glTexSubImage2D(
		GL_TEXTURE_2D,
		0,
		0,
		0,
		mult * theApp.sizeX,
		mult * theApp.sizeY,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		data );

	if( theApp.glType == 0 ) {
		glBegin( GL_TRIANGLE_STRIP );

		glTexCoord2f( 0.0f, 0.0f );
		glVertex3i( 0, 0, 0 );

		glTexCoord2f( (float)(mult * theApp.sizeX) / size, 0.0f );
		glVertex3i( theApp.surfaceSizeX, 0, 0 );

		glTexCoord2f( 0.0f, (float)(mult * theApp.sizeY) / size );
		glVertex3i( 0, theApp.surfaceSizeY, 0 );

		glTexCoord2f( (float)(mult * theApp.sizeX) / size, (float)(mult * theApp.sizeY) / size );
		glVertex3i( theApp.surfaceSizeX, theApp.surfaceSizeY, 0 );

		glEnd();
	} else {
		glBegin( GL_QUADS );

		glTexCoord2f( 0.0f, 0.0f );
		glVertex3i( 0, 0, 0 );

		glTexCoord2f( (float)(mult * theApp.sizeX) / size, 0.0f );
		glVertex3i( theApp.surfaceSizeX, 0, 0 );

		glTexCoord2f( (float)(mult * theApp.sizeX) / size, (float)(mult * theApp.sizeY) / size );
		glVertex3i( theApp.surfaceSizeX, theApp.surfaceSizeY, 0 );

		glTexCoord2f( 0.0f, (float)(mult * theApp.sizeY) / size );
		glVertex3i( 0, theApp.surfaceSizeY, 0 );

		glEnd();
	}

	CDC *dc = theApp.m_pMainWnd->GetDC();

	SwapBuffers( dc->GetSafeHdc() );
	// since OpenGL draws on the back buffer,
	// we have to swap it to the front buffer to see it
	
	// draw informations with GDI on the front buffer
	dc->SetBkMode( theApp.showSpeedTransparent ? TRANSPARENT : OPAQUE );
	if( theApp.showSpeed && ( theApp.videoOption > VIDEO_4X ) ) {
		char buffer[30];
		if( theApp.showSpeed == 1 ) {
			sprintf( buffer, "%3d%%", systemSpeed );
		} else {
			sprintf( buffer, "%3d%%(%d, %d fps)", systemSpeed, systemFrameSkip, theApp.showRenderedFrames );
		}
		dc->SetTextColor( RGB(0x00, 0x00, 0xFF) );
		dc->TextOut( 10, 20, buffer );
	}
	if( theApp.screenMessage ) {
		if( ( ( GetTickCount() - theApp.screenMessageTime ) < 3000 ) && !theApp.disableStatusMessage ) {
			dc->SetTextColor( RGB(0xFF, 0x00, 0x00) );
			dc->TextOut( 10, theApp.surfaceSizeY - 20, theApp.screenMessageBuffer );
		} else {
			theApp.screenMessage = false;
		}
	}

	theApp.m_pMainWnd->ReleaseDC( dc );
}


void OpenGLDisplay::resize( int w, int h )
{
	initializeMatrices( w, h );
}


void OpenGLDisplay::updateFiltering( int value )
{
	switch( value )
	{
	case 0:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		break;
	case 1:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		break;
	}
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
}


void OpenGLDisplay::initializeMatrices( int w, int h )
{
	if( theApp.fullScreenStretch ) {
		glViewport( 0, 0, w, h );
	} else {
		calculateDestRect( w, h );
		glViewport(
			destRect.left,
			destRect.top,
			destRect.right - destRect.left,
			destRect.bottom - destRect.top );
	}

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho(
		/* left   */ 1.0f,
		/* right  */ (GLdouble)(w - 1),
		/* bottom */ (GLdouble)(h - 1),
		/* top    */ 1.0f,
		0.0f,
		1.0f );

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}


bool OpenGLDisplay::initializeTexture( int w, int h )
{
	// size = 2^n
	// w = 24  > size = 256 = 2^8
	// w = 255 > size = 256 = 2^8
	// w = 256 > size = 512 = 2^9
	// w = 300 > size = 512 = 2^9
	// OpenGL textures have to be square and a power of 2

	float n1 = log10( (float)w ) / log10( 2.0f );
	float n2 = log10( (float)h ) / log10( 2.0f );
	float n = ( n1 > n2 ) ? n1 : n2;

	if( ((float)((int)n)) != n ) {
		// round up
		n = ((float)((int)n)) + 1.0f;
	}

	size = pow( 2.0f, n );

	glGenTextures( 1, &texture );
	glBindTexture( GL_TEXTURE_2D, texture );
	updateFiltering( theApp.glFilter );

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		(GLsizei)size,
		(GLsizei)size,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		NULL );

	width = w;
	height = h;
	
	return ( glGetError() == GL_NO_ERROR) ? true : false;
}


void OpenGLDisplay::setVSync( int interval )
{
	const char *extensions = (const char *)glGetString( GL_EXTENSIONS );
	
	if( strstr( extensions, "WGL_EXT_swap_control" ) == 0 ) {
		winlog( "Error: WGL_EXT_swap_control extension not supported on your computer.\n" );
		return;
	} else {
		PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = NULL;
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress( "wglSwapIntervalEXT" );
		if( wglSwapIntervalEXT ) {
			wglSwapIntervalEXT( interval );
		}
	}
}


bool OpenGLDisplay::changeRenderSize( int w, int h )
{
	if( (width != w) || (height != h) ) {
		if( texture != 0 ) {
			glDeleteTextures( 1, &texture );
			texture = 0;
		}
		
		if( !initializeTexture( w, h ) ) {
			failed = true;
			return false;
		}
	}
	
	return true;
}


void OpenGLDisplay::calculateDestRect( int w, int h )
{
	float scaleX = (float)w / (float)width;
	float scaleY = (float)h / (float)height;
	float min = (scaleX < scaleY) ? scaleX : scaleY;
	if( theApp.fsMaxScale && (min > theApp.fsMaxScale) ) {
		min = (float)theApp.fsMaxScale;
	}
	destRect.left = 0;
	destRect.top = 0;
	destRect.right = (LONG)(width * min);
	destRect.bottom = (LONG)(height * min);
	if( destRect.right != w ) {
		LONG diff = (w - destRect.right) / 2;
		destRect.left += diff;
		destRect.right += diff;
	}
	if( destRect.bottom != h ) {
		LONG diff = (h - destRect.bottom) / 2;
		destRect.top += diff;
		destRect.bottom += diff;
	}
}


void OpenGLDisplay::setOption( const char *option, int value )
{
	if( !_tcscmp( option, _T("vsync") ) ) {
		setVSync( value );
	}

	if( !_tcscmp( option, _T("glFilter") ) ) {
		updateFiltering( value );
	}

	if( !_tcscmp( option, _T("maxScale") ) ) {
		initializeMatrices( theApp.dest.right, theApp.dest.bottom );
	}

	if( !_tcscmp( option, _T("fullScreenStretch") ) ) {
		initializeMatrices( theApp.dest.right, theApp.dest.bottom );
	}
}


int OpenGLDisplay::selectFullScreenMode( GUID ** )
{
	HWND wnd = GetDesktopWindow();
	RECT r;
	GetWindowRect( wnd, &r );
	int w = ( r.right - r.left ) & 0xFFF;
	int h = ( r.bottom - r.top ) & 0xFFF;
	HDC dc = GetDC( wnd );
	int c = GetDeviceCaps( dc, BITSPIXEL );
	ReleaseDC( wnd, dc );
	return (c << 24) | (w << 12) | h;
}


IDisplay *newOpenGLDisplay()
{
	return new OpenGLDisplay();
}
