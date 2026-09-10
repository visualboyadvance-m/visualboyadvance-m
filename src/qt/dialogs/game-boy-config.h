#ifndef VBAM_QT_DIALOGS_GAME_BOY_CONFIG_H_
#define VBAM_QT_DIALOGS_GAME_BOY_CONFIG_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "qt/config/option.h"
#include "qt/dialogs/base-dialog.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;

namespace widgets {
class PathPicker;
}

namespace dialogs {

// Manages the Game Boy configuration dialog: emulated system, borders,
// printer, boot ROMs and the three custom palettes.
class GameBoyConfig final : public BaseDialog {
    Q_OBJECT

public:
    static GameBoyConfig* NewInstance(QWidget* parent);
    ~GameBoyConfig() override = default;

private:
    explicit GameBoyConfig(QWidget* parent);

    QWidget* CreateSystemTab();
    QWidget* CreateBootRomTab();
    QWidget* CreateCustomColorsTab();
    QWidget* CreatePaletteTab(QWidget* parent, size_t palette_id);

    // One custom palette editor.
    struct PalettePanel {
        config::OptionID option_id;
        QComboBox* default_selector = nullptr;
        QCheckBox* use_palette = nullptr;
        std::array<QPushButton*, 8> colour_buttons = {};
        std::array<uint16_t, 8> palette = {};
    };

    void UpdateColourButtons(PalettePanel& panel);
    void OnColourClicked(PalettePanel& panel, size_t colour_index);
    void OnDefaultPaletteSelected(PalettePanel& panel, int selection);
    void OnPaletteReset(PalettePanel& panel);
    void OnUsePaletteToggled(size_t palette_id, bool checked);

    QTabWidget* notebook_;
    QTabWidget* inner_notebook_ = nullptr;
    std::vector<PalettePanel> palettes_;
    QLabel* gb_bios_label_ = nullptr;
    QLabel* gbc_bios_label_ = nullptr;
    widgets::PathPicker* gb_bios_picker_ = nullptr;
    widgets::PathPicker* gbc_bios_picker_ = nullptr;
    bool loading_ = false;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_GAME_BOY_CONFIG_H_
