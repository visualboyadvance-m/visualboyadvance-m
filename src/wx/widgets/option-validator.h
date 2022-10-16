#ifndef VBAM_WX_WIDGETS_OPTION_VALIDATOR_H_
#define VBAM_WX_WIDGETS_OPTION_VALIDATOR_H_

#include <wx/validate.h>

#include "config/option.h"

#if wxCHECK_VERSION(3, 1, 1)
#define WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE 1
#else
#define WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE 0
#endif  // wxCHECK_VERSION(3,1,1)

namespace widgets {

// Generic wxWidgets validator for a config::Option. This base class acts as a
// Controller in a Model-View-Controller pattern. Implementers should only have
// to provide the OptionID and implement the 4 methods indicated below.
//
// Sample usage:
//
// class MyOptionValidator : public widgets::OptionValidator {
// public:
//     MyOptionValidator()
//         : widgets::OptionValidator(config::OptionID::kOptionID) {}
//     ~MyOptionValidator() override = default;
//
// private:
//     // widgets::OptionValidator implementation.
//     wxObject* Clone() const override {
//         return MyOptionValidator();
//     }
//
//     bool IsWindowValueValid() override {
//         wxWindow* window = GetWindow();
//         // Validate window value here.
//     }
//
//     bool WriteToWindow() override {
//         wxWindow* window = GetWindow();
//         OptionType value = option()->GetType();
//         return window->SetValue(value);
//     }
//
//     bool WriteToOption() override {
//         wxWindow* window = GetWindow();
//         return option()->SetType(window->GetValue());
//     }
//
// };
//
// void DialogInitializer() {
//     wxWindow* window = dialog->FindWindow("WindowId");
//     window->SetValidator(MyOptionValidator());
// }
class OptionValidator : public wxValidator, public config::Option::Observer {
public:
    explicit OptionValidator(config::OptionID option_id);
    ~OptionValidator() override = default;

    // Returns a copy of the object.
    wxObject* Clone() const override = 0;

    // Returns true if the value held by GetWindow() is valid.
    [[nodiscard]] virtual bool IsWindowValueValid() = 0;

    // Updates the GetWindow() value, from the option() value.
    // Returns true on success, false on failure.
    [[nodiscard]] virtual bool WriteToWindow() = 0;

    // Updates the option() value, from the GetWindow() value.
    // Returns true on success, false on failure.
    [[nodiscard]] virtual bool WriteToOption() = 0;

private:
    // wxValidator implementation.
    bool TransferFromWindow() final;
    bool Validate(wxWindow*) final;
    bool TransferToWindow() final;
#if WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    void SetWindow(wxWindow* window) final;
#endif

    // config::Option::Observer implementation.
    void OnValueChanged() final;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_OPTION_VALIDATOR_H_
