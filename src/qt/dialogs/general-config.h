#ifndef VBAM_QT_DIALOGS_GENERAL_CONFIG_H_
#define VBAM_QT_DIALOGS_GENERAL_CONFIG_H_

#include "qt/dialogs/base-dialog.h"

class QComboBox;
class QSpinBox;

namespace dialogs {

// The General options dialog: screenshot format, rewind interval, throttle and
// the miscellaneous UI / emulation toggles.
class GeneralConfig final : public BaseDialog {
    Q_OBJECT

public:
    static GeneralConfig* NewInstance(QWidget* parent);
    ~GeneralConfig() override = default;

protected:
    void OnDialogShown() override;

private:
    explicit GeneralConfig(QWidget* parent);

    // Throttle spin control / canned setting choice interaction.
    void SetThrottleSel(int value);
    void SetThrottle(int selection);

    QSpinBox* throttle_spin_;
    QComboBox* throttle_sel_;
    bool syncing_ = false;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_GENERAL_CONFIG_H_
