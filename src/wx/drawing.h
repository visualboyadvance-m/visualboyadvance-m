#ifndef GAME_DRAWING_H
#define GAME_DRAWING_H

#include "wxvbam.h"

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
    wxGLContext* ctx;
#endif
    bool SetContext();
    void DrawingPanelInit();
    GLuint texid, vlist;
    int texsize;
};
#endif

#if defined(__WXMSW__) && !defined(NO_D3D)
#include <d3d9.h> // main include file

struct Vertex
{
    float x, y, z;
    float u, v;
};

class DXDrawingPanel : public DrawingPanel
{
public:
    DXDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~DXDrawingPanel();

protected:
    void DrawingPanelInit();
    void DrawArea(wxWindowDC&);
    void OnSize(wxSizeEvent& ev);

private:
    IDirect3D9Ex* d3d;
    IDirect3DDevice9Ex* d3ddev;
    IDirect3DVertexBuffer9* vbuffer;
    IDirect3DTexture9* texture;
};
#endif

#if defined(__WXMAC__)
class Quartz2DDrawingPanel : public BasicDrawingPanel {
public:
    Quartz2DDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual void DrawImage(wxWindowDC& dc, wxImage* im);
};
#endif

#endif /* GAME_DRAWING_H */
