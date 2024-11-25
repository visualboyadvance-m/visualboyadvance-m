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

#if defined(__WXMSW__) && !defined(NO_D3D)
class DXDrawingPanel : public DrawingPanel {
public:
    DXDrawingPanel(wxWindow* parent, int _width, int _height);

protected:
    void DrawArea(wxWindowDC&);
};
#endif

#if defined(__WXMAC__)
class Quartz2DDrawingPanel : public BasicDrawingPanel {
public:
    Quartz2DDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual void DrawImage(wxWindowDC& dc, wxImage* im);
};

#endif // VBAM_WX_DRAWING_H_
