// utility widgets
#include "wx/wxmisc.h"
#include <wx/wx.h>

wxFarRadio::wxFarRadio()
    : wxCheckBox()
{
    Next = this;
}

wxFarRadio::~wxFarRadio()
{
    BreakGroup();
}

void wxFarRadio::SetValue(bool val)
{
    wxCheckBox::SetValue(val);
    UpdatedValue();
}

void wxFarRadio::SetGroup(class wxFarRadio* grp)
{
    if (grp == this)
        return;

    wxFarRadio* checked = GetValue() ? this : NULL;

    for (wxFarRadio* gp = Next; gp != this; gp = gp->Next) {
        if (gp == grp)
            return;

        if (gp->GetValue())
            checked = gp;
    }

    wxFarRadio* link = Next;
    Next = grp;
    bool clear_checked = false;
    wxFarRadio* gp;

    for (gp = grp; gp->Next != grp; gp = gp->Next) {
        if (checked && GetValue())
            clear_checked = true;
    }

    gp->Next = link;

    if (checked && gp->GetValue())
        clear_checked = true;

    if (clear_checked)
        checked->SetValue(false);

    int l;

    for (l = 1, gp = Next; gp != this; gp = gp->Next, l++)
        ;
}

void wxFarRadio::BreakGroup()
{
    wxFarRadio** gp;

    for (gp = &Next; *gp != this; gp = &(*gp)->Next)
        ;

    *gp = Next;
    Next = this;
}

wxFarRadio* wxFarRadio::GetNext()
{
    return Next;
}

void wxFarRadio::UpdatedValue()
{
    if (!GetValue()) {
        wxFarRadio* gp;

        // just like system wx, ensure at least one always checked
        for (gp = Next; gp != this; gp = gp->Next)
            if (gp->GetValue())
                break;

        if (gp == this) {
            SetValue(true);
            return;
        }
    } else
        for (wxFarRadio* gp = Next; gp != this; gp = gp->Next)
            gp->SetValue(false);
}

void wxFarRadio::UpdateEvt(wxCommandEvent& ev)
{
    UpdatedValue();
    ev.Skip();
}

IMPLEMENT_DYNAMIC_CLASS(wxFarRadio, wxCheckBox);
BEGIN_EVENT_TABLE(wxFarRadio, wxCheckBox)
EVT_CHECKBOX(wxID_ANY, wxFarRadio::UpdateEvt)
END_EVENT_TABLE()

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
        return true;
    }

    wxDirPickerCtrl* dp = wxDynamicCast(GetWindow(), wxDirPickerCtrl);

    if (dp) {
        *vptr = dp->GetPath();
        return true;
    }

    return false;
}

#include <wx/clrpicker.h>

bool wxColorValidator::TransferToWindow()
{
    if (!ptr32 && !ptr16)
        return false;

    wxColourPickerCtrl* cp = wxDynamicCast(GetWindow(), wxColourPickerCtrl);

    if (!cp)
        return false;

    if (ptr32)
        cp->SetColour(wxColor(((*ptr32 >> 16) & 0xff),
            ((*ptr32 >> 8) & 0xff),
            ((*ptr32) & 0xff)));
    else
        cp->SetColour(wxColor(((*ptr16 << 3) & 0xf8),
            ((*ptr16 >> 2) & 0xf8),
            ((*ptr16 >> 7) & 0xf8)));

    return true;
}

bool wxColorValidator::TransferFromWindow()
{
    if (!ptr32 && !ptr16)
        return false;

    wxColourPickerCtrl* cp = wxDynamicCast(GetWindow(), wxColourPickerCtrl);

    if (!cp)
        return false;

    wxColor c = cp->GetColour();

    if (ptr32)
        *ptr32 = ((c.Red() & 0xff) << 16) + ((c.Green() & 0xff) << 8) + ((c.Blue() & 0xff));
    else
        *ptr16 = ((c.Red() & 0xf8) >> 3) + ((c.Green() & 0xf8) << 2) + ((c.Blue() & 0xf8) << 7);

    return true;
}

static void enable(wxWindow_v controls, std::vector<int> reverse, bool en)
{
    for (int i = 0; i < controls.size(); i++)
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


wxPositiveDoubleValidator::wxPositiveDoubleValidator(double* _val)
    : wxGenericValidator(&str_val)
    , double_val(_val)
{
    if (double_val) {
        str_val = wxString::Format(wxT("%.1f"), *double_val);
        TransferToWindow();
    }
}

bool wxPositiveDoubleValidator::TransferToWindow()
{
    if (double_val) {
        str_val = wxString::Format(wxT("%.1f"), *double_val);
        return wxGenericValidator::TransferToWindow();
    }
    return true;
}

bool wxPositiveDoubleValidator::TransferFromWindow()
{
    if (wxGenericValidator::TransferFromWindow()) {
        if (double_val) {
            return str_val.ToDouble(double_val);
        }
    }
    return false;
}

bool wxPositiveDoubleValidator::Validate(wxWindow* parent)
{
    if (wxGenericValidator::Validate(parent)) {
        wxTextCtrl* ctrl = wxDynamicCast(GetWindow(), wxTextCtrl);

        if (ctrl) {
            wxString cur_txt = ctrl->GetValue();
            double val;
            if (cur_txt.ToDouble(&val)) {
                return val >= 0;
            }
            return false;
        }

        return true;
    }
    return false;
}

wxObject* wxPositiveDoubleValidator::Clone() const
{
    return new wxPositiveDoubleValidator(double_val);
}
