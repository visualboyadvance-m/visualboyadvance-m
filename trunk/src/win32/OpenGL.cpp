#ifndef NO_OGL

//OpenGL library
#pragma comment( lib, "OpenGL32" )

// MFC
#include "stdafx.h"

//GUI
#include "MainWnd.h"
#include "FullscreenSettings.h"

// Internals
#include "../System.h"
#include "../gba/GBA.h"
#include "../gba/Globals.h"
#include "../Util.h"
#include "../gb/gbGlobals.h"
#include "../common/memgzio.h"

//Math
#include <cmath>
#include <sys/stat.h>

// OpenGL
#include <gl/GL.h> // main include file
#include <GL/glu.h>
#include "glFont.h"
#include <gl/glext.h>
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
	HGLRC hRC;
	GLuint texture;
	int width,height;
	float size;
	u8 *filterData;
	RECT destRect;
	bool failed;
	GLFONT font;
	int pitch;
	u8 *data;
	DWORD currentAdapter;

	void initializeMatrices( int w, int h );
	bool initializeTexture( int w, int h );
	void updateFiltering( int value );
	void setVSync( int interval = 1 );
	void calculateDestRect( int w, int h );
	void initializeFont();

public:
	OpenGLDisplay();
	virtual ~OpenGLDisplay();
	virtual DISPLAY_TYPE getType() { return OPENGL; };

	virtual void EnableOpenGL();
	virtual void DisableOpenGL();
	virtual bool initialize();
	virtual void cleanup();
	virtual void clear();
	virtual void render();
	virtual bool changeRenderSize( int w, int h );
	virtual void resize( int w, int h );
	virtual void setOption( const char *, int );
	virtual bool selectFullScreenMode( VIDEO_MODE &mode );
};

#include "gzglfont.h"
//Load GL font
void OpenGLDisplay::initializeFont()
{
    int ret;
    z_stream strm;
	char *buf = (char *)malloc(GZGLFONT_SIZE);

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, 16+MAX_WBITS);
    if (ret != Z_OK)
        return;

    strm.avail_in = sizeof(gzglfont);
    strm.next_in = gzglfont;
    strm.avail_out = GZGLFONT_SIZE;
	strm.next_out = (Bytef *)buf;
    ret = inflate(&strm, Z_NO_FLUSH);
	if (ret==Z_STREAM_END)
	{
		glGenTextures( 1, &texture );
		glFontCreate(&font, (char *)buf, texture);
		texture=0;
	}
	free(buf);
    (void)inflateEnd(&strm);
}

//OpenGL class constructor
OpenGLDisplay::OpenGLDisplay()
{
	hDC = NULL;
	hRC = NULL;
	texture = 0;
	width = 0;
	height = 0;
	size = 0.0f;
	failed = false;
	filterData = NULL;
	currentAdapter = 0;
}

//OpenGL class destroyer
OpenGLDisplay::~OpenGLDisplay()
{
	cleanup();
}

//Set OpenGL PFD and contexts
void OpenGLDisplay::EnableOpenGL()
{
	PIXELFORMATDESCRIPTOR pfd;
	// get the device context (DC)
	hDC = GetDC( theApp.m_pMainWnd->GetSafeHwnd() );
	// set the pixel format for the DC
	ZeroMemory( &pfd, sizeof( pfd ) );
	pfd.nSize = sizeof( pfd );
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
    SetPixelFormat (GetDC (theApp.m_pMainWnd->GetSafeHwnd()), ChoosePixelFormat ( GetDC (theApp.m_pMainWnd->GetSafeHwnd()), &pfd), &pfd);
    wglMakeCurrent (GetDC (theApp.m_pMainWnd->GetSafeHwnd()), wglCreateContext(GetDC (theApp.m_pMainWnd->GetSafeHwnd()) ) );
}
//Remove contexts
void OpenGLDisplay::DisableOpenGL()
{
	wglMakeCurrent( NULL, NULL );
	wglDeleteContext( hRC );
	ReleaseDC( theApp.m_pMainWnd->GetSafeHwnd(), hDC );
}
//Remove resources used
void OpenGLDisplay::cleanup()
{
	if(texture != 0) {
		glDeleteTextures(1, &texture);
		texture = 0;
	}

	DisableOpenGL();
	if(filterData) {
		free(filterData);
		filterData = NULL;
	}
	width = 0;
	height = 0;
	size = 0.0f;

	DISPLAY_DEVICE dev;
	ZeroMemory( &dev, sizeof(dev) );
	dev.cb = sizeof(dev);
	EnumDisplayDevices( NULL, currentAdapter, &dev, 0 );
	// restore default video mode
	ChangeDisplaySettingsEx( dev.DeviceName, NULL, NULL, 0, NULL );
}

//init renderer
bool OpenGLDisplay::initialize()
{
	switch( theApp.cartridgeType )
	{
	case IMAGE_GBA:
		sizeX = 240;
		sizeY = 160;
		break;
	case IMAGE_GB:
		if ( gbBorderOn )
		{
			sizeX = 256;
			sizeY = 224;
		}
		else
		{
			sizeX = 160;
			sizeY = 144;
		}
		break;
	}


	switch(videoOption)
	{
	case VIDEO_1X:
		surfaceSizeX = sizeX;
		surfaceSizeY = sizeY;
		break;
	case VIDEO_2X:
		surfaceSizeX = sizeX * 2;
		surfaceSizeY = sizeY * 2;
		break;
	case VIDEO_3X:
		surfaceSizeX = sizeX * 3;
		surfaceSizeY = sizeY * 3;
		break;
	case VIDEO_4X:
		surfaceSizeX = sizeX * 4;
		surfaceSizeY = sizeY * 4;
		break;
	case VIDEO_5X:
		surfaceSizeX = sizeX * 5;
		surfaceSizeY = sizeY * 5;
		break;
	case VIDEO_6X:
		surfaceSizeX = sizeX * 6;
		surfaceSizeY = sizeY * 6;
		break;
	case VIDEO_320x240:
	case VIDEO_640x480:
	case VIDEO_800x600:
	case VIDEO_1024x768:
	case VIDEO_1280x1024:
	case VIDEO_OTHER:
		{
			if( fullScreenStretch ) {
				surfaceSizeX = fsWidth;
				surfaceSizeY = fsHeight;
			} else {
				float scaleX = (float)fsWidth / (float)sizeX;
				float scaleY = (float)fsHeight / (float)sizeY;
				float min = ( scaleX < scaleY ) ? scaleX : scaleY;
				if( maxScale )
					min = ( min > (float)maxScale ) ? (float)maxScale : min;
				surfaceSizeX = (int)((float)sizeX * min);
				surfaceSizeY = (int)((float)sizeY * min);
			}
		}
		break;
	}

	theApp.rect.left = 0;
	theApp.rect.top = 0;
	theApp.rect.right = sizeX;
	theApp.rect.bottom = sizeY;

	theApp.dest.left = 0;
	theApp.dest.top = 0;
	theApp.dest.right = surfaceSizeX;
	theApp.dest.bottom = surfaceSizeY;

	DWORD style = WS_POPUP | WS_VISIBLE;
	DWORD styleEx = 0;

	if( videoOption <= VIDEO_6X )
		style |= WS_OVERLAPPEDWINDOW;
	else
		styleEx = 0;

	if( videoOption <= VIDEO_6X )
		AdjustWindowRectEx( &theApp.dest, style, TRUE, styleEx );
	else
		AdjustWindowRectEx( &theApp.dest, style, FALSE, styleEx );

	int winSizeX = theApp.dest.right - theApp.dest.left;
	int winSizeY = theApp.dest.bottom - theApp.dest.top;
	int x = 0, y = 0;

	if( videoOption <= VIDEO_6X ) {
		x = windowPositionX;
		y = windowPositionY;
	} else {
		winSizeX = fsWidth;
		winSizeY = fsHeight;
	}

	

	theApp.updateMenuBar();

	theApp.adjustDestRect();

	currentAdapter = fsAdapter;
	DISPLAY_DEVICE dev;
	ZeroMemory( &dev, sizeof(dev) );
	dev.cb = sizeof(dev);
	EnumDisplayDevices( NULL, currentAdapter, &dev, 0 );
	if( videoOption >= VIDEO_320x240 ) {
		// enter full screen mode
		DEVMODE mode;
		ZeroMemory( &mode, sizeof(mode) );
		mode.dmSize = sizeof(mode);
		mode.dmBitsPerPel = fsColorDepth;
		mode.dmPelsWidth = fsWidth;
		mode.dmPelsHeight = fsHeight;
		mode.dmDisplayFrequency = fsFrequency;
		mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
		LONG ret = ChangeDisplaySettingsEx( dev.DeviceName, &mode, NULL, CDS_FULLSCREEN, NULL );
		if( ret != DISP_CHANGE_SUCCESSFUL ) {
			systemMessage( 0, "Can not change display mode!" );
			failed = true;
		}
	} else {
		// restore default mode
		ChangeDisplaySettingsEx( dev.DeviceName, NULL, NULL, 0, NULL );
	}

	EnableOpenGL();
	initializeFont();
	glPushAttrib( GL_ENABLE_BIT );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glEnable( GL_TEXTURE_2D );
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	initializeMatrices( surfaceSizeX, surfaceSizeY );

	setVSync( vsync && !gba_joybus_active );

#ifdef MMX
	if(!disableMMX)
		cpu_mmx = theApp.detectMMX();
	else
		cpu_mmx = 0;
#endif

	systemRedShift = 19;
	systemGreenShift = 11;
	systemBlueShift = 3;
	systemColorDepth = 32;
	fsColorDepth = 32;

	Init_2xSaI(32);

	utilUpdateSystemColorMaps(theApp.cartridgeType == IMAGE_GBA && gbColorOption == 1);
	theApp.updateFilter();
	theApp.updateIFB();
	pitch = filterWidth * (systemColorDepth>>3) + 4;
	data = pix + ( sizeX + 1 ) * 4;

	if(failed)
		return false;

	return true;
}

//clear colour buffer
void OpenGLDisplay::clear()
{
	glClearColor(0.0,0.0,0.0,1.0);
	glClear( GL_COLOR_BUFFER_BIT );
}

//main render func
void OpenGLDisplay::render()
{
	clear();

	pitch = filterWidth * (systemColorDepth>>3) + 4;
	data = pix + ( sizeX + 1 ) * 4;

	// apply pixel filter
	if(theApp.filterFunction) {
		data = filterData;
		theApp.filterFunction(
			pix + pitch,
			pitch,
			(u8*)theApp.delta,
			(u8*)filterData,
			width * 4 ,
			filterWidth,
			filterHeight);
	}

	// Texturemap complete texture to surface
	// so we have free scaling and antialiasing

	if( theApp.filterFunction ) {
		glPixelStorei( GL_UNPACK_ROW_LENGTH, width);
	} else {
		glPixelStorei( GL_UNPACK_ROW_LENGTH, sizeX + 1 );
	}
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_BGRA,GL_UNSIGNED_BYTE,data );


	glBegin( GL_QUADS );

	glTexCoord2f( 0.0f, 0.0f );
	glVertex3i( 0, 0, 0 );

	glTexCoord2f( (float)(width) / size, 0.0f );
	glVertex3i( surfaceSizeX, 0, 0 );

	glTexCoord2f( (float)(width) / size, (float)(height) / size );
	glVertex3i( surfaceSizeX, surfaceSizeY, 0 );

	glTexCoord2f( 0.0f, (float)(height) / size );
	glVertex3i( 0, surfaceSizeY, 0 );
	glEnd();


	if( showSpeed ) { // && ( videoOption > VIDEO_6X ) ) {
		char buffer[30];
		if( showSpeed == 1 ) {
			sprintf( buffer, "%3d%%", systemSpeed );
		} else {
			sprintf( buffer, "%3d%%(%d, %d fps)", systemSpeed, systemFrameSkip, showRenderedFrames );
		}
		glFontBegin(&font);
		glPushMatrix();
		float fontscale = (float)surfaceSizeX / 100.0f;
		glScalef(fontscale, fontscale, fontscale);
		glColor4f(1.0f, 0.25f, 0.25f, 1.0f);
		glFontTextOut(buffer, (surfaceSizeX-(strlen(buffer)*11))/(fontscale*2), (surfaceSizeY-20)/fontscale, 0);
		glPopMatrix();
		glFontEnd();
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBindTexture( GL_TEXTURE_2D, texture );
	}
	if( screenMessage ) {
		if( ( ( GetTickCount() - theApp.screenMessageTime ) < 3000 ) && !disableStatusMessages ) {
			glFontBegin(&font);
			glPushMatrix();

			float fontscale = (float)surfaceSizeX / 100.0f;
			glScalef(fontscale, fontscale, fontscale);
			glColor4f(1.0f, 0.25f, 0.25f, 1.0f);
			glFontTextOut((char *)((const char *)theApp.screenMessageBuffer), (surfaceSizeX-(theApp.screenMessageBuffer.GetLength()*11))/(fontscale*2), (surfaceSizeY-40)/fontscale, 0);
			glPopMatrix();
			glFontEnd();
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glBindTexture( GL_TEXTURE_2D, texture );
		} else {
			screenMessage = false;
		}
	}

	glFlush();
	SwapBuffers( hDC );
	// since OpenGL draws on the back buffer,
	// we have to swap it to the front buffer to see the content

}

//resize screen
void OpenGLDisplay::resize( int w, int h )
{
	initializeMatrices( w, h );	
}

//update filtering methods
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

//init projection matrixes and viewports
void OpenGLDisplay::initializeMatrices( int w, int h )
{
	if( fullScreenStretch ) {
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

//init font texture
bool OpenGLDisplay::initializeTexture( int w, int h )
{
	// size = 2^n
	// w = 24  > size = 256 = 2^8
	// w = 255 > size = 256 = 2^8
	// w = 256 > size = 512 = 2^9
	// w = 300 > size = 512 = 2^9
	// OpenGL textures have to be square and a power of 2
	// We could use methods that allow tex's to not be powers of two
	// but that requires extra OGL extensions

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
	updateFiltering( glFilter );

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

	//return ( glGetError() == GL_NO_ERROR) ? true : false;
	// Workaround: We usually get GL_INVALID_VALUE, but somehow it works nevertheless
	// In consequence, we must not treat it as an error or else the app behaves as if an error occured.
	// This in the end results in theApp->input not being created = no input when switching from D3D to OGL
	return true;
}

//turn vsync on or off
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

//change render size for fonts and filter data
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
		if (filterData)
			free(filterData);
		filterData = (u8 *)malloc(4*w*h);
	}
	
	return true;
}

//calculate RECTs
void OpenGLDisplay::calculateDestRect( int w, int h )
{
	float scaleX = (float)w / (float)width;
	float scaleY = (float)h / (float)height;
	float min = (scaleX < scaleY) ? scaleX : scaleY;
	if( maxScale && (min > maxScale) ) {
		min = (float)maxScale;
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

//config options
void OpenGLDisplay::setOption( const char *option, int value )
{
	if( !_tcscmp( option, _T("vsync") ) ) {
		setVSync( value );
	}

	if( !_tcscmp( option, _T("glFilter") ) ) {
		updateFiltering( value );
	}

	if( !_tcscmp( option, _T("maxScale") ) ) {
		initializeMatrices( theApp.dest.right-theApp.dest.left, theApp.dest.bottom-theApp.dest.top );
	}

	if( !_tcscmp( option, _T("fullScreenStretch") ) ) {
		initializeMatrices( theApp.dest.right-theApp.dest.left, theApp.dest.bottom-theApp.dest.top );
	}
}

//set fullscreen mode
bool OpenGLDisplay::selectFullScreenMode( VIDEO_MODE &mode )
{
	FullscreenSettings dlg;
	dlg.setAPI( this->getType() );
	INT_PTR ret = dlg.DoModal();
	if( ret == IDOK ) {
		mode.adapter = dlg.m_device;
		switch( dlg.m_colorDepth )
		{
		case 30:
			// TODO: support
			return false;
			break;
		case 24:
			mode.bitDepth = 32;
			break;
		case 16:
		case 15:
			mode.bitDepth = 16;
			break;
		}
		mode.width = dlg.m_width;
		mode.height = dlg.m_height;
		mode.frequency = dlg.m_refreshRate;
		return true;
	} else {
		return false;
	}
}


IDisplay *newOpenGLDisplay()
{
	return new OpenGLDisplay();
}

#endif // #ifndef NO_OGL
