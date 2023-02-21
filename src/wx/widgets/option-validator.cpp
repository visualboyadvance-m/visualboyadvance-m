#include "widgets/option-validator.h"

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/spinctrl.h>

namespace widgets {

OptionValidator::OptionValidator(config::OptionID option_id)
    : wxValidator(), config::Option::Observer(option_id) {}

bool OptionValidator::TransferFromWindow() {
    return WriteToOption();
}

bool OptionValidator::Validate(wxWindow*) {
    return IsWindowValueValid();
}

bool OptionValidator::TransferToWindow() {
    return WriteToWindow();
}

#if WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
void OptionValidator::SetWindow(wxWindow* window) {
    wxValidator::SetWindow(window);
    [[maybe_unused]] const bool write_success = WriteToWindow();
    assert(write_success);
}
#endif

void OptionValidator::OnValueChanged() {
    [[maybe_unused]] const bool write_success = WriteToWindow();
    assert(write_success);
}

OptionSelectedValidator::OptionSelectedValidator(config::OptionID option_id,
                                                 uint32_t value)
    : OptionValidator(option_id), value_(value) {
    assert(option()->is_unsigned());
    assert(value_ >= option()->GetUnsignedMin());
    assert(value_ <= option()->GetUnsignedMax());
}

wxObject* OptionSelectedValidator::Clone() const {
    return new OptionSelectedValidator(option()->id(), value_);
}

bool OptionSelectedValidator::IsWindowValueValid() {
    return true;
}

bool OptionSelectedValidator::WriteToWindow() {
    wxCheckBox* checkbox = wxDynamicCast(GetWindow(), wxCheckBox);
    if (checkbox) {
        checkbox->SetValue(option()->GetUnsigned() == value_);
        return true;
    }

    wxRadioButton* radio_button = wxDynamicCast(GetWindow(), wxRadioButton);
    if (radio_button) {
        radio_button->SetValue(option()->GetUnsigned() == value_);
        return true;
    }

    assert(false);
    return false;
}

bool OptionSelectedValidator::WriteToOption() {
    const wxCheckBox* checkbox = wxDynamicCast(GetWindow(), wxCheckBox);
    if (checkbox) {
        if (checkbox->GetValue()) {
            option()->SetUnsigned(value_);
        }
        return true;
    }

    const wxRadioButton* radio_button =
        wxDynamicCast(GetWindow(), wxRadioButton);
    if (radio_button) {
        if (radio_button->GetValue()) {
            option()->SetUnsigned(value_);
        }
        return true;
    }

    assert(false);
    return false;
}

OptionSpinCtrlValidator::OptionSpinCtrlValidator(config::OptionID option_id)
    : OptionValidator(option_id) {
    assert(option()->is_int());
}

wxObject* OptionSpinCtrlValidator::Clone() const {
    return new OptionSpinCtrlValidator(option()->id());
}

bool OptionSpinCtrlValidator::IsWindowValueValid() {
    return true;
}

bool OptionSpinCtrlValidator::WriteToWindow() {
    wxSpinCtrl* spin_ctrl = wxDynamicCast(GetWindow(), wxSpinCtrl);
    assert(spin_ctrl);
    spin_ctrl->SetValue(option()->GetInt());
    return true;
}

bool OptionSpinCtrlValidator::WriteToOption() {
    const wxSpinCtrl* spin_ctrl = wxDynamicCast(GetWindow(), wxSpinCtrl);
    assert(spin_ctrl);
    return option()->SetInt(spin_ctrl->GetValue());
}

OptionChoiceValidator::OptionChoiceValidator(config::OptionID option_id)
    : OptionValidator(option_id) {
    assert(option()->is_unsigned());
}

wxObject* OptionChoiceValidator::Clone() const {
    return new OptionChoiceValidator(option()->id());
}

bool OptionChoiceValidator::IsWindowValueValid() {
    return true;
}

bool OptionChoiceValidator::WriteToWindow() {
    wxChoice* choice = wxDynamicCast(GetWindow(), wxChoice);
    assert(choice);
    choice->SetSelection(option()->GetUnsigned());
    return true;
}

bool OptionChoiceValidator::WriteToOption() {
    const wxChoice* choice = wxDynamicCast(GetWindow(), wxChoice);
    assert(choice);
    return option()->SetUnsigned(choice->GetSelection());
}

}  // namespace widgets
