#ifndef VBAM_WX_DRAWING_H_
#define VBAM_WX_DRAWING_H_

#include "wx/wxvbam.h"

class BasicDrawingPanel : public DrawingPanel {
public:
    BasicDrawingPanel(wxWindow* parent, int _width, int _height);

protected:
    void DrawArea(wxWindowDC& dc);
    virtual void DrawImage(wxWindowDC& dc, wxImage* im);
};

// wx <= 2.8 may not be compiled with opengl support
#if !wxCHECK_VERSION(2, 9, 0) && !wxUSE_GLCANVAS
    #define NO_OGL
#endif

#ifndef NO_OGL
#include <wx/glcanvas.h>

// shuffled parms for 2.9 indicates non-auto glcontext
// before 2.9, wxMAC does not have this (but wxGTK & wxMSW do)
#if !wxCHECK_VERSION(2, 9, 0) && defined(__WXMAC__)
    #define wxglc(a, b, c, d, e, f) wxGLCanvas(a, b, d, e, f, wxEmptyString, c)
    #define wxGL_IMPLICIT_CONTEXT
#else
    #define wxglc wxGLCanvas
#endif

class GLDrawingPanel : public DrawingPanelBase, public wxGLCanvas {
public:
    GLDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~GLDrawingPanel();

protected:
    void DrawArea(wxWindowDC& dc);
    void OnSize(wxSizeEvent& ev);
    void AdjustViewport();
    void RefreshGL();
#ifndef wxGL_IMPLICIT_CONTEXT
    wxGLContext* ctx = nullptr;
#endif
    bool SetContext();
    void DrawingPanelInit();
    GLuint texid, vlist;
    int texsize;
};
#endif

#include <wx/string.h>

class SDLDrawingPanel : public DrawingPanel {
public:
    SDLDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~SDLDrawingPanel();

protected:
    void DrawArea();
    void DrawArea(uint8_t** data);
    void DrawArea(wxWindowDC&) override;
    void PaintEv(wxPaintEvent& ev) override;
    void EraseBackground(wxEraseEvent& ev) override;
    void OnSize(wxSizeEvent& ev) override;
    void DrawingPanelInit() override;

private:
    SDL_Window *sdlwindow = NULL;
    SDL_Texture *texture = NULL;
    SDL_Renderer *renderer = NULL;
	wxString renderername = wxEmptyString;
};

#if defined(__WXMSW__) && !defined(NO_D3D)
class DXDrawingPanel : public DrawingPanel {
public:
    DXDrawingPanel(wxWindow* parent, int _width, int _height);

protected:
    void DrawArea(wxWindowDC&);
};
#endif

#if defined(__WXMAC__)
#ifndef NO_METAL
#ifdef __OBJC__
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <CoreGraphics/CoreGraphics.h>
#endif

#include <simd/simd.h>

typedef enum AAPLVertexInputIndex
{
    AAPLVertexInputIndexVertices     = 0,
    AAPLVertexInputIndexViewportSize = 1,
} AAPLVertexInputIndex;

// Texture index values shared between shader and C code to ensure Metal shader buffer inputs match
//   Metal API texture set calls
typedef enum AAPLTextureIndex
{
    AAPLTextureIndexBaseColor = 0,
} AAPLTextureIndex;

//  This structure defines the layout of each vertex in the array of vertices set as an input to the
//    Metal vertex shader.  Since this header is shared between the .metal shader and C code,
//    you can be sure that the layout of the vertex array in the code matches the layout that
//    the vertex shader expects

typedef struct
{
    // Positions in pixel space. A value of 100 indicates 100 pixels from the origin/center.
    vector_float2 position;

    // 2D texture coordinate
    vector_float2 textureCoordinate;
} AAPLVertex;

bool is_macosx_1012_or_newer();

class MetalDrawingPanel : public DrawingPanel {
public:
    MetalDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~MetalDrawingPanel();

protected:
    void DrawArea();
    void DrawArea(uint8_t** data);
    void DrawArea(wxWindowDC&) override;
    void PaintEv(wxPaintEvent& ev) override;
    void EraseBackground(wxEraseEvent& ev) override;
    void OnSize(wxSizeEvent& ev) override;
    void DrawingPanelInit() override;

#ifdef __OBJC__
private:
    void CreateMetalView();
    id<MTLTexture> loadTextureUsingData(void *data);

    NSView *view;
    MTKView *metalView;
    NSRect metalFrame;
    MTLRenderPassDescriptor *renderPassDescriptor;
    id<MTLRenderCommandEncoder> renderEncoder;
    id<MTLCommandBuffer> commandBuffer;
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLRenderPipelineState> _pipelineState;
    id<MTLTexture> _texture;
    id<MTLBuffer> _vertices;
    NSUInteger _numVertices;
    vector_uint2 _viewportSize;
    vector_uint2 _contentSize;
#endif
};
#endif

class Quartz2DDrawingPanel : public BasicDrawingPanel {
public:
    Quartz2DDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual void DrawImage(wxWindowDC& dc, wxImage* im);
};
#endif

#endif // VBAM_WX_DRAWING_H_
