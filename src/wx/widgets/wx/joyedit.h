#ifndef _WX_JOYKEYTEXT_H
#define _WX_JOYKEYTEXT_H

// wxJoyKeyTextCtrl: a wxTextCtrl which stores/acts on key presses and joystick
// The value is the symbolic name of the key pressed
// Supports manual clearing (bs), multiple keys in widget, automatic tab on key

#include "config/game-control.h"
#include "wx/keyedit.h"
#include "wx/sdljoy.h"

class wxJoyKeyTextCtrl : public wxKeyTextCtrl {
public:
    // default constructor; required for use with xrc
    // FIXME: clearable and keyenter should be style flags
    wxJoyKeyTextCtrl()
        : wxKeyTextCtrl()
    {
    }
    virtual ~wxJoyKeyTextCtrl(){};

    // convert mod+key to accel string, separated by -
    static wxString ToString(int mod, int key, int joy, bool isConfig = false);
    // parses single key string into mod+key
    static bool FromString(const wxString& s, int& mod, int& key, int& joy);
    // parse a single key in given wxChar array up to given len
    static bool ParseString(const wxString& s, int len, int& mod, int& key, int& joy);
    // parse multi-key string into array
    // returns empty array on parse errors
    static wxAcceleratorEntry_v ToAccelFromString(const wxString& s, wxChar sep = wxT(','));
    // convert multiple keys, separated by multikey
    static wxString FromAccelToString(wxAcceleratorEntry_v keys, wxChar sep = wxT(','), bool isConfig = false);

protected:
    void OnJoy(wxJoyEvent&);

    DECLARE_DYNAMIC_CLASS(wxJoyKeyTextCtrl);
    DECLARE_EVENT_TABLE();
};

// A simple copy-only validator
class wxJoyKeyValidator : public wxValidator {
public:
    wxJoyKeyValidator(const config::GameControl v)
        : wxValidator()
        , val_(v)
    {
    }
    wxJoyKeyValidator(const wxJoyKeyValidator& v)
        : wxValidator()
        , val_(v.val_)
    {
    }
    wxObject* Clone() const override
    {
        return new wxJoyKeyValidator(val_);
    }
    bool TransferToWindow() override;
    bool TransferFromWindow() override;
    bool Validate(wxWindow* p) override
    {
        (void)p; // unused params
        return true;
    }

protected:
    const config::GameControl val_;

    DECLARE_CLASS(wxJoyKeyValidator)
};

#endif /* WX_JOYKEYTEXT_H */
