#ifndef _WX_KEYTEXT_H
#define _WX_KEYTEXT_H

// wxKeyTextCtrl: a wxTextCtrl which stores/acts on key presses
// The value is the symbolic name of the key pressed
// Supports manual clearing (bs), multiple keys in widget, automatic tab on key

#include <vector>
#include <wx/accel.h>
#include <wx/textctrl.h>

typedef std::vector<wxAcceleratorEntry> wxAcceleratorEntry_v;

class wxKeyTextCtrl : public wxTextCtrl {
public:
    // default constructor; required for use with xrc
    // FIXME: clearable and keyenter should be style flags
    wxKeyTextCtrl()
        : wxTextCtrl()
        , clearable(true)
        , multikey(wxT(','))
        , keyenter(true)
        , lastmod(0)
        , lastkey(0){};
    virtual ~wxKeyTextCtrl(){};

    void SetClearable(bool set = true)
    {
        clearable = set;
    }
    void SetMultikey(wxChar c = wxT(','))
    {
        multikey = c;
    }
    void SetKeyEnter(bool set = true)
    {
        keyenter = set;
    }

    bool GetClearable()
    {
        return clearable;
    }
    wxChar GetMultikey()
    {
        return multikey;
    }
    bool GetKeyEnter()
    {
        return keyenter;
    }

    // convert mod+key to accel string, separated by -
    static wxString ToString(int mod, int key);
    // convert multiple keys, separated by multikey
    static wxString ToString(wxAcceleratorEntry_v keys, wxChar sep = wxT(','));
    // parses single key string into mod+key
    static bool FromString(const wxString& s, int& mod, int& key);
    // parse multi-key string into accelentry array
    // note that meta flag may be set in accelentry array item even
    // where not supported for accelerators (i.e. non-mac)
    // returns empty array on parse errors
    static wxAcceleratorEntry_v FromString(const wxString& s, wxChar sep = wxT(','));
    // parse a single key in given wxChar array up to given len
    static bool ParseString(const wxString& s, int len, int& mod, int& key);

protected:
    void OnKeyDown(wxKeyEvent&);
    void OnKeyUp(wxKeyEvent&);

    bool clearable;
    wxChar multikey;
    bool keyenter;
    // the last keydown event received; this is processed on next keyup
    int lastmod, lastkey;

    DECLARE_DYNAMIC_CLASS(wxKeyTextCtrl);
    DECLARE_EVENT_TABLE();
};

// A simple copy-only validator
class wxKeyValidator : public wxValidator {
public:
    wxKeyValidator(wxAcceleratorEntry_v* v)
        : wxValidator()
        , val(v)
    {
    }
    wxKeyValidator(const wxKeyValidator& v)
        : wxValidator()
        , val(v.val)
    {
    }
    wxObject* Clone() const
    {
        return new wxKeyValidator(val);
    }
    bool TransferToWindow();
    bool TransferFromWindow();
    bool Validate(wxWindow* p)
    {
        return true;
    }

protected:
    wxAcceleratorEntry_v* val;

    DECLARE_CLASS(wxKeyValidator)
};

const struct {
    wxKeyCode code;
    wxString name;
    wxString display_name;
} keys_with_display_names[] = {
    { WXK_BACK,              wxTRANSLATE("Back"),           wxTRANSLATE("Backspace") },
    { WXK_PAGEUP,            wxTRANSLATE("PageUp"),         wxTRANSLATE("Page Up") },
    { WXK_PAGEDOWN,          wxTRANSLATE("PageDown"),       wxTRANSLATE("Page Down") },
    { WXK_NUMLOCK,           wxTRANSLATE("Num_lock"),       wxTRANSLATE("Num Lock") },
    { WXK_SCROLL,            wxTRANSLATE("Scroll_lock"),    wxTRANSLATE("Scroll Lock") },
    { WXK_NUMPAD_SPACE,      wxTRANSLATE("KP_Space"),       wxTRANSLATE("Num Space") },
    { WXK_NUMPAD_TAB,        wxTRANSLATE("KP_Tab"),         wxTRANSLATE("Num Tab") },
    { WXK_NUMPAD_ENTER,      wxTRANSLATE("KP_Enter"),       wxTRANSLATE("Num Enter") },
    { WXK_NUMPAD_HOME,       wxTRANSLATE("KP_Home"),        wxTRANSLATE("Num Home") },
    { WXK_NUMPAD_LEFT,       wxTRANSLATE("KP_Left"),        wxTRANSLATE("Num left") },
    { WXK_NUMPAD_UP,         wxTRANSLATE("KP_Up"),          wxTRANSLATE("Num Up") },
    { WXK_NUMPAD_RIGHT,      wxTRANSLATE("KP_Right"),       wxTRANSLATE("Num Right") },
    { WXK_NUMPAD_DOWN,       wxTRANSLATE("KP_Down"),        wxTRANSLATE("Num Down") },

    // these two are in some 3.1+ builds for whatever reason
    { WXK_NUMPAD_PAGEUP,     wxTRANSLATE("KP_PageUp"),      wxTRANSLATE("Num PageUp") },
    { WXK_NUMPAD_PAGEDOWN,   wxTRANSLATE("KP_PageDown"),    wxTRANSLATE("Num PageDown") },

    { WXK_NUMPAD_PAGEUP,     wxTRANSLATE("KP_PageUp"),      wxTRANSLATE("Num Page Up") },
    { WXK_NUMPAD_PAGEDOWN,   wxTRANSLATE("KP_PageDown"),    wxTRANSLATE("Num Page Down") },
    { WXK_NUMPAD_END,        wxTRANSLATE("KP_End"),         wxTRANSLATE("Num End") },
    { WXK_NUMPAD_BEGIN,      wxTRANSLATE("KP_Begin"),       wxTRANSLATE("Num Begin") },
    { WXK_NUMPAD_INSERT,     wxTRANSLATE("KP_Insert"),      wxTRANSLATE("Num Insert") },
    { WXK_NUMPAD_DELETE,     wxTRANSLATE("KP_Delete"),      wxTRANSLATE("Num Delete") },
    { WXK_NUMPAD_EQUAL,      wxTRANSLATE("KP_Equal"),       wxTRANSLATE("Num =") },
    { WXK_NUMPAD_MULTIPLY,   wxTRANSLATE("KP_Multiply"),    wxTRANSLATE("Num *") },
    { WXK_NUMPAD_ADD,        wxTRANSLATE("KP_Add"),         wxTRANSLATE("Num +") },
    { WXK_NUMPAD_SEPARATOR,  wxTRANSLATE("KP_Separator"),   wxTRANSLATE("Num ,") },
    { WXK_NUMPAD_SUBTRACT,   wxTRANSLATE("KP_Subtract"),    wxTRANSLATE("Num -") },
    { WXK_NUMPAD_DECIMAL,    wxTRANSLATE("KP_Decimal"),     wxTRANSLATE("Num .") },
    { WXK_NUMPAD_DIVIDE,     wxTRANSLATE("KP_Divide"),      wxTRANSLATE("Num /") },
};

// 2.x does not have the WXK_RAW_XXX values for Mac
#if wxMAJOR_VERSION < 3
    #define WXK_RAW_CONTROL   WXK_COMMAND
    #define wxMOD_RAW_CONTROL wxMOD_CMD
#endif

#endif /* WX_KEYTEXT_H */
