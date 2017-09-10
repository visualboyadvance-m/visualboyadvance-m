#ifndef _WX_JOYKEYTEXT_H
#define _WX_JOYKEYTEXT_H

// wxJoyKeyTextCtrl: a wxTextCtrl which stores/acts on key presses and joystick
// The value is the symbolic name of the key pressed
// Supports manual clearing (bs), multiple keys in widget, automatic tab on key

#include "wx/keyedit.h"
#include "wx/sdljoy.h"

typedef struct wxJoyKeyBinding {
    int key; // key code; listed first for easy static init
    int mod; // modifier flags
    int joy; // joystick # (starting at 1)
    // if joy is non-0, key = control number, and mod = control type
} wxJoyKeyBinding;

typedef std::vector<wxJoyKeyBinding> wxJoyKeyBinding_v;

// joystick control types
// mod for joysticks
enum { WXJB_AXIS_PLUS,
    WXJB_AXIS_MINUS,
    WXJB_BUTTON,
    WXJB_HAT_FIRST,
    WXJB_HAT_N = WXJB_HAT_FIRST,
    WXJB_HAT_S,
    WXJB_HAT_W,
    WXJB_HAT_E,
    WXJB_HAT_NW,
    WXJB_HAT_NE,
    WXJB_HAT_SW,
    WXJB_HAT_SE,
    WXJB_HAT_LAST = WXJB_HAT_SE };

class wxJoyKeyTextCtrl : public wxKeyTextCtrl {
public:
    // default constructor; required for use with xrc
    // FIXME: clearable and keyenter should be style flags
    wxJoyKeyTextCtrl()
        : wxKeyTextCtrl()
    {
    }
    virtual ~wxJoyKeyTextCtrl(){};

    // key is event.GetControlIndex(), and joy is event.GetJoy() + 1
    // mod is derived from GetControlValue() and GetControlType():
    // convert wxSDLJoyEvent's type+val into mod (WXJB_*)
    static int DigitalButton(wxSDLJoyEvent& event);
    // convert mod+key to accel string, separated by -
    static wxString ToString(int mod, int key, int joy);
    // convert multiple keys, separated by multikey
    static wxString ToString(wxJoyKeyBinding_v keys, wxChar sep = wxT(','));
    // parses single key string into mod+key
    static bool FromString(const wxString& s, int& mod, int& key, int& joy);
    // parse multi-key string into array
    // returns empty array on parse errors
    static wxJoyKeyBinding_v FromString(const wxString& s, wxChar sep = wxT(','));
    // parse a single key in given wxChar array up to given len
    static bool ParseString(const wxString& s, int len, int& mod, int& key, int& joy);

protected:
    void OnJoy(wxSDLJoyEvent&);

    DECLARE_DYNAMIC_CLASS(wxJoyKeyTextCtrl);
    DECLARE_EVENT_TABLE();
};

// A simple copy-only validator
class wxJoyKeyValidator : public wxValidator {
public:
    wxJoyKeyValidator(wxJoyKeyBinding_v* v)
        : wxValidator()
        , val(v)
    {
    }
    wxJoyKeyValidator(const wxJoyKeyValidator& v)
        : wxValidator()
        , val(v.val)
    {
    }
    wxObject* Clone() const
    {
        return new wxJoyKeyValidator(val);
    }
    bool TransferToWindow();
    bool TransferFromWindow();
    bool Validate(wxWindow* p)
    {
        return true;
    }

protected:
    wxJoyKeyBinding_v* val;

    DECLARE_CLASS(wxJoyKeyValidator)
};

#endif /* WX_JOYKEYTEXT_H */
