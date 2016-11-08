#ifndef GAME_DRAWING_H
#define GAME_DRAWING_H

#include "wxvbam.h"

class BasicDrawingPanel : public DrawingPanel, public wxPanel {
public:
    BasicDrawingPanel(wxWindow* parent, int _width, int _height);

protected:
    void DrawArea(wxWindowDC& dc);

    DECLARE_CLASS()
};

#ifndef NO_OGL
#include <wx/glcanvas.h>

class GLDrawingPanel : public DrawingPanel, public wxGLCanvas {
public:
    GLDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~GLDrawingPanel();

protected:
    void DrawArea(wxWindowDC& dc);
    void OnSize(wxSizeEvent& ev);
    void AdjustViewport();
#if wxCHECK_VERSION(2, 9, 0)
    wxGLContext* ctx;
#endif
    void DrawingPanelInit();
    GLuint texid, vlist;
    int texsize;

    DECLARE_CLASS()
};
#endif

#if defined(__WXMSW__) && !defined(NO_D3D)
class DXDrawingPanel : public DrawingPanel, public wxPanel {
public:
    DXDrawingPanel(wxWindow* parent, int _width, int _height);

protected:
    void DrawArea(wxWindowDC&);
    void DrawingPanelInit();

    DECLARE_CLASS()
};
#endif

#ifndef NO_CAIRO
#include <cairo.h>

class CairoDrawingPanel : public DrawingPanel, public wxPanel {
public:
    CairoDrawingPanel(wxWindow* parent, int _width, int _height);
    ~CairoDrawingPanel();

protected:
    void DrawArea(wxWindowDC&);
    cairo_surface_t* conv_surf;

    DECLARE_CLASS()
};
#endif

#endif /* GAME_DRAWING_H */
