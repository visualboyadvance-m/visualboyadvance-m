#include "wx/widgets/option-validator.h"

#include <gtest/gtest.h>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/spinctrl.h>
#include <wx/window.h>
#include <wx/xrc/xmlres.h>

#include "core/base/test/notreached.h"
#include "wx/config/option.h"
#include "wx/widgets/test/widgets-test.h"

namespace widgets {

TEST_F(WidgetsTest, OptionSelectedValidator) {
    wxCheckBox* check_box_0 = new wxCheckBox(frame(), XRCID("Zero"), "Zero");
    wxCheckBox* check_box_1 = new wxCheckBox(frame(), XRCID("One"), "One");
    config::Option* option = config::Option::ByID(config::OptionID::kPrefCaptureFormat);

    check_box_0->SetValidator(OptionSelectedValidator(config::OptionID::kPrefCaptureFormat, 0));
    check_box_1->SetValidator(OptionSelectedValidator(config::OptionID::kPrefCaptureFormat, 1));

    // Tick the first checkbox, the second should be unticked.
    check_box_0->SetValue(true);
    frame()->TransferDataFromWindow();
    EXPECT_EQ(option->GetUnsigned(), 0);
    EXPECT_FALSE(check_box_1->GetValue());

    // And vice-versa.
    check_box_1->SetValue(true);
    frame()->TransferDataFromWindow();
    EXPECT_EQ(option->GetUnsigned(), 1);
    EXPECT_FALSE(check_box_0->GetValue());

    // Set the kPrefCaptureFormat option to 0, the first checkbox should be ticked.
    option->SetUnsigned(0);
    frame()->TransferDataToWindow();
    EXPECT_TRUE(check_box_0->GetValue());
    EXPECT_FALSE(check_box_1->GetValue());

    // Set the kPrefCaptureFormat option to 1, the second checkbox should be ticked.
    option->SetUnsigned(1);
    frame()->TransferDataToWindow();
    EXPECT_FALSE(check_box_0->GetValue());
    EXPECT_TRUE(check_box_1->GetValue());
}

TEST_F(WidgetsTest, OptionSelectedValidator_InvalidData) {
    wxCheckBox* check_box = new wxCheckBox(frame(), XRCID("Checbox"), "Checkbox");

    // The value is higher than the max for this option.
    EXPECT_DEATH(
        check_box->SetValidator(OptionSelectedValidator(config::OptionID::kPrefCaptureFormat, 2)),
        "value_ <= option\\(\\)->GetUnsignedMax\\(\\)");

    // The option is not an unsigned option.
    EXPECT_DEATH(check_box->SetValidator(OptionSelectedValidator(config::OptionID::kDispFilter, 0)),
                 "option\\(\\)->is_unsigned\\(\\)");

    // Unsupported wxWindow type.
    wxWindow* window = new wxWindow(frame(), XRCID("Window"));
#if WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    VBAM_EXPECT_NOTREACHED(
        window->SetValidator(OptionSelectedValidator(config::OptionID::kPrefCaptureFormat, 0)));
#else   // WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    window->SetValidator(OptionSelectedValidator(config::OptionID::kPrefCaptureFormat, 0));
    VBAM_EXPECT_NOTREACHED(frame()->TransferDataToWindow());
#endif  // WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
}

TEST_F(WidgetsTest, OptionIntValidator) {
    wxSpinCtrl* spin_ctrl = new wxSpinCtrl(frame(), XRCID("SpinCtrl"));
    config::Option* option = config::Option::ByID(config::OptionID::kDispMaxThreads);

    spin_ctrl->SetValidator(OptionIntValidator(config::OptionID::kDispMaxThreads));

    // Set the kPrefCaptureFormat option to 0, the spin control should be set to 0.
    option->SetInt(0);
    frame()->TransferDataToWindow();
    EXPECT_EQ(spin_ctrl->GetValue(), 0);

    // Set the spin control to 100, the kPrefCaptureFormat option should be set to 100.
    spin_ctrl->SetValue(100);
    frame()->TransferDataFromWindow();
    EXPECT_EQ(option->GetInt(), 100);
}

TEST_F(WidgetsTest, OptionIntValidator_InvalidData) {
    wxSpinCtrl* spin_ctrl = new wxSpinCtrl(frame(), XRCID("SpinCtrl"));

    // The option is not an int option.
    EXPECT_DEATH(spin_ctrl->SetValidator(OptionIntValidator(config::OptionID::kDispFilter)),
                 "option\\(\\)->is_int\\(\\)");

    // Unsupported wxWindow type.
    wxWindow* window = new wxWindow(frame(), XRCID("Window"));
#if WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    VBAM_EXPECT_NOTREACHED(
        window->SetValidator(OptionIntValidator(config::OptionID::kDispMaxThreads)));
#else   // WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    window->SetValidator(OptionIntValidator(config::OptionID::kDispMaxThreads));
    VBAM_EXPECT_NOTREACHED(frame()->TransferDataToWindow());
#endif  // WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
}

TEST_F(WidgetsTest, OptionChoiceValidator) {
    wxChoice* choice = new wxChoice(frame(), XRCID("Choice"));
    choice->Append("Zero");
    choice->Append("One");
    config::Option* option = config::Option::ByID(config::OptionID::kPrefCaptureFormat);

    choice->SetValidator(OptionChoiceValidator(config::OptionID::kPrefCaptureFormat));

    // Set the kPrefCaptureFormat option to 0, the first item should be selected.
    option->SetUnsigned(0);
    frame()->TransferDataToWindow();
    EXPECT_EQ(choice->GetSelection(), 0);

    // Set the kPrefCaptureFormat option to 1, the second item should be selected.
    option->SetUnsigned(1);
    frame()->TransferDataToWindow();
    EXPECT_EQ(choice->GetSelection(), 1);

    // Select the first item, the kPrefCaptureFormat option should be 0.
    choice->SetSelection(0);
    frame()->TransferDataFromWindow();
    EXPECT_EQ(option->GetUnsigned(), 0);

    // Select the second item, the kPrefCaptureFormat option should be 1.
    choice->SetSelection(1);
    frame()->TransferDataFromWindow();
    EXPECT_EQ(option->GetUnsigned(), 1);
}

TEST_F(WidgetsTest, OptionChoiceValidator_InvalidData) {
    wxChoice* choice = new wxChoice(frame(), XRCID("Choice"));

    // The option is not an unsigned option.
    EXPECT_DEATH(choice->SetValidator(OptionChoiceValidator(config::OptionID::kDispFilter)),
                 "option\\(\\)->is_unsigned\\(\\)");

    // Unsupported wxWindow type.
    wxWindow* window = new wxWindow(frame(), XRCID("Window"));
#if WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    VBAM_EXPECT_NOTREACHED(
        window->SetValidator(OptionChoiceValidator(config::OptionID::kPrefCaptureFormat)));
#else   // WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    window->SetValidator(OptionChoiceValidator(config::OptionID::kPrefCaptureFormat));
    VBAM_EXPECT_NOTREACHED(frame()->TransferDataToWindow());
#endif  // WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
}

}  // namespace widgets
