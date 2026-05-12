#include "wx/dialogs/joypad-config.h"

#include <wx/checkbox.h>
#include <wx/log.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/xrc/xmlres.h>

#include "core/base/check.h"
#include "wx/config/command.h"
#include "wx/config/option-proxy.h"
#include "wx/dialogs/base-dialog.h"
#include "wx/widgets/client-data.h"
#include "wx/widgets/option-validator.h"
#include "wx/widgets/user-input-ctrl.h"
#include "wx/widgets/utils.h"

namespace dialogs {

namespace {

using GameJoyClientData = widgets::ClientData<config::GameJoy>;

// A validator for the UserInputCtrl. This validator is used to transfer the
// GameControl data to and from the UserInputCtrl. `bindings_provider` must
// outlive this object.
class UserInputCtrlValidator : public wxValidator {
public:
    explicit UserInputCtrlValidator(const config::GameCommand game_control,
                                    const config::BindingsProvider bindings_provider);
    ~UserInputCtrlValidator() override = default;

    wxObject* Clone() const override;

protected:
    // wxValidator implementation.
    bool TransferToWindow() override;
    bool TransferFromWindow() override;
    bool Validate(wxWindow*) override { return true; }

    const config::GameCommand game_control_;
    const config::BindingsProvider bindings_provider;
};

UserInputCtrlValidator::UserInputCtrlValidator(const config::GameCommand game_control,
                                               config::BindingsProvider const bindings_provider)
    : wxValidator(), game_control_(game_control), bindings_provider(bindings_provider) {
    VBAM_CHECK(bindings_provider);
}

wxObject* UserInputCtrlValidator::Clone() const {
    return new UserInputCtrlValidator(game_control_, bindings_provider);
}

bool UserInputCtrlValidator::TransferToWindow() {
    widgets::UserInputCtrl* control = wxDynamicCast(GetWindow(), widgets::UserInputCtrl);
    VBAM_CHECK(control);

    control->SetInputs(bindings_provider()->InputsForCommand(config::Command(game_control_)));
    return true;
}

bool UserInputCtrlValidator::TransferFromWindow() {
    widgets::UserInputCtrl* control = wxDynamicCast(GetWindow(), widgets::UserInputCtrl);
    VBAM_CHECK(control);

    bindings_provider()->ClearCommandAssignments(config::Command(game_control_));
    for (const auto& input : control->inputs()) {
        bindings_provider()->AssignInputToCommand(input, config::Command(game_control_));
    }

    return true;
}

}  // namespace

// static
JoypadConfig* JoypadConfig::NewInstance(wxWindow* parent,
                                        const config::BindingsProvider bindings_provider) {
    VBAM_CHECK(parent);
    VBAM_CHECK(bindings_provider);
    return new JoypadConfig(parent, bindings_provider);
}

JoypadConfig::JoypadConfig(wxWindow* parent, const config::BindingsProvider bindings_provider)
    : BaseDialog(parent, "JoypadConfig"),
      tab_loaded_(8, false),
      bindings_provider_(bindings_provider) {
    notebook_ = GetValidatedChild<wxNotebook>("JoypadConfigNotebook");

    this->Bind(wxEVT_CHECKBOX, std::bind(&JoypadConfig::ToggleSDLGameControllerMode, this),
               XRCID("SDLGameControllerMode"));

    GetValidatedChild<wxCheckBox>("SDLGameControllerMode")
        ->SetValue(OPTION(kSDLGameControllerMode));

    GetValidatedChild("AutofireThrottle")
        ->SetValidator(
            widgets::OptionIntValidator(config::OptionID::kJoyAutofireThrottle));
}

bool JoypadConfig::LoadLazyTab(int index) {
    if (index < 0 || index >= 8 || tab_loaded_[index]) {
        return false;
    }

    // Index layout: 0/1 = P1 Standard/Special, 2/3 = P2, 4/5 = P3, 6/7 = P4.
    // Even indices load the player's outer JoyPanel skeleton plus its
    // Standard sub-tab. Odd indices load only the Special sub-tab and
    // therefore depend on the matching even index being loaded first.
    const unsigned ux_index = static_cast<unsigned>(index / 2) + 1;
    const int sub_index = index % 2;
    const int paired_skeleton_index = index - sub_index;

    if (sub_index == 1 && !tab_loaded_[paired_skeleton_index]) {
        LoadLazyTab(paired_skeleton_index);
    }
    if (sub_index == 0 &&
        ux_index - 1 != static_cast<unsigned>(notebook_->GetPageCount())) {
        // Earlier players not loaded yet; load them in order so AddPage
        // appends pages in the right slot.
        for (int i = 0; i < index; ++i) {
            if (!tab_loaded_[i]) {
                LoadLazyTab(i);
            }
        }
    }

    wxPanel* player_panel = nullptr;
    if (sub_index == 0) {
        player_panel = LoadPlayerSkeleton(ux_index);
        if (!player_panel) {
            return false;
        }
    } else {
        player_panel = wxDynamicCast(
            FindWindow(wxString::Format("joy%u", ux_index)), wxPanel);
        VBAM_CHECK(player_panel);
    }

    LoadPlayerSubTab(player_panel, ux_index, sub_index);
    tab_loaded_[index] = true;
    Fit();
    return true;
}

wxPanel* JoypadConfig::LoadPlayerSkeleton(unsigned ux_index) {
    wxPanel* panel = wxXmlResource::Get()->LoadPanel(notebook_, "JoyPanel");
    if (!panel) {
        wxLogError(_("Failed to load JoyPanel"));
        return nullptr;
    }
    // The XRC names every instance "JoyPanel"; rename to joy{N} so dialog-
    // level lookups can identify which player this panel belongs to.
    panel->SetName(wxString::Format("joy%u", ux_index));
    notebook_->AddPage(panel, wxString::Format(_("Player %u"), ux_index));

    // Per-player DefaultConfig validator and the Defaults/Clear buttons
    // live on the JoyPanel skeleton, not on its sub-tabs.
    const config::GameJoy* joypad_match = nullptr;
    for (const config::GameJoy& joypad : config::kAllGameJoys) {
        if (joypad.ux_index() == ux_index) {
            joypad_match = &joypad;
            break;
        }
    }
    VBAM_CHECK(joypad_match);
    panel->SetClientObject(new GameJoyClientData(*joypad_match));

    widgets::GetValidatedChild(panel, "DefaultConfig")
        ->SetValidator(widgets::OptionSelectedValidator(
            config::OptionID::kJoyDefault, ux_index));

    panel->Bind(wxEVT_BUTTON,
                std::bind(&JoypadConfig::ResetToDefaults, this, panel),
                XRCID("Defaults"));
    panel->Bind(wxEVT_BUTTON,
                std::bind(&JoypadConfig::ClearJoypad, this, panel),
                XRCID("Clear"));
    return panel;
}

void JoypadConfig::LoadPlayerSubTab(wxPanel* player_panel, unsigned ux_index,
                                    int sub_index) {
    wxNotebook* inner =
        widgets::GetValidatedChild<wxNotebook>(player_panel, "JoyPanelNotebook");

    const wxString xrc_name =
        sub_index == 0 ? wxT("JoyPanelStandard") : wxT("JoyPanelSpecial");
    const wxString label = sub_index == 0 ? _("Standard") : _("Special");

    wxPanel* sub_panel =
        wxXmlResource::Get()->LoadPanel(inner, xrc_name);
    if (!sub_panel) {
        wxLogError(_("Failed to load joypad sub-tab '%s'"), xrc_name);
        return;
    }
    inner->AddPage(sub_panel, label);
    InitPlayerSubTab(player_panel, sub_panel, ux_index);
}

void JoypadConfig::InitPlayerSubTab(wxPanel* player_panel, wxPanel* sub_panel,
                                    unsigned ux_index) {
    const config::GameJoy* joypad_match = nullptr;
    for (const config::GameJoy& joypad : config::kAllGameJoys) {
        if (joypad.ux_index() == ux_index) {
            joypad_match = &joypad;
            break;
        }
    }
    VBAM_CHECK(joypad_match);
    const config::GameJoy& joypad = *joypad_match;

    // Walk every GameKey and wire any control that happens to live in this
    // sub-panel. Keys that belong to the other sub-tab are silently skipped.
    wxWindow* prev = nullptr;
    for (const config::GameKey& game_key : config::kAllGameKeys) {
        const wxString game_key_name = config::GameKeyToString(game_key);
        wxWindow* found = sub_panel->FindWindow(game_key_name);
        if (!found) continue;

        widgets::UserInputCtrl* game_key_control =
            wxDynamicCast(found, widgets::UserInputCtrl);
        VBAM_CHECK(game_key_control);

        game_key_control->SetValidator(UserInputCtrlValidator(
            config::GameCommand(joypad, game_key), bindings_provider_));

        if (prev) {
            game_key_control->MoveAfterInTabOrder(prev);
        }
        prev = game_key_control;

        // Per-key Clear button. Bind on the player_panel so the event
        // handler is rooted at the level that stays alive for the
        // lifetime of the dialog.
        player_panel->Bind(
            wxEVT_BUTTON,
            std::bind(&widgets::UserInputCtrl::Clear, game_key_control),
            XRCID(wxString("Clear" + game_key_name).c_str()));
    }
}

void JoypadConfig::ResetToDefaults(wxWindow* panel) {
    const config::GameJoy& joypad = GameJoyClientData::From(panel);
    for (const config::GameKey& game_key : config::kAllGameKeys) {
        widgets::GetValidatedChild<widgets::UserInputCtrl>(panel, config::GameKeyToString(game_key))
            ->SetInputs(bindings_provider_()->DefaultInputsForCommand(
                config::GameCommand(joypad, game_key)));
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
