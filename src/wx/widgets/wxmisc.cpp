// utility widgets

#include <cctype>
#include <string>
#include <algorithm>

#include "wx/wxmisc.h"
#include <wx/wx.h>
#include <wx/spinctrl.h>

bool wxBoolIntValidator::TransferToWindow()
{
    if (!vptr)
        return false;

    bool checked = (*vptr & mask) == val;
    wxCheckBox* cb = wxDynamicCast(GetWindow(), wxCheckBox);

    if (cb) {
        cb->SetValue(checked);
        return true;
    }

    wxRadioButton* rb = wxDynamicCast(GetWindow(), wxRadioButton);

    if (rb) {
        rb->SetValue(checked);
        return true;
    }

    return false;
}

bool wxBoolIntValidator::TransferFromWindow()
{
    if (!vptr)
        return false;

    bool nv = false;
    wxCheckBox* cb = wxDynamicCast(GetWindow(), wxCheckBox);

    if (cb)
        nv = cb->GetValue();
    else {
        wxRadioButton* rb = wxDynamicCast(GetWindow(), wxRadioButton);

        if (rb)
            nv = rb->GetValue();
        else
            return false;
    }

    if (mask == ~0 && !nv && *vptr != val)
        return true;

    *vptr = (*vptr & ~mask) | (nv ? val : 0);
    return true;
}

bool wxBoolRevValidator::TransferToWindow()
{
    if (!vptr)
        return false;

    wxCheckBox* cb = wxDynamicCast(GetWindow(), wxCheckBox);

    if (cb) {
        cb->SetValue(!*vptr);
        return true;
    }

    wxRadioButton* rb = wxDynamicCast(GetWindow(), wxRadioButton);

    if (rb) {
        rb->SetValue(!*vptr);
        return true;
    }

    return false;
}

bool wxBoolRevValidator::TransferFromWindow()
{
    if (!vptr)
        return false;

    wxCheckBox* cb = wxDynamicCast(GetWindow(), wxCheckBox);

    if (cb)
        *vptr = !cb->GetValue();
    else {
        wxRadioButton* rb = wxDynamicCast(GetWindow(), wxRadioButton);

        if (rb)
            *vptr = !rb->GetValue();
        else
            return false;
    }

    return true;
}

#include <wx/filepicker.h>

bool wxFileDirPickerValidator::TransferToWindow()
{
    if (!vptr)
        return false;

    wxFilePickerCtrl* fp = wxDynamicCast(GetWindow(), wxFilePickerCtrl);

    if (fp) {
        fp->SetPath(*vptr);
        return true;
    }

    wxDirPickerCtrl* dp = wxDynamicCast(GetWindow(), wxDirPickerCtrl);

    if (dp) {
        dp->SetPath(*vptr);
        return true;
    }

    return false;
}

bool wxFileDirPickerValidator::TransferFromWindow()
{
    if (!vptr)
        return false;

    wxFilePickerCtrl* fp = wxDynamicCast(GetWindow(), wxFilePickerCtrl);

    if (fp) {
        *vptr = fp->GetPath();
        if (vlabel) vlabel->SetLabel(*vptr);
        return true;
    }

    wxDirPickerCtrl* dp = wxDynamicCast(GetWindow(), wxDirPickerCtrl);

    if (dp) {
        *vptr = dp->GetPath();
        if (vlabel) vlabel->SetLabel(*vptr);
        return true;
    }

    return false;
}

static void enable(wxWindow_v controls, std::vector<int> reverse, bool en)
{
    for (size_t i = 0; i < controls.size(); i++)
        controls[i]->Enable(reverse.size() <= i || !reverse[i] ? en : !en);
}

#define boolen(r)                                                          \
    do {                                                                   \
        int en;                                                            \
        wxCheckBox* cb = wxDynamicCast(GetWindow(), wxCheckBox);           \
        if (cb)                                                            \
            en = cb->GetValue();                                           \
        else {                                                             \
            wxRadioButton* rb = wxDynamicCast(GetWindow(), wxRadioButton); \
            if (!rb)                                                       \
                return false;                                              \
            en = rb->GetValue();                                           \
        }                                                                  \
        enable(controls, reverse, r ? !en : en);                           \
        return true;                                                       \
    } while (0)

bool wxBoolEnValidator::TransferToWindow()
{
    if (!wxGenericValidator::TransferToWindow())
        return false;

    boolen(false);
}

bool wxBoolIntEnValidator::TransferToWindow()
{
    if (!wxBoolIntValidator::TransferToWindow())
        return false;

    boolen(false);
}

bool wxBoolRevEnValidator::TransferToWindow()
{
    if (!wxBoolRevValidator::TransferToWindow())
        return false;

    boolen(true);
}

void wxBoolEnHandler::ToggleCheck(wxCommandEvent& ev)
{
    enable(controls, reverse, ev.IsChecked());
    ev.Skip();
}

void wxBoolEnHandler::Enable(wxCommandEvent& ev)
{
    enable(controls, reverse, true);
    ev.Skip();
}

void wxBoolEnHandler::Disable(wxCommandEvent& ev)
{
    enable(controls, reverse, false);
    ev.Skip();
}

static const wxString  val_hexdigits_s[] = {
    wxT("0"), wxT("1"), wxT("2"), wxT("3"), wxT("4"), wxT("5"), wxT("6"),
    wxT("7"), wxT("8"), wxT("9"), wxT("A"), wxT("B"), wxT("C"), wxT("D"),
    wxT("E"), wxT("F"), wxT("a"), wxT("b"), wxT("c"), wxT("d"), wxT("e"),
    wxT("f")
};

const wxArrayString val_hexdigits(sizeof(val_hexdigits_s) / sizeof(val_hexdigits_s[0]),
    val_hexdigits_s);

static const wxString  val_sigdigits_s[] = {
    wxT("0"), wxT("1"), wxT("2"), wxT("3"), wxT("4"), wxT("5"), wxT("6"),
    wxT("7"), wxT("8"), wxT("9"), wxT("-"), wxT("+")
};

const wxArrayString val_sigdigits(sizeof(val_sigdigits_s) / sizeof(val_sigdigits_s[0]),
    val_sigdigits_s);

static const wxString  val_unsdigits_s[] = {
    wxT("0"), wxT("1"), wxT("2"), wxT("3"), wxT("4"), wxT("5"), wxT("6"),
    wxT("7"), wxT("8"), wxT("9")
};

const wxArrayString val_unsdigits(sizeof(val_unsdigits_s) / sizeof(val_unsdigits_s[0]),
    val_unsdigits_s);

wxUIntValidator::wxUIntValidator(uint32_t* _val)
    : uint_val(_val)
{
    if (uint_val)
        TransferToWindow();
}

bool wxUIntValidator::TransferToWindow()
{
    if (uint_val) {
        wxSpinCtrl* spin = wxDynamicCast(GetWindow(), wxSpinCtrl);
        if (spin) {
            spin->SetValue(*uint_val);
            return true;
        }

        wxTextCtrl* txt = wxDynamicCast(GetWindow(), wxTextCtrl);
        if (txt) {
            txt->SetValue(wxString::Format(wxT("%d"), *uint_val));
            return true;
        }
    }

    return false;
}

bool wxUIntValidator::TransferFromWindow()
{
    if (uint_val) {
        wxSpinCtrl* spin = wxDynamicCast(GetWindow(), wxSpinCtrl);
        if (spin) {
            *uint_val = spin->GetValue();
            return true;
        }

        wxTextCtrl* txt = wxDynamicCast(GetWindow(), wxTextCtrl);
        if (txt) {
            *uint_val = wxAtoi(txt->GetValue());
            return true;
        }
    }

    return false;
}

bool wxUIntValidator::Validate(wxWindow* parent)
{
    (void)parent; // unused params

    wxSpinCtrl* spin = wxDynamicCast(GetWindow(), wxSpinCtrl);
    if (spin) {
        if (spin->GetValue() >= 0) {
            return true;
        }
        return false;
    }

    wxTextCtrl* txt = wxDynamicCast(GetWindow(), wxTextCtrl);
    if (txt) {
        std::string val = std::string(txt->GetValue().mb_str());

        return !val.empty()
            && std::find_if(val.begin(), val.end(), [](unsigned char c) { return !std::isdigit(c); }) == val.end();
    }

    return false;
}

wxObject* wxUIntValidator::Clone() const
{
    return new wxUIntValidator(uint_val);
}
