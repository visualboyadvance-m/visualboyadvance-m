#ifndef VBAM_QT_DIALOGS_GAME_BOY_ADVANCE_CONFIG_H_
#define VBAM_QT_DIALOGS_GAME_BOY_ADVANCE_CONFIG_H_

#include <QString>

#include "qt/dialogs/base-dialog.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QWidget;

namespace widgets {
class PathPicker;
}

namespace dialogs {

// Manages the Game Boy Advance configuration dialog: save type, boot ROM and
// the per-game overrides (vba-over.ini).
class GameBoyAdvanceConfig final : public BaseDialog {
    Q_OBJECT

public:
    static GameBoyAdvanceConfig* NewInstance(QWidget* parent);
    ~GameBoyAdvanceConfig() override = default;

    // Returns a valid override key for the loaded GBA ROM: the 4-char game
    // code if all bytes are printable ASCII, otherwise "CRC_XXXXXXXX".
    static QString GetOverrideId();

protected:
    void OnDialogShown() override;
    bool OnAccept() override;

private:
    explicit GameBoyAdvanceConfig(QWidget* parent);

    QWidget* CreateSaveTypeTab();
    QWidget* CreateBootRomTab();
    QWidget* CreateGameOverridesTab();

    void OnSaveTypeChanged(int index);
    void OnDetect();
    void LoadOverrides();
    void SaveOverrides();
    void ResetOverrideControls();

    QTabWidget* notebook_;

    // Save type tab.
    QComboBox* save_type_ = nullptr;
    QComboBox* flash_size_ = nullptr;
    QPushButton* detect_ = nullptr;

    // Boot ROM tab.
    widgets::PathPicker* bios_picker_ = nullptr;
    QLabel* bios_label_ = nullptr;

    // Game overrides tab.
    QWidget* game_settings_ = nullptr;
    QLabel* game_code_ = nullptr;
    QLineEdit* comment_ = nullptr;
    QComboBox* ov_rtc_ = nullptr;
    QComboBox* ov_save_type_ = nullptr;
    QComboBox* ov_flash_size_ = nullptr;
    QComboBox* ov_mirroring_ = nullptr;
    QString override_comment_on_show_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_GAME_BOY_ADVANCE_CONFIG_H_
