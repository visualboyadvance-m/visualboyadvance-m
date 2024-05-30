#include "wx/widgets/option-validator.h"

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>

#include "core/base/check.h"

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
    OnValueChanged();
}
#endif

void OptionValidator::OnValueChanged() {
    VBAM_CHECK(WriteToWindow());
}

OptionSelectedValidator::OptionSelectedValidator(config::OptionID option_id, uint32_t value)
    : OptionValidator(option_id), value_(value) {
    VBAM_CHECK(option()->is_unsigned());
    VBAM_CHECK(value_ >= option()->GetUnsignedMin());
    VBAM_CHECK(value_ <= option()->GetUnsignedMax());
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

    VBAM_NOTREACHED();
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

    const wxRadioButton* radio_button = wxDynamicCast(GetWindow(), wxRadioButton);
    if (radio_button) {
        if (radio_button->GetValue()) {
            option()->SetUnsigned(value_);
        }
        return true;
    }

    VBAM_NOTREACHED();
    return false;
}

OptionIntValidator::OptionIntValidator(config::OptionID option_id) : OptionValidator(option_id) {
    VBAM_CHECK(option()->is_int());
}

wxObject* OptionIntValidator::Clone() const {
    return new OptionIntValidator(option()->id());
}

bool OptionIntValidator::IsWindowValueValid() {
    return true;
}

bool OptionIntValidator::WriteToWindow() {
    wxSpinCtrl* spin_ctrl = wxDynamicCast(GetWindow(), wxSpinCtrl);
    if (spin_ctrl) {
        spin_ctrl->SetValue(option()->GetInt());
        return true;
    }

    wxSlider* slider = wxDynamicCast(GetWindow(), wxSlider);
    if (slider) {
        slider->SetValue(option()->GetInt());
        return true;
    }

    VBAM_NOTREACHED();
    return false;
}

bool OptionIntValidator::WriteToOption() {
    const wxSpinCtrl* spin_ctrl = wxDynamicCast(GetWindow(), wxSpinCtrl);
    if (spin_ctrl) {
        return option()->SetInt(spin_ctrl->GetValue());
    }

    const wxSlider* slider = wxDynamicCast(GetWindow(), wxSlider);
    if (slider) {
        return option()->SetInt(slider->GetValue());
    }

    VBAM_NOTREACHED();
    return false;
}

OptionChoiceValidator::OptionChoiceValidator(config::OptionID option_id)
    : OptionValidator(option_id) {
    VBAM_CHECK(option()->is_unsigned());
}

wxObject* OptionChoiceValidator::Clone() const {
    return new OptionChoiceValidator(option()->id());
}

bool OptionChoiceValidator::IsWindowValueValid() {
    return true;
}

bool OptionChoiceValidator::WriteToWindow() {
    wxChoice* choice = wxDynamicCast(GetWindow(), wxChoice);
    if (choice) {
        choice->SetSelection(option()->GetUnsigned());
        return true;
    }

    VBAM_NOTREACHED();
    return false;
}

bool OptionChoiceValidator::WriteToOption() {
    const wxChoice* choice = wxDynamicCast(GetWindow(), wxChoice);
    if (choice) {
        return option()->SetUnsigned(choice->GetSelection());
    }

    VBAM_NOTREACHED();
    return false;
}

OptionBoolValidator::OptionBoolValidator(config::OptionID option_id) : OptionValidator(option_id) {
    VBAM_CHECK(option()->is_bool());
}

wxObject* OptionBoolValidator::Clone() const {
    return new OptionBoolValidator(option()->id());
}

bool OptionBoolValidator::IsWindowValueValid() {
    return true;
}

bool OptionBoolValidator::WriteToWindow() {
    wxCheckBox* checkbox = wxDynamicCast(GetWindow(), wxCheckBox);
    if (checkbox) {
        checkbox->SetValue(option()->GetBool());
        return true;
    }

    VBAM_NOTREACHED();
    return false;
}

bool OptionBoolValidator::WriteToOption() {
    const wxCheckBox* checkbox = wxDynamicCast(GetWindow(), wxCheckBox);
    if (checkbox) {
        return option()->SetBool(checkbox->GetValue());
    }

    VBAM_NOTREACHED();
    return false;
}

}  // namespace widgets
