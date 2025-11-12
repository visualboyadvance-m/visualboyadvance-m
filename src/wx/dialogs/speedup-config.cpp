#include "wx/dialogs/speedup-config.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/event.h>
#include <wx/spinctrl.h>
#include <wx/xrc/xmlres.h>

#include "core/base/check.h"
#include "wx/config/option-proxy.h"
#include "wx/dialogs/base-dialog.h"
#include "wx/widgets/option-validator.h"

// This dialog has a spin control representing the speedup value as it feels
// like to the user. Values below 450 can be an actual throttle or a number of
// frames to skip depending on whether the Frame Skip checkbox is checked. The
// number of frames is the spin control value divided by 100, for example 300
// with the Frame Skip checkbox checked means 3 frames to skip. Values above 450
// are always the number of frames to skip, for example 900 means 9 frames to skip.
//
// The config option SpeedupThrottle is the throttle value, if frame skip is
// used it is 0. The config option SpeedupFrameSkip is the number of frames to
// skip, if throttle is used it is 0. The config option SpeedupThrottleFrameSkip
// represents the Frame Skip checkbox for values under 450, that is if they are
// interpreted as a frame skip or a throttle.

namespace dialogs {

bool SpeedupConfigValidator::TransferToWindow() {
    uint32_t opt_frame_skip           = OPTION(kPrefSpeedupFrameSkip);
    uint32_t opt_throttle             = OPTION(kPrefSpeedupThrottle);
    bool     opt_throttle_frame_skip  = OPTION(kPrefSpeedupThrottleFrameSkip);

    auto dialog = static_cast<SpeedupConfig*>(GetWindow());

    dialog->prev_frame_skip_cb_ = opt_throttle_frame_skip;

    if (opt_frame_skip != 0) {
        dialog->speedup_throttle_spin_->SetValue(opt_frame_skip * 100);
        dialog->prev_throttle_spin_ = opt_frame_skip * 100;

        dialog->frame_skip_cb_->SetValue(true);
        dialog->frame_skip_cb_->Disable();
    } else {
        dialog->speedup_throttle_spin_->SetValue(opt_throttle);
        dialog->prev_throttle_spin_ = opt_throttle;

        dialog->frame_skip_cb_->SetValue(opt_throttle_frame_skip);

        if (opt_throttle != 0)
            dialog->frame_skip_cb_->Enable();
        else
            dialog->frame_skip_cb_->Disable();
    }

    return true;
}

bool SpeedupConfigValidator::TransferFromWindow() {
    auto dialog = static_cast<SpeedupConfig*>(GetWindow());
    uint32_t val = dialog->speedup_throttle_spin_->GetValue();

    if (val == 0) {
        OPTION(kPrefSpeedupThrottle)          = 0;
        OPTION(kPrefSpeedupFrameSkip)         = 0;
        OPTION(kPrefSpeedupThrottleFrameSkip) = false;
    } else if (val <= OPTION(kPrefSpeedupThrottle).Max()) {
        OPTION(kPrefSpeedupThrottle)          = val;
        OPTION(kPrefSpeedupFrameSkip)         = 0;
        OPTION(kPrefSpeedupThrottleFrameSkip) = dialog->frame_skip_cb_->GetValue();
    } else { // val > throttle_max
        OPTION(kPrefSpeedupThrottle)          = 100;
        OPTION(kPrefSpeedupFrameSkip)         = val / 100;
        OPTION(kPrefSpeedupThrottleFrameSkip) = false;
    }

    return true;
}

// static
SpeedupConfig* SpeedupConfig::NewInstance(wxWindow* parent) {
    VBAM_CHECK(parent);
    return new SpeedupConfig(parent);
}

SpeedupConfig::SpeedupConfig(wxWindow* parent)
    : BaseDialog(parent, "SpeedupConfig") {

    speedup_throttle_spin_ = GetValidatedChild<wxSpinCtrl>("SpeedupThrottleSpin");
    frame_skip_cb_         = GetValidatedChild<wxCheckBox>("SpeedupThrottleFrameSkip");

    speedup_throttle_spin_->SetRange(OPTION(kPrefSpeedupThrottle).Min(), OPTION(kPrefSpeedupFrameSkip).Max() * 100);

    // Add style flags instead of replacing them to preserve text editing capability
    long style = speedup_throttle_spin_->GetWindowStyle();
    style |= wxSP_ARROW_KEYS;
#ifdef wxTE_PROCESS_ENTER
    style |= wxTE_PROCESS_ENTER;
#endif
    speedup_throttle_spin_->SetWindowStyle(style);

    GetValidatedChild<wxCheckBox>("SpeedupMute")->SetValidator(widgets::OptionBoolValidator(config::OptionID::kPrefSpeedupMute));

    using namespace std::placeholders;

    Bind(wxEVT_SPIN_UP,     std::bind(&SpeedupConfig::SetSpeedupThrottle, this, _1), XRCID("SpeedupThrottleSpin"));
    Bind(wxEVT_SPIN_DOWN,   std::bind(&SpeedupConfig::SetSpeedupThrottle, this, _1), XRCID("SpeedupThrottleSpin"));
    Bind(wxEVT_SPIN,        std::bind(&SpeedupConfig::SetSpeedupThrottle, this, _1), XRCID("SpeedupThrottleSpin"));
    Bind(wxEVT_TEXT,        std::bind(&SpeedupConfig::SetSpeedupThrottle, this, _1), XRCID("SpeedupThrottleSpin"));
    Bind(wxEVT_CHECKBOX,    std::bind(&SpeedupConfig::ToggleSpeedupFrameSkip, this), XRCID("SpeedupThrottleFrameSkip"));

    // Filter input to only allow digits
    speedup_throttle_spin_->Bind(wxEVT_CHAR, [this](wxKeyEvent& evt) {
        int keycode = evt.GetKeyCode();

        // Handle Enter key - activate OK button
        if (keycode == WXK_RETURN || keycode == WXK_NUMPAD_ENTER) {
            wxButton* ok_button = static_cast<wxButton*>(FindWindow(wxID_OK));
            if (ok_button) {
                wxCommandEvent click_event(wxEVT_BUTTON, wxID_OK);
                click_event.SetEventObject(ok_button);
                ok_button->GetEventHandler()->ProcessEvent(click_event);
            }
            return; // Don't skip - we handled it
        }

        // Allow: digits, backspace, delete, arrow keys, tab
        if (wxIsdigit(keycode) ||
            keycode == WXK_BACK ||
            keycode == WXK_DELETE ||
            keycode == WXK_LEFT ||
            keycode == WXK_RIGHT ||
            keycode == WXK_HOME ||
            keycode == WXK_END ||
            keycode == WXK_TAB ||
            keycode < WXK_SPACE) { // Control characters
            evt.Skip(); // Allow the character
        }
        // Ignore all other characters
    });

    // Handle when focus leaves the spin control to apply any pending changes
    speedup_throttle_spin_->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& evt) {
        // Force apply value adjustments when focus leaves
        uint32_t val = speedup_throttle_spin_->GetValue();
        uint32_t original_val = val;

        // Update checkbox based on final value
        if (val == 0) {
            frame_skip_cb_->SetValue(false);
            frame_skip_cb_->Disable();
        } else if (val <= OPTION(kPrefSpeedupThrottle).Max()) {
            frame_skip_cb_->SetValue(prev_frame_skip_cb_);
            frame_skip_cb_->Enable();
        } else { // val > throttle_max
            val = std::floor((double)val / 100) * 100;
            uint32_t max = speedup_throttle_spin_->GetMax();
            if (val > max) val = max;

            frame_skip_cb_->SetValue(true);
            frame_skip_cb_->Disable();

            if (val != original_val) {
                speedup_throttle_spin_->SetValue(val);
                prev_throttle_spin_ = val;
            }
        }
        evt.Skip(); // Allow default focus handling
    });

    SetValidator(SpeedupConfigValidator());

    Fit();
}

void SpeedupConfig::SetSpeedupThrottle(wxCommandEvent& evt)
{
    VBAM_CHECK(evt.GetEventObject() == speedup_throttle_spin_);
    bool is_text_event = (evt.GetEventType() == wxEVT_TEXT);

    // For text events, parse the actual text value to get real-time updates
    uint32_t val;
    if (is_text_event) {
        wxString text = evt.GetString();
        long parsed_val;
        if (text.ToLong(&parsed_val) && parsed_val >= 0) {
            val = static_cast<uint32_t>(parsed_val);
        } else {
            val = 0;
        }
    } else {
        val = speedup_throttle_spin_->GetValue();
    }

    uint32_t original_val = val;

    // Update checkbox state based on current value
    if (val == 0) {
        frame_skip_cb_->SetValue(false);
        frame_skip_cb_->Disable();
    } else if (val <= OPTION(kPrefSpeedupThrottle).Max()) {
        frame_skip_cb_->SetValue(prev_frame_skip_cb_);
        frame_skip_cb_->Enable();
    } else { // val > throttle_max
        frame_skip_cb_->SetValue(true);
        frame_skip_cb_->Disable();

        // Only apply value adjustments for spin button events, not text events
        // This allows free typing without interruption
        if (!is_text_event) {
            if (val > prev_throttle_spin_)
                val += 100;

            val = std::floor((double)val / 100) * 100;

            uint32_t max = speedup_throttle_spin_->GetMax();

            if (val > max)
                val = max;
        }
    }

    // Only call SetValue for spin button events, not during typing
    if (!is_text_event && val != original_val) {
        speedup_throttle_spin_->SetValue(val);
    }

    prev_throttle_spin_ = is_text_event ? original_val : val;
}

void SpeedupConfig::ToggleSpeedupFrameSkip()
{
    prev_frame_skip_cb_ = frame_skip_cb_->GetValue();
}

}  // namespace dialogs
