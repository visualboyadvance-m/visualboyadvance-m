#ifndef WX_MISC_H
#define WX_MISC_H
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

// for wxTextValidator include lists
extern const wxArrayString val_hexdigits, val_sigdigits, val_unsdigits;

#endif /* WX_MISC_H */
