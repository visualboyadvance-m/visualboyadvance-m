#ifndef VBAM_WX_WIDGETS_WX_MISC_H_
#define VBAM_WX_WIDGETS_WX_MISC_H_
// utility widgets

#include <cstdint>
#include <vector>

#include <wx/stattext.h>
#include <wx/valgen.h>

// boolean copy-only validator that uses a constant int
// may be attached to radio button or checkbox
class wxBoolIntValidator : public wxValidator {
public:
    wxBoolIntValidator(int* _vptr, int _val, int _mask = ~0)
        : wxValidator()
        , val(_val)
        , mask(_mask)
        , vptr(_vptr)
    {
    }
    wxBoolIntValidator(const wxBoolIntValidator& v)
        : wxValidator()
        , val(v.val)
        , mask(v.mask)
        , vptr(v.vptr)
    {
    }
    wxObject* Clone() const
    {
        return new wxBoolIntValidator(vptr, val, mask);
    }
    bool TransferToWindow();
    bool TransferFromWindow();
    bool Validate(wxWindow* p)
    {
        (void)p; // unused params
        return true;
    }

protected:
    int val, mask, *vptr;
};
class wxUIntValidator : public wxValidator {
public:
    wxUIntValidator(uint32_t* _val);
    bool TransferToWindow();
    bool TransferFromWindow();
    bool Validate(wxWindow* parent);
    wxObject* Clone() const;
protected:
    uint32_t* uint_val;
};

// boolean copy-only validator with reversed value
// may be attached to radio button or checkbox
class wxBoolRevValidator : public wxValidator {
public:
    wxBoolRevValidator(bool* _vptr)
        : wxValidator()
        , vptr(_vptr)
    {
    }
    wxBoolRevValidator(const wxBoolRevValidator& v)
        : wxValidator()
        , vptr(v.vptr)
    {
    }
    wxObject* Clone() const
    {
        return new wxBoolRevValidator(vptr);
    }
    bool TransferToWindow();
    bool TransferFromWindow();
    bool Validate(wxWindow* p)
    {
        (void)p; // unused params
        return true;
    }

protected:
    bool* vptr;
};

// wxFilePickerCtrl/wxDirPickerCtrl copy-only vvalidator
class wxFileDirPickerValidator : public wxValidator {
public:
    wxFileDirPickerValidator(wxString* _vptr, wxStaticText* _label = NULL)
        : wxValidator()
        , vptr(_vptr)
        , vlabel(_label)
    {
    }
    wxFileDirPickerValidator(const wxFileDirPickerValidator& v)
        : wxValidator()
        , vptr(v.vptr)
        , vlabel(v.vlabel)
    {
    }
    wxObject* Clone() const
    {
        return new wxFileDirPickerValidator(vptr, vlabel);
    }
    bool TransferToWindow();
    bool TransferFromWindow();
    bool Validate(wxWindow* p)
    {
        (void)p; // unused params
        return true;
    }

protected:
    wxString* vptr;
    wxStaticText* vlabel;
};

// Copy-only validators for checkboxes and radio buttons that enables a set
// of dependent widgets
// Requires an event handler during run-time

// there's probably a standard wxWindowList or some such, but it's
// undocumented and I prefer arrays
typedef std::vector<wxWindow*> wxWindow_v;

class wxBoolEnValidator : public wxGenericValidator {
public:
    wxBoolEnValidator(bool* vptr)
        : wxGenericValidator(vptr)
    {
    }
    wxBoolEnValidator(bool* vptr, wxWindow_v& cnt, std::vector<int> rev = std::vector<int>())
        : wxGenericValidator(vptr)
        , controls(cnt)
        , reverse(rev)
    {
    }
    wxBoolEnValidator(const wxBoolEnValidator& v)
        : wxGenericValidator(v)
        , controls(v.controls)
        , reverse(v.reverse)
    {
    }
    wxObject* Clone() const
    {
        return new wxBoolEnValidator(*this);
    }
    // set these after init, rather than in constructor
    wxWindow_v controls;
    // set reverse entries to true if disabled when checkbox checked
    // controls past the end of the reverse array are not reversed
    std::vector<int> reverse;
    // inherit validate, xferfrom from parent
    bool TransferToWindow();
};

class wxBoolIntEnValidator : public wxBoolIntValidator {
public:
    wxBoolIntEnValidator(int* vptr, int val, int mask = ~0)
        : wxBoolIntValidator(vptr, val, mask)
    {
    }
    wxBoolIntEnValidator(int* vptr, int val, int mask, wxWindow_v& cnt,
        std::vector<int> rev = std::vector<int>())
        : wxBoolIntValidator(vptr, val, mask)
        , controls(cnt)
        , reverse(rev)
    {
    }
    wxBoolIntEnValidator(const wxBoolIntEnValidator& v)
        : wxBoolIntValidator(v)
        , controls(v.controls)
        , reverse(v.reverse)
    {
    }
    wxObject* Clone() const
    {
        return new wxBoolIntEnValidator(*this);
    }
    // set these after init, rather than in constructor
    wxWindow_v controls;
    // set reverse entries to true if disabled when checkbox checked
    // controls past the end of the reverse array are not reversed
    std::vector<int> reverse;
    // inherit validate, xferfrom from parent
    bool TransferToWindow();
};

class wxBoolRevEnValidator : public wxBoolRevValidator {
public:
    wxBoolRevEnValidator(bool* vptr)
        : wxBoolRevValidator(vptr)
    {
    }
    wxBoolRevEnValidator(bool* vptr, wxWindow_v& cnt, std::vector<int> rev = std::vector<int>())
        : wxBoolRevValidator(vptr)
        , controls(cnt)
        , reverse(rev)
    {
    }
    wxBoolRevEnValidator(const wxBoolRevEnValidator& v)
        : wxBoolRevValidator(v)
        , controls(v.controls)
        , reverse(v.reverse)
    {
    }
    wxObject* Clone() const
    {
        return new wxBoolRevEnValidator(*this);
    }
    // set these after init, rather than in constructor
    wxWindow_v controls;
    // set reverse entries to true if disabled when checkbox checked
    // controls past the end of the reverse array are not reversed
    std::vector<int> reverse;
    // inherit validate, xferfrom from parent
    bool TransferToWindow();
};

// and here's an event handler that can be attached to the widget or a parent
// of the widget
// It is a descendent of wxEvtHandler so its methods can be used as
// event handlers
class wxBoolEnHandler : public wxEvtHandler {
public:
    wxWindow_v controls;
    std::vector<int> reverse;
    void ToggleCheck(wxCommandEvent& ev);
    void Enable(wxCommandEvent& ev);
    void Disable(wxCommandEvent& ev);
};

// use this to connect to the handler of id in win to obj
#define wxCBBoolEnHandlerConnect(win, id, obj)               \
    (win)->Connect(id,                                       \
        wxEVT_COMMAND_CHECKBOX_CLICKED,                      \
        wxCommandEventHandler(wxBoolEnHandler::ToggleCheck), \
        NULL,                                                \
        &obj)

#define wxRBBoolEnHandlerConnect(win, id, obj, f)  \
    (win)->Connect(id,                             \
        wxEVT_COMMAND_RADIOBUTTON_SELECTED,        \
        wxCommandEventHandler(wxBoolEnHandler::f), \
        NULL,                                      \
        &obj)

#define wxRBEBoolEnHandlerConnect(win, id, obj) wxRBBoolEnHandlerConnect(win, id, obj, Enable)
#define wxRBDBoolEnHandlerConnect(win, id, obj) wxRBBoolEnHandlerConnect(win, id, obj, Disable)

// for wxTextValidator include lists
extern const wxArrayString val_hexdigits, val_sigdigits, val_unsdigits;

#endif // VBAM_WX_WIDGETS_WX_MISC_H_
