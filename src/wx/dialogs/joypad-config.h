#ifndef VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_
#define VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_

#include "wx/config/bindings.h"
#include "wx/dialogs/base-dialog.h"

namespace dialogs {

// Manages the Joypad configuration dialog.
// Note that this dialog will silently overwrite shortcut assignments.
class JoypadConfig : public BaseDialog {
public:
    static JoypadConfig* NewInstance(wxWindow* parent,
                                     const config::BindingsProvider bindings_provider);
    ~JoypadConfig() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    JoypadConfig(wxWindow* parent, const config::BindingsProvider bindings_provider);

    // Resets all Joypad controls for `panel` to defaults.
    void ResetToDefaults(wxWindow* panel);

    // Clears all Joypad controls.
    void ClearJoypad(wxWindow* panel);

    // Clears all Joypad controls for all Joypads.
    void ClearAllJoypads();

    // Toggle SDL GameController mode for all joysticks.
    void ToggleSDLGameControllerMode();

    const config::BindingsProvider bindings_provider_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_
