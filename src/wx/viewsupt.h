#ifndef WX_VIEWSUPT_H
#define WX_VIEWSUPT_H

#include <wx/wx.h>
#include <wx/window.h>
#include <wx/image.h>
#include <wx/caret.h>
#include <wx/spinctrl.h>
#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/scrolbar.h>
#include <wx/stattext.h>
#include <wx/checkbox.h>

#include <stdint.h> // for uint32_t

// avoid exporting too much stuff
namespace Viewers {
// common to all viewers:
//   - track in MainFrame::popups
//   - wxID_CLOSE button closes window
//   - AutoUpdate checkbox toggles calling Update() every screen refresh
class Viewer : public wxDialog {
public:
    void CloseDlg(wxCloseEvent& ev);
    Viewer(const wxString& name);
    virtual ~Viewer()
    {
    }
    virtual void Update() = 0;
    bool auto_update;

    // A lot of viewers have GUI elements to set parameters.  Almost all
    // of them just read back the value and update the display.  This
    // event handler does that with validators.
    void ActiveCtrl(wxCommandEvent& ev);
    void ActiveCtrlScr(wxScrollEvent& ev)
    {
        ActiveCtrl(ev);
    }
    void ActiveCtrlSpin(wxSpinEvent& ev)
    {
        ActiveCtrl(ev);
    }

protected:
    wxString dname;
    void SetAutoUpdate(wxCommandEvent& ev)
    {
        auto_update = ev.IsChecked();
    }

    DECLARE_EVENT_TABLE()
};

// on errors, abort program
#define baddialog()                                                      \
    do {                                                                 \
        wxLogError(_("Unable to load dialog %s from resources"), dname.c_str()); \
        wxGetApp().frame->Close(true);                                   \
        return;                                                          \
    } while (0)

// widgets to use with auto validator
#define getvfld(sv, n, t, v)             \
    do {                                 \
        t* _w = sv XRCCTRL(*this, n, t); \
        if (!_w)                         \
            baddialog();                 \
        _w->SetValidator(v);             \
    } while (0)
#define getradio(sv, n, var, val) getvfld(sv, n, wxRadioButton, wxBoolIntValidator(&var, val))
#define getslider(sv, n, var) getvfld(sv, n, wxSlider, wxGenericValidator(&var))
#define getspin(sv, n, var) getvfld(sv, n, wxSpinCtrl, wxGenericValidator(&var))

#define LoadXRCViewer(t)                                    \
    do {                                                    \
        wxDialog* d = new Viewers::t##Viewer;               \
        if (d) {                                            \
            d->SetWindowStyle(wxCAPTION | wxRESIZE_BORDER); \
            d->Show();                                      \
        }                                                   \
    } while (0)

// a list box with no horizontal scrollbar and a funky vertical scrollbar:
// range = 1 - 500
//   but/pagesz = # of lines shown/# of lines shown - 1
// 1-100 = normal
// 101-200 = 10x semi-stationary
// 201-300 = stationary @ center
// 301-400 = 10x semi-stationary
// 401-500 = normal

// note that since listboxes' size is impossible to control correctly (at
// least with wxGTK), this uses a textctrl to display the items.

class DisList : public wxPanel {
public:
    DisList();
    // called after init to create subcontrols and size panel
    void Refit(int cols);
    void MoveSB();
    void MoveView(wxScrollEvent& ev);

private:
    void RefillNeeded();

public:
    // called by parent's refill handler or any other time strings have
    // changed
    void Refill();

private:
    void Resize(wxSizeEvent& ev);
    void SetSel();

public:
    // make addr visible and then select it
    void SetSel(uint32_t addr);
    void UnSel()
    {
        issel = false;
    }

    // currently visible lines
    int nlines;
    // at least nlines strings to display
    wxArrayString strings;
    // and their starting addrs (mostly for scrollbar)
    wxArrayInt addrs;
    // how far back to scroll for single line
    int back_size;
    // address of top line
    uint32_t topaddr;
    // max address for scrollbar
    uint32_t maxaddr;

protected:
    // assigned to textctrl to avoid mouse input
    void MouseEvent(wxMouseEvent& ev)
    {
	(void)ev; // unused param
    }
    // the subwidgets
    wxTextCtrl tc;
    wxScrollBar sb;
    // cached computed line tc size
    int lineheight, extraheight;
    // need to know if tc/sb have been Create()d yet
    bool didinit;
    // selection info
    uint32_t seladdr;
    bool issel;

    DECLARE_DYNAMIC_CLASS(DisList) // for xrc
    DECLARE_EVENT_TABLE()
};

BEGIN_DECLARE_EVENT_TYPES()
// event generated when fewer lines available than needed
DECLARE_LOCAL_EVENT_TYPE(EVT_REFILL_NEEDED, 0)
END_DECLARE_EVENT_TYPES()

// a hex editor with a funky scrollbar like above

// since it's impossible to exercise this much control over a text
// control, it uses a panel into which text is drawn.
// Maybe some day the above will be changed to do that as well, since
// it allows better mouse control as well.

class MemView : public wxPanel {
public:
    MemView();
    // called after init to create subcontrols and size panel
    void Refit();
    void MoveSB();
    void MoveView(wxScrollEvent& ev);

private:
    void RefillNeeded();

public:
    // called by parent's refill handler or any other time strings have
    // changed
    void Refill();

private:
    void Refill(wxDC& dc);
    void RepaintEv(wxPaintEvent& ev);
    void Repaint();
    void Resize(wxSizeEvent& ev);

public:
    // make addr visible
    void ShowAddr(uint32_t addr, bool force_update = false);

    // current selection, or topaddr if none
    uint32_t GetAddr();
    // currently visible lines
    int nlines;
    // at least nlines * 4 words to display
    wxArrayInt words;
    // address of top line
    uint32_t topaddr;
    // max address for scrollbar
    uint32_t maxaddr;
    // bytes per word == (1 << fmt)
    int fmt;
    // after write, these contain write addr and val
    uint32_t writeaddr, writeval;
    // when selection is made, this widget is updated w/ addr
    wxControl* addrlab;

protected:
    // easier than checking maxaddr
    int addrlen;

    void MouseEvent(wxMouseEvent& ev);
    void KeyEvent(wxKeyEvent& ev);
    // the subwidgets
    wxPanel disp;
    wxScrollBar sb;
    wxCaret* caret;
    // cached text size
    int charheight, charwidth;
    // need to know if tc/sb have been Create()d yet
    bool didinit;
    // selection info
    int selnib, seladdr;
    bool isasc;
    void ShowCaret();

    DECLARE_DYNAMIC_CLASS(MemView) // for xrc
    DECLARE_EVENT_TABLE()
};

BEGIN_DECLARE_EVENT_TYPES()
// event generated when write occurs
// check writeaddr/writeval/fmt
DECLARE_LOCAL_EVENT_TYPE(EVT_WRITEVAL, 0)
END_DECLARE_EVENT_TYPES()

// Display a color in a square, with the RGB value to its right.

// this is too hard to integrate into xrc (impossible to initialize
// correctly) so no accomodations are made for this

class ColorView : public wxControl {
public:
    ColorView(wxWindow* parent, wxWindowID id);
    void SetRGB(int r, int g, int b);
    void GetRGB(int& _r, int& _g, int& _b)
    {
        _r = r;
        _g = g;
        _b = b;
    }

protected:
    int r, g, b;
    wxPanel* cp;
    wxStaticText *rt, *gt, *bt;
};
#define unkctrl(n, v)                                                     \
    do {                                                                  \
        if (!wxXmlResource::Get()->AttachUnknownControl(wxT(n), v, this)) \
            baddialog();                                                  \
    } while (0)
#define colorctrl(v, n)                    \
    do {                                   \
        v = new ColorView(this, XRCID(n)); \
        if (!v)                            \
            baddialog();                   \
        unkctrl(n, v);                     \
    } while (0)

// Display a small bitmap in jumbopixel style.  If a pixel is selected, it
// is highlighted with a border.  For wxvbam, no event is generated.
// Instead, a ColorView can be assigned to it, and on selection, that
// widget will be updated to the selected color.
// The whole class can also be derived to add more functionality to the
// button click.

// It must be intialized in 3 phases: 2-phase xrc-style (new + Create()),
// and then InitBMP()

class PixView : public wxPanel {
public:
    PixView()
        : wxPanel()
        , bm(0)
    {
    }
    bool InitBMP(int w = 8, int h = 8, ColorView* cv = NULL);
    // stride is in pixels
    // format is rgb24 (aka wxImage format)
    // x/y is added to data and returned coords
    // if data == NULL, bitmap will be reset to default (all-black)
    virtual void SetData(const unsigned char* data, int stride, int x = 0, int y = 0);
    // desel if out of displayed range
    void SetSel(int x, int y, bool dsel_cview_update = true);
    // -1, -1 = no sel
    void GetSel(int& x, int& y)
    {
        x = selx < 0 ? -1 : ox + selx;
        y = sely < 0 ? -1 : oy + sely;
    }
    ColorView* cview;

protected:
    wxImage im;
    wxBitmap* bm;
    void Redraw(wxPaintEvent& ev);
    virtual void SelPoint(wxMouseEvent& ev);
    int ox, oy, selx, sely;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(PixView)
};
#define pixview(v, n, w, h, cv)         \
    do {                                \
        v = XRCCTRL(*this, n, PixView); \
        if (!v)                         \
            baddialog();                \
        v->InitBMP(w, h, cv);           \
    } while (0)

// a graphics viewer panel; expected to be inside of a wxScrollWindow
class GfxPanel : public wxPanel {
public:
    GfxPanel()
        : wxPanel()
        , bm(0)
        , selx(-1)
        , sely(-1)
    {
    }
    int bmw, bmh;
    wxBitmap* bm;
    wxImage* im;
    PixView* pv;

protected:
    void DrawBitmap(wxPaintEvent& ev);
    void Click(wxMouseEvent& ev);
    void MouseMove(wxMouseEvent& ev);
    void DoSel(wxMouseEvent& ev, bool force = false);

private:
    int selx, sely;

    DECLARE_DYNAMIC_CLASS(GfxPanel)
    DECLARE_EVENT_TABLE()
};

BEGIN_DECLARE_EVENT_TYPES()
// event generated on mouse click
// generates wxMouseEvent with coords adjusted to original bitmap
// size regardless of scaling
DECLARE_LOCAL_EVENT_TYPE(EVT_COMMAND_GFX_CLICK, 0)
END_DECLARE_EVENT_TYPES()
#define EVT_GFX_CLICK(id, fun)                       \
    DECLARE_EVENT_TABLE_ENTRY(EVT_COMMAND_GFX_CLICK, \
        id,                                          \
        wxID_ANY,                                    \
        wxMouseEventHandler(fun),                    \
        NULL)                                        \
    ,

// like Viewer, common stuff to all gfx viewers
// this is what actually manages the GfxPanel
class GfxViewer : public Viewer {
public:
    GfxViewer(const wxString& dname, int maxw, int maxh);
    void ChangeBMP();
    void BMPSize(int w, int h);

protected:
    void StretchTog(wxCommandEvent& ev);
    void RefreshEv(wxCommandEvent& ev);
    void SaveBMP(wxCommandEvent& ev);
    wxImage image;
    GfxPanel* gv;

private:
    static wxString bmp_save_dir;
    wxScrolledWindow* gvs;
    wxCheckBox* str;

    DECLARE_EVENT_TABLE()
};

// if the jumbopixel view is all there is, maybe send a GFX_CLICK..
class PixViewEvt : public PixView {
    // generates a GFX_CLICK if a point is selected
    void SetData(const unsigned char* data, int stride, int x = 0, int y = 0);

protected:
    // always generates a GFX_CLICK
    void SelPoint(wxMouseEvent& ev);
    void click();
    DECLARE_DYNAMIC_CLASS(PixViewEvt)
};

// a display-only checkbox which does not look like it's disabled
class DispCheckBox : public wxCheckBox {
public:
    bool AcceptsFocus() const
    {
        return false;
    }
    void MouseEvent(wxMouseEvent& ev)
    {
	(void)ev; // unused param
    }
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(DispCheckBox)
};

// standard widgets in graphical viewers
}
#endif /* WX_VIEWSUPT_H */
