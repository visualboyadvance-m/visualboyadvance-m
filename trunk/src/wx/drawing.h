#ifndef GAME_DRAWING_H
#define GAME_DRAWING_H

class BasicDrawingPanel : public DrawingPanel, public wxPanel
{
public:
	BasicDrawingPanel(wxWindow* parent, int _width, int _height);
	wxWindow* GetWindow() { return this; }
	void Delete() { Destroy(); }

protected:
	void PaintEv2(wxPaintEvent &ev) { PaintEv(ev); }
	void DrawArea(wxWindowDC &dc);

	DECLARE_CLASS()
	DECLARE_EVENT_TABLE()
};

#ifndef NO_OGL
#include <wx/glcanvas.h>

class GLDrawingPanel : public DrawingPanel, public wxGLCanvas
{
public:
	GLDrawingPanel(wxWindow* parent, int _width, int _height);
	virtual ~GLDrawingPanel();
	wxWindow* GetWindow() { return this; }
	void Delete() { Destroy(); }

protected:
	void PaintEv2(wxPaintEvent &ev) { PaintEv(ev); }
	void OnSize(wxSizeEvent &);
	void DrawArea(wxWindowDC &dc);
#if wxCHECK_VERSION(2,9,0) || !defined(__WXMAC__)
	wxGLContext ctx;
#endif
	bool did_init;
	void Init();
	GLuint texid, vlist;
	int texsize;

	DECLARE_CLASS()
	DECLARE_EVENT_TABLE()
};
#endif

#if defined(__WXMSW__) && !defined(NO_D3D)
class DXDrawingPanel : public DrawingPanel, public wxPanel
{
public:
	DXDrawingPanel(wxWindow* parent, int _width, int _height);
	wxWindow* GetWindow() { return this; }
	void Delete() { Destroy(); }

protected:
	void PaintEv2(wxPaintEvent &ev) { PaintEv(ev); }
	void DrawArea(wxWindowDC &);
	bool did_init;
	void Init();

	DECLARE_CLASS()
	DECLARE_EVENT_TABLE()
};
#endif

#ifndef NO_CAIRO
#include <cairo.h>

class CairoDrawingPanel : public DrawingPanel, public wxPanel
{
public:
	CairoDrawingPanel(wxWindow* parent, int _width, int _height);
	~CairoDrawingPanel();
	wxWindow* GetWindow() { return this; }
	void Delete() { Destroy(); }

protected:
	void PaintEv2(wxPaintEvent &ev) { PaintEv(ev); }
	void DrawArea(wxWindowDC &);
	cairo_surface_t* conv_surf;

	DECLARE_CLASS()
	DECLARE_EVENT_TABLE()
};
#endif

#endif /* GAME_DRAWING_H */
