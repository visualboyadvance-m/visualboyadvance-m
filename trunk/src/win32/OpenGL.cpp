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
#include "..\memgzio.h"

#include <cmath>
#include "glFont.h"

// OpenGL
#include <gl/GL.h> // main include file
#include <GL/glu.h>
#ifdef HAS_GLEXT
#include <gl/glext.h>
#endif

typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALFARPROC)( int );
PFNGLCREATEPROGRAMOBJECTARBPROC  glCreateProgramObjectARB  = NULL;
PFNGLDELETEOBJECTARBPROC         glDeleteObjectARB         = NULL;
PFNGLUSEPROGRAMOBJECTARBPROC     glUseProgramObjectARB     = NULL;
PFNGLCREATESHADEROBJECTARBPROC   glCreateShaderObjectARB   = NULL;
PFNGLSHADERSOURCEARBPROC         glShaderSourceARB         = NULL;
PFNGLCOMPILESHADERARBPROC        glCompileShaderARB        = NULL;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB = NULL;
PFNGLATTACHOBJECTARBPROC         glAttachObjectARB         = NULL;
PFNGLGETINFOLOGARBPROC           glGetInfoLogARB           = NULL;
PFNGLLINKPROGRAMARBPROC          glLinkProgramARB          = NULL;
PFNGLGETUNIFORMLOCATIONARBPROC   glGetUniformLocationARB   = NULL;
PFNGLUNIFORM4FARBPROC            glUniform4fARB            = NULL;
PFNGLUNIFORM1IARBPROC            glUniform1iARB            = NULL;




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
	int width;
	int height;
	float size;
	u8 *filterData;
	RECT destRect;
	bool failed;
	GLFONT font;
	int VertexShader,FragmentShader,textureLocation,ShaderProgram,g_location_grayScaleWeights;
	char *VertexShaderSource,*FragmentShaderSource;

	void initializeMatrices( int w, int h );
	bool initializeTexture( int w, int h );
	void updateFiltering( int value );
	void setVSync( int interval = 1 );
	void calculateDestRect( int w, int h );
	void initializeFont();
	void InitGLSLShader();
	void DeInitGLSLShader();
	void SetGLSLShaderConstants();

public:
	OpenGLDisplay();
	virtual ~OpenGLDisplay();
	virtual DISPLAY_TYPE getType() { return OPENGL; };

	virtual void EnableOpenGL();
	virtual void DisableOpenGL();
	virtual bool initialize();
	virtual void cleanup();
	virtual void render();
	virtual void clear();
	virtual bool changeRenderSize( int w, int h );
	virtual void resize( int w, int h );
	virtual void setOption( const char *, int );
	virtual int  selectFullScreenMode( GUID ** );
};



#include "gzglfont.h"
char *readShaderFile(char *FileName);
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

char *readShaderFile(char *FileName) {
	FILE *fp;
	char *DATA = NULL;

	size_t flength = 0;

	fp = fopen(FileName,"rt");

    fseek(fp, 0, SEEK_END);
    flength = ftell(fp);
    rewind(fp);

	DATA = (char *)malloc(sizeof(char) * (flength+1));
	flength = fread(DATA, sizeof(char), flength, fp);
	DATA[flength] = '\0';
		
	fclose(fp);	

	return DATA;
}

void OpenGLDisplay::InitGLSLShader (void) {
	glCreateProgramObjectARB  = (PFNGLCREATEPROGRAMOBJECTARBPROC)wglGetProcAddress("glCreateProgramObjectARB");
    glDeleteObjectARB         = (PFNGLDELETEOBJECTARBPROC)wglGetProcAddress("glDeleteObjectARB");
    glUseProgramObjectARB     = (PFNGLUSEPROGRAMOBJECTARBPROC)wglGetProcAddress("glUseProgramObjectARB");
    glCreateShaderObjectARB   = (PFNGLCREATESHADEROBJECTARBPROC)wglGetProcAddress("glCreateShaderObjectARB");
    glShaderSourceARB         = (PFNGLSHADERSOURCEARBPROC)wglGetProcAddress("glShaderSourceARB");
    glCompileShaderARB        = (PFNGLCOMPILESHADERARBPROC)wglGetProcAddress("glCompileShaderARB");
    glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC)wglGetProcAddress("glGetObjectParameterivARB");
    glAttachObjectARB         = (PFNGLATTACHOBJECTARBPROC)wglGetProcAddress("glAttachObjectARB");
    glGetInfoLogARB           = (PFNGLGETINFOLOGARBPROC)wglGetProcAddress("glGetInfoLogARB");
    glLinkProgramARB          = (PFNGLLINKPROGRAMARBPROC)wglGetProcAddress("glLinkProgramARB");
    glGetUniformLocationARB   = (PFNGLGETUNIFORMLOCATIONARBPROC)wglGetProcAddress("glGetUniformLocationARB");
    glUniform4fARB            = (PFNGLUNIFORM4FARBPROC)wglGetProcAddress("glUniform4fARB");
	glUniform1iARB            = (PFNGLUNIFORM1IARBPROC)wglGetProcAddress("glUniform1iARB");

    VertexShader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	FragmentShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

	VertexShaderSource = readShaderFile("texture.vert");
	FragmentShaderSource = readShaderFile("texture.frag");

	const char * VS = VertexShaderSource;
	const char * FS = FragmentShaderSource;

	glShaderSourceARB(VertexShader, 1, &VS,NULL);
	glShaderSourceARB(FragmentShader, 1, &FS,NULL);
		
	free(VertexShaderSource);free(FragmentShaderSource);
		
	glCompileShaderARB(VertexShader);
	glCompileShaderARB(FragmentShader);
		
	ShaderProgram = glCreateProgramObjectARB();
			
	glAttachObjectARB(ShaderProgram,VertexShader);
	glAttachObjectARB(ShaderProgram,FragmentShader);

	glLinkProgramARB(ShaderProgram);	 
}

void OpenGLDisplay::SetGLSLShaderConstants()
{
	//get shader uniforms and shader weights
	textureLocation = glGetUniformLocationARB( ShaderProgram, "testTexture" );
	g_location_grayScaleWeights = glGetUniformLocationARB( ShaderProgram, "grayScaleWeights" );
	glUniform1iARB( textureLocation, 1 );

    // Load the grey scale weights for the luminance filter.
    float fGrayScaleWeights[] = { 0.30f, 0.59f, 0.11f, 0.0f };
    glUniform4fARB( g_location_grayScaleWeights, fGrayScaleWeights[0], 
    fGrayScaleWeights[1], fGrayScaleWeights[2], fGrayScaleWeights[3] );

}
void OpenGLDisplay::DeInitGLSLShader (void) {
	glDeleteObjectARB(VertexShader);
	glDeleteObjectARB(FragmentShader);
	glDeleteObjectARB(ShaderProgram);
}

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
}


OpenGLDisplay::~OpenGLDisplay()
{
	DeInitGLSLShader();
	cleanup();
}


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

void OpenGLDisplay::DisableOpenGL()
{
	wglMakeCurrent( NULL, NULL );
	wglDeleteContext( hRC );
	ReleaseDC( theApp.m_pMainWnd->GetSafeHwnd(), hDC );
}

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
}


bool OpenGLDisplay::initialize()
{
	theApp.mode320Available = FALSE;
	theApp.mode640Available = FALSE;
	theApp.mode800Available = FALSE;
	theApp.mode1024Available = FALSE;
	theApp.mode1280Available = FALSE;

	EnableOpenGL();
	initializeFont();
	glPushAttrib( GL_ENABLE_BIT );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glEnable( GL_TEXTURE_2D );
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	
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

	return TRUE;
}


void OpenGLDisplay::clear()
{
	glClearColor(0.0,0.0,0.0,1.0);
	glClear( GL_COLOR_BUFFER_BIT );
}

void OpenGLDisplay::render()
{ 
	clear();
	if (theApp.GLSLShaders){
	InitGLSLShader();
    glUseProgramObjectARB( ShaderProgram );
	SetGLSLShaderConstants();
	}
	else{
	glUseProgramObjectARB(NULL);
	DeInitGLSLShader();
	}
	
	int pitch = theApp.filterWidth * (systemColorDepth>>3) + 4;
	u8 *data = pix + ( theApp.sizeX + 1 ) * 4;

	// apply pixel filter
	if(theApp.filterFunction) {
		data = filterData;
		theApp.filterFunction(
			pix + pitch,
			pitch,
			(u8*)theApp.delta,
			(u8*)filterData,
			width * 4 ,
			theApp.filterWidth,
			theApp.filterHeight);
	}

	// Texturemap complete texture to surface
	// so we have free scaling and antialiasing

	if( theApp.filterFunction ) {
		glPixelStorei( GL_UNPACK_ROW_LENGTH, width);
	} else {
		glPixelStorei( GL_UNPACK_ROW_LENGTH, theApp.sizeX + 1 );
	}
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,data );

	 switch(  theApp.glType )
     {
        case 0: 
		glBegin( GL_TRIANGLE_STRIP );
		glTexCoord2f( 0.0f, 0.0f );
		glVertex3i( 0, 0, 0 );

		glTexCoord2f( (float)(width) / size, 0.0f );
		glVertex3i( theApp.surfaceSizeX, 0, 0 );

		glTexCoord2f( 0.0f, (float)(height) / size );
		glVertex3i( 0, theApp.surfaceSizeY, 0 );

		glTexCoord2f( (float)(width) / size, (float)(height) / size );
		glVertex3i( theApp.surfaceSizeX, theApp.surfaceSizeY, 0 );

		glEnd();
        break;
		case 1:  
		glBegin( GL_QUADS );

		glTexCoord2f( 0.0f, 0.0f );
		glVertex3i( 0, 0, 0 );

		glTexCoord2f( (float)(width) / size, 0.0f );
		glVertex3i( theApp.surfaceSizeX, 0, 0 );

		glTexCoord2f( (float)(width) / size, (float)(height) / size );
		glVertex3i( theApp.surfaceSizeX, theApp.surfaceSizeY, 0 );

		glTexCoord2f( 0.0f, (float)(height) / size );
		glVertex3i( 0, theApp.surfaceSizeY, 0 );

		glEnd();
        break;
        case 2:     
		glBegin( GL_POLYGON );

		glTexCoord2f( 0.0f, 0.0f );
		glVertex3i( 0, 0, 0 );

		glTexCoord2f( (float)(width) / size, 0.0f );
		glVertex3i( theApp.surfaceSizeX, 0, 0 );

		glTexCoord2f( (float)(width) / size, (float)(height) / size );
		glVertex3i( theApp.surfaceSizeX, theApp.surfaceSizeY, 0 );

		glTexCoord2f( 0.0f, (float)(height) / size );
		glVertex3i( 0, theApp.surfaceSizeY, 0 );

		glEnd();
        break;
  }



	if( theApp.showSpeed ) { // && ( theApp.videoOption > VIDEO_4X ) ) {
		char buffer[30];
		if( theApp.showSpeed == 1 ) {
			sprintf( buffer, "%3d%%", systemSpeed );
		} else {
			sprintf( buffer, "%3d%%(%d, %d fps)", systemSpeed, systemFrameSkip, theApp.showRenderedFrames );
		}
		glFontBegin(&font);
		glPushMatrix();
		float fontscale = (float)theApp.surfaceSizeX / 100.0f;
		glScalef(fontscale, fontscale, fontscale);
		glColor4f(1.0f, 0.25f, 0.25f, 1.0f);
		glFontTextOut(buffer, (theApp.surfaceSizeX-(strlen(buffer)*11))/(fontscale*2), (theApp.surfaceSizeY-20)/fontscale, 0);
		glPopMatrix();
		glFontEnd();
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBindTexture( GL_TEXTURE_2D, texture );
	}
	if( theApp.screenMessage ) {
		if( ( ( GetTickCount() - theApp.screenMessageTime ) < 3000 ) && !theApp.disableStatusMessage ) {
			glFontBegin(&font);
			glPushMatrix();

			float fontscale = (float)theApp.surfaceSizeX / 100.0f;
			glScalef(fontscale, fontscale, fontscale);
			glColor4f(1.0f, 0.25f, 0.25f, 1.0f);
			glFontTextOut((char *)((const char *)theApp.screenMessageBuffer), (theApp.surfaceSizeX-(theApp.screenMessageBuffer.GetLength()*11))/(fontscale*2), (theApp.surfaceSizeY-40)/fontscale, 0);
			glPopMatrix();
			glFontEnd();
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glBindTexture( GL_TEXTURE_2D, texture );
		} else {
			theApp.screenMessage = false;
		}
	}
	
	glFlush();
	

	SwapBuffers( hDC );
	// since OpenGL draws on the back buffer,
	// we have to swap it to the front buffer to see it
    
	// draw informations with GDI on the front buffer
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
		if (filterData)
			free(filterData);
		filterData = (u8 *)malloc(4*w*h);

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
