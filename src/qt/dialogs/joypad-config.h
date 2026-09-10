#ifndef VBAM_QT_DIALOGS_JOYPAD_CONFIG_H_
#define VBAM_QT_DIALOGS_JOYPAD_CONFIG_H_

#include <array>
#include <memory>
#include <unordered_map>

#include "qt/config/bindings.h"
#include "qt/config/command.h"
#include "qt/dialogs/base-dialog.h"

class QCheckBox;
class QSpinBox;
class QTabWidget;

namespace widgets {
class UserInputCtrl;
}

namespace dialogs {

// The Joypad configuration dialog: one tab per emulated joypad, each with a
// Standard and a Special sub-tab of input capture controls. Edits are made on
// a copy of the bindings and committed on OK.
class JoypadConfig final : public BaseDialog {
    Q_OBJECT

public:
    static JoypadConfig* NewInstance(QWidget* parent,
                                    const config::BindingsProvider bindings_provider);
    ~JoypadConfig() override = default;

protected:
    void OnDialogShown() override;
    bool OnAccept() override;

private:
    JoypadConfig(QWidget* parent, const config::BindingsProvider bindings_provider);

    QWidget* CreatePlayerTab(const config::GameJoy& joypad);
    QWidget* CreateSubTab(QWidget* parent, const config::GameJoy& joypad,
                          const std::vector<config::GameKey>& keys);

    // Loads the controls of every joypad from `bindings_`.
    void LoadControls();

    void ResetToDefaults(const config::GameJoy& joypad);
    void ClearJoypad(const config::GameJoy& joypad);
    void ClearAllJoypads();
    void ToggleSDLGameControllerMode(bool checked);
    void OnDefaultToggled(const config::GameJoy& joypad, bool checked);

    const config::BindingsProvider bindings_provider_;
    // Working copy of the bindings while the dialog is shown.
    config::Bindings bindings_;

    QTabWidget* notebook_;
    QCheckBox* game_controller_mode_;
    QSpinBox* autofire_throttle_;
    std::unordered_map<config::GameCommand, widgets::UserInputCtrl*> controls_;
    std::array<QCheckBox*, config::kNbJoypads> default_checks_ = {};
    bool loading_ = false;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_JOYPAD_CONFIG_H_
