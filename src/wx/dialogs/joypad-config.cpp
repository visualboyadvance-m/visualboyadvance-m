#include "wx/dialogs/joypad-config.h"

#include <wx/checkbox.h>

#include "wx/config/option-proxy.h"
#include "wx/dialogs/base-dialog.h"
#include "wx/opts.h"
#include "wx/widgets/option-validator.h"
#include "wx/widgets/user-input-ctrl.h"
#include "wx/widgets/utils.h"

namespace dialogs {

// static
JoypadConfig* JoypadConfig::NewInstance(wxWindow* parent) {
    assert(parent);
    return new JoypadConfig(parent);
}

JoypadConfig::JoypadConfig(wxWindow* parent) : BaseDialog(parent, "JoypadConfig") {
    this->Bind(wxEVT_CHECKBOX, std::bind(&JoypadConfig::ToggleSDLGameControllerMode, this),
               XRCID("SDLGameControllerMode"));

    GetValidatedChild<wxCheckBox>("SDLGameControllerMode")
        ->SetValue(OPTION(kSDLGameControllerMode));

    for (int joypad = 0; joypad < 4; joypad++) {
        wxWindow* panel = GetValidatedChild(wxString::Format("joy%d", joypad + 1));

        widgets::GetValidatedChild(panel, "DefaultConfig")
            ->SetValidator(
                widgets::OptionSelectedValidator(config::OptionID::kJoyDefault, joypad + 1));

        // Set up tab order so input is easy to configure. Note that there are
        // two tabs for each panel, so we must check for the parent before
        // setting up the tab order.
        wxWindow* prev = nullptr;
        wxWindow* prev_parent = nullptr;
        for (const config::GameKey& game_key : config::kAllGameKeys) {
            const wxString game_key_name = config::GameKeyToString(game_key);
            widgets::UserInputCtrl* game_key_control =
                widgets::GetValidatedChild<widgets::UserInputCtrl>(panel, game_key_name);
            wxWindow* current_parent = game_key_control->GetParent();

            game_key_control->SetValidator(
                widgets::UserInputCtrlValidator(config::GameControl(joypad, game_key)));

            if (current_parent == prev_parent) {
                // The first control will be skipped here, but that's fine since
                // we don't care where it fits in the tab order.
                assert(prev);
                game_key_control->MoveAfterInTabOrder(prev);
            }
            prev = game_key_control;
            prev_parent = current_parent;

            // Bind the individual "Clear" key event.
            panel->Bind(wxEVT_BUTTON, std::bind(&widgets::UserInputCtrl::Clear, game_key_control),
                        XRCID(wxString("Clear" + config::GameKeyToString(game_key)).c_str()));
        }

        // Finally, bind the per-joypad "Defaults" and "Clear" events.
        panel->Bind(wxEVT_BUTTON, std::bind(&JoypadConfig::ResetToDefaults, this, panel),
                    XRCID("Defaults"));
        panel->Bind(wxEVT_BUTTON, std::bind(&JoypadConfig::ClearJoypad, this, panel),
                    XRCID("Clear"));
    }

    this->Fit();
}

void JoypadConfig::ResetToDefaults(wxWindow* panel) {
    for (const config::GameKey& game_key : config::kAllGameKeys) {
        widgets::GetValidatedChild<widgets::UserInputCtrl>(panel, config::GameKeyToString(game_key))
            ->SetInputs(kDefaultBindings.find(config::GameControl(0, game_key))->second);
    }
}

void JoypadConfig::ClearJoypad(wxWindow* panel) {
    for (const config::GameKey& game_key : config::kAllGameKeys) {
        widgets::GetValidatedChild<widgets::UserInputCtrl>(panel, config::GameKeyToString(game_key))
            ->Clear();
    }
}

void JoypadConfig::ToggleSDLGameControllerMode() {
    OPTION(kSDLGameControllerMode) =
        GetValidatedChild<wxCheckBox>("SDLGameControllerMode")->IsChecked();
    ClearAllJoypads();
}

void JoypadConfig::ClearAllJoypads() {
    for (unsigned joypad = 0; joypad < 4; joypad++) {
        wxWindow* panel = GetValidatedChild(wxString::Format("joy%d", joypad + 1));

        ClearJoypad(panel);
    }
}

}  // namespace dialogs
