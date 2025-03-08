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

#ifndef NO_D3D
#include <d3d9.h>
#include <d3dx9.h>
#include <wrl/client.h>

class D3DDrawingPanel : public DrawingPanel {
    public:
        D3DDrawingPanel(wxWindow* parent, int width, int height);
        virtual ~D3DDrawingPanel();
    
    protected:
        // Drawing methods
        void DrawArea(wxWindowDC& dc) override;
        void DrawArea(uint8_t** data);
        void DrawOSD(wxWindowDC& dc) override;
        void DrawOSD();
        
        // Event handlers
        void OnSize(wxSizeEvent& ev) override;
        void PaintEv(wxPaintEvent& ev) override;
        void EraseBackground(wxEraseEvent& ev) override;
        
        // D3D management
        void DrawingPanelInit() override;
        void UpdateTexture();
        void AdjustViewport();
        void Render();
    
    private:
        // Direct3D resources
        IDirect3D9* d3d_;
        IDirect3DDevice9* device_;
        IDirect3DTexture9* texture_;
        IDirect3DVertexBuffer9* vertex_buffer_;
        ID3DXFont* font_;
        D3DPRESENT_PARAMETERS d3dpp_;
    
        DECLARE_EVENT_TABLE()
    };
    #endif

#if defined(__WXMAC__)
class Quartz2DDrawingPanel : public BasicDrawingPanel {
public:
    Quartz2DDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual void DrawImage(wxWindowDC& dc, wxImage* im);
};
#endif

#endif // VBAM_WX_DRAWING_H_
