#ifndef VBAM_QT_DIALOGS_SPEEDUP_CONFIG_H_
#define VBAM_QT_DIALOGS_SPEEDUP_CONFIG_H_

// Manages the Speedup/Turbo configuration dialog.
//
// See the explanation for the implementation in the .cpp file.

#include "qt/dialogs/base-dialog.h"

class QCheckBox;
class QSpinBox;

namespace dialogs {

class SpeedupConfig final : public BaseDialog {
    Q_OBJECT

public:
    static SpeedupConfig* NewInstance(QWidget* parent);
    ~SpeedupConfig() override = default;

private:
    explicit SpeedupConfig(QWidget* parent);

    // Loads the spin box / check box from the options.
    void LoadFromOptions();
    // Writes the spin box / check box to the options.
    bool SaveToOptions();

    void OnSpinValueChanged(int value);
    void OnEditingFinished();
    void ToggleSpeedupFrameSkip();

    QSpinBox* speedup_throttle_spin_;
    QCheckBox* frame_skip_cb_;
    bool prev_frame_skip_cb_ = false;
    unsigned prev_throttle_spin_ = 0;
    bool loading_ = false;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_SPEEDUP_CONFIG_H_
