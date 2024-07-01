#ifndef VBAM_WX_DIALOGS_SPEEDUP_CONFIG_H_
#define VBAM_WX_DIALOGS_SPEEDUP_CONFIG_H_

// Manages the Speedup/Turbo configuration dialog.
//
// See the explanation for the implementation in the .cpp file.

#include <wx/checkbox.h>
#include "wx/dialogs/base-dialog.h"
#include <wx/spinctrl.h>
#include <wx/validate.h>

namespace dialogs {

class SpeedupConfigValidator : public wxValidator {
public:
    SpeedupConfigValidator() = default;
    ~SpeedupConfigValidator() override = default;

    bool TransferToWindow() override;
    bool TransferFromWindow() override;

    bool Validate([[maybe_unused]]wxWindow* parent) override { return true; }

private:
    wxObject* Clone() const override { return new SpeedupConfigValidator(); }
};

class SpeedupConfig : public BaseDialog {
public:
    static SpeedupConfig* NewInstance(wxWindow* parent);
    ~SpeedupConfig() override = default;

    void ToggleSpeedupFrameSkip();
    void ToggleSpeedupMute();
    void SetSpeedupThrottle(wxCommandEvent& evt);

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    SpeedupConfig(wxWindow* parent);

    wxSpinCtrl* speedup_throttle_spin_;
    wxCheckBox* frame_skip_cb_;
    bool prev_frame_skip_cb_;
    unsigned prev_throttle_spin_;

    friend class SpeedupConfigValidator;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_SPEEDUP_CONFIG_H_
