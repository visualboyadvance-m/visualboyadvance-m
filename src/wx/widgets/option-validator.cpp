#include "widgets/option-validator.h"

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

}  // namespace widgets
