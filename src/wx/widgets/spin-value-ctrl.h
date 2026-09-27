#ifndef VBAM_WX_WIDGETS_SPIN_VALUE_CTRL_H_
#define VBAM_WX_WIDGETS_SPIN_VALUE_CTRL_H_

#include <wx/panel.h>
#include <wx/string.h>

class wxSpinButton;
class wxSpinEvent;
class wxTextCtrl;

namespace widgets {

// A number box with up/down arrows, like wxSpinCtrlDouble, that can also show
// a suffix ("px", "%") after the number and a word in place of its minimum,
// as Qt's QAbstractSpinBox::specialValueText does ("Profile", "Off" for a
// setting left to something else). The value moves in steps of `step` from
// `min`, shown with `decimals` decimals in the user's locale; typing either
// decimal separator, the suffix or the special text is accepted.
class SpinValueCtrl : public wxPanel {
public:
    SpinValueCtrl(wxWindow* parent, wxWindowID id, double min, double max, double step,
                  int decimals, const wxString& suffix = wxEmptyString,
                  const wxString& special_text = wxEmptyString);
    ~SpinValueCtrl() override = default;

    double GetValue() const { return value_; }
    // Clamped to [min, max].
    void SetValue(double value);

    // True while the box shows its minimum and has a special text for it.
    bool IsSpecialValue() const;

    double GetMin() const { return min_; }
    double GetMax() const { return max_; }

private:
    void OnSpin(wxSpinEvent& event);
    // Reads what was typed, then shows it back formatted.
    void Commit();
    void UpdateText();
    wxString Format(double value) const;

    wxTextCtrl* text_ = nullptr;
    wxSpinButton* spin_ = nullptr;
    const double min_;
    const double max_;
    const double step_;
    const int decimals_;
    const wxString suffix_;
    const wxString special_text_;
    double value_;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_SPIN_VALUE_CTRL_H_
