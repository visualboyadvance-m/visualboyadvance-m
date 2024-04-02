#ifndef VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_
#define VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_

#include <wx/dialog.h>

#include "wx/widgets/keep-on-top-styler.h"

namespace dialogs {

// Manages the Joypad configuration dialog.
class JoypadConfig : public wxDialog {
public:
    static JoypadConfig* NewInstance(wxWindow* parent);
    ~JoypadConfig() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    JoypadConfig(wxWindow* parent);

    // Resets all Joypad controls for `panel` to defaults.
    void ResetToDefaults(wxWindow* panel);

    // Clears all Joypad controls.
    void ClearJoypad(wxWindow* panel);

    // Clears all Joypad controls for all Joypads.
    void ClearAllJoypads();
    
    // Toggle SDL GameController mode for all joysticks.
    void ToggleSDLGameControllerMode();

    const widgets::KeepOnTopStyler keep_on_top_styler_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_JOYPAD_CONFIG_H_
