#ifndef VBAM_WX_WIDGETS_OPTION_VALIDATOR_H_
#define VBAM_WX_WIDGETS_OPTION_VALIDATOR_H_

#include <wx/validate.h>

#include "wx/config/option.h"

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

// "Generic" validator for a wxChecBox or wxRadioButton widget with a kUnsigned
// Option. This will make sure the kUnsigned Option and the wxRadioButton or
// wxCheckBox are kept in sync. The widget will be checked if the kUnsigned
// Option matches the provided `value` parameter in the constructor.
class OptionSelectedValidator : public OptionValidator {
public:
    OptionSelectedValidator(config::OptionID option_id, uint32_t value);
    ~OptionSelectedValidator() override = default;

    // Returns a copy of the object.
    wxObject* Clone() const override;

private:
    // OptionValidator implementation.
    bool IsWindowValueValid() override;
    bool WriteToWindow() override;
    bool WriteToOption() override;

    const uint32_t value_;
};

// Validator for a wxSpinCtrl  widget with a kInt Option. This will keep the
// kInt Option and the wxSpinCtrl selection in sync.
class OptionSpinCtrlValidator : public OptionValidator {
public:
    explicit OptionSpinCtrlValidator(config::OptionID option_id);
    ~OptionSpinCtrlValidator() override = default;

    // Returns a copy of the object.
    wxObject* Clone() const override;

private:
    // OptionValidator implementation.
    bool IsWindowValueValid() override;
    bool WriteToWindow() override;
    bool WriteToOption() override;
};

// Validator for a wxChoice  widget with a kUnsigned Option. This will keep the
// kUnsigned Option and the wxChoice selection in sync.
class OptionChoiceValidator : public OptionValidator {
public:
    explicit OptionChoiceValidator(config::OptionID option_id);
    ~OptionChoiceValidator() override = default;

    // Returns a copy of the object.
    wxObject* Clone() const override;

private:
    // OptionValidator implementation.
    bool IsWindowValueValid() override;
    bool WriteToWindow() override;
    bool WriteToOption() override;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_OPTION_VALIDATOR_H_
