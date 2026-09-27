#include "wx/widgets/spin-value-ctrl.h"

#include <algorithm>
#include <cmath>

#include <wx/sizer.h>
#include <wx/spinbutt.h>
#include <wx/textctrl.h>

namespace widgets {

namespace {

// The spin button counts whole steps from the minimum.
int StepCount(double min, double max, double step) {
    return static_cast<int>(std::lround((max - min) / step));
}

}  // namespace

SpinValueCtrl::SpinValueCtrl(wxWindow* parent, wxWindowID id, double min, double max,
                             double step, int decimals, const wxString& suffix,
                             const wxString& special_text)
    : wxPanel(parent, id),
      min_(min),
      max_(std::max(min, max)),
      step_(step > 0 ? step : 1),
      decimals_(std::max(0, decimals)),
      suffix_(suffix),
      special_text_(special_text),
      value_(min) {
    text_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                           wxTE_PROCESS_ENTER);
    spin_ = new wxSpinButton(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxSP_VERTICAL | wxSP_ARROW_KEYS);
    spin_->SetRange(0, StepCount(min_, max_, step_));

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(text_, 1, wxALIGN_CENTER_VERTICAL);
    sizer->Add(spin_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
    SetSizer(sizer);

    // Wide enough for the longest of the extremes and the special text.
    int widest = 0;
    for (const wxString& s : {Format(min_), Format(max_), special_text_}) {
        widest = std::max(widest, text_->GetTextExtent(s + wxT("00")).GetWidth());
    }
    text_->SetMinSize(wxSize(std::max(widest, text_->FromDIP(60)), -1));

    spin_->Bind(wxEVT_SPIN, &SpinValueCtrl::OnSpin, this);
    text_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { Commit(); });
    text_->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
        Commit();
        event.Skip();
    });

    SetValue(min_);
    Fit();
}

void SpinValueCtrl::SetValue(double value) {
    if (!std::isfinite(value))
        value = min_;
    value_ = std::min(std::max(value, min_), max_);
    spin_->SetValue(static_cast<int>(std::lround((value_ - min_) / step_)));
    UpdateText();
}

bool SpinValueCtrl::IsSpecialValue() const {
    return !special_text_.empty() && value_ <= min_;
}

void SpinValueCtrl::OnSpin(wxSpinEvent& event) {
    // Snap to the step grid, as a Qt spin box's arrows do.
    value_ = std::min(std::max(min_ + event.GetPosition() * step_, min_), max_);
    UpdateText();
}

void SpinValueCtrl::Commit() {
    wxString typed = text_->GetValue().Strip(wxString::both);
    if (!special_text_.empty() && typed.CmpNoCase(special_text_) == 0) {
        SetValue(min_);
        return;
    }
    if (!suffix_.empty() && typed.EndsWith(suffix_))
        typed.RemoveLast(suffix_.length());
    typed.Trim();

    // Either decimal separator, whatever the locale says.
    wxString dot = typed;
    dot.Replace(wxT(","), wxT("."));
    double value;
    if (dot.ToCDouble(&value) || typed.ToDouble(&value))
        SetValue(value);
    else
        UpdateText();  // not a number: show the old value again
}

void SpinValueCtrl::UpdateText() {
    text_->ChangeValue(IsSpecialValue() ? special_text_ : Format(value_) + suffix_);
}

wxString SpinValueCtrl::Format(double value) const {
    // "%.*f" follows the C locale wxWidgets set up, so the separator is the
    // user's.
    return wxString::Format(wxT("%.*f"), decimals_, value);
}

}  // namespace widgets
