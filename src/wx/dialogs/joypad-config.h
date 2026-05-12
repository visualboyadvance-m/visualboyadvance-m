#ifndef VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_
#define VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_

#include <vector>

#include "wx/config/bindings.h"
#include "wx/dialogs/base-dialog.h"

class wxNotebook;

namespace dialogs {

// Manages the Joypad configuration dialog.
// Note that this dialog will silently overwrite shortcut assignments.
class JoypadConfig : public BaseDialog {
public:
    static JoypadConfig* NewInstance(wxWindow* parent,
                                     const config::BindingsProvider bindings_provider);
    ~JoypadConfig() override = default;

    // 4 player tabs × 2 inner sub-tabs (Standard, Special) = 8 lazy slots.
    int LazyTabCount() const override { return 8; }
    bool LoadLazyTab(int index) override;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    JoypadConfig(wxWindow* parent, const config::BindingsProvider bindings_provider);

    // Loads the JoyPanel skeleton for one player and adds it to the outer
    // notebook. Caller is responsible for then populating the player's
    // Standard / Special sub-tabs via LoadPlayerSubTab.
    wxPanel* LoadPlayerSkeleton(unsigned ux_index);

    // Loads a player's Standard (sub_index=0) or Special (sub_index=1)
    // sub-tab and runs its per-tab init.
    void LoadPlayerSubTab(wxPanel* player_panel, unsigned ux_index, int sub_index);

    // Wires validators / event handlers for all controls in a freshly-loaded
    // sub-tab. Called once per sub-tab.
    void InitPlayerSubTab(wxPanel* player_panel, wxPanel* sub_panel, unsigned ux_index);

    // Resets all Joypad controls for `panel` to defaults.
    void ResetToDefaults(wxWindow* panel);

    // Clears all Joypad controls.
    void ClearJoypad(wxWindow* panel);

    // Clears all Joypad controls for all Joypads.
    void ClearAllJoypads();

    // Toggle SDL GameController mode for all joysticks.
    void ToggleSDLGameControllerMode();

    wxNotebook* notebook_ = nullptr;
    std::vector<bool> tab_loaded_;
    const config::BindingsProvider bindings_provider_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_
