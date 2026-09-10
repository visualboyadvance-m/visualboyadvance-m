#ifndef VBAM_QT_DIALOGS_DISPLAY_CONFIG_H_
#define VBAM_QT_DIALOGS_DISPLAY_CONFIG_H_

#include <memory>
#include <vector>

#include "qt/config/option-observer.h"
#include "qt/config/option.h"
#include "qt/dialogs/base-dialog.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QRadioButton;
class QSlider;
class QSpinBox;
class QTabWidget;

namespace widgets {
class PathPicker;
}

namespace dialogs {

// Manages the Display configuration dialog.
//
// The renderer, bit depth, frame skip, OSD and zoom settings are applied on
// OK. The display filter, interframe blending and the LCD color-correction
// controls apply as soon as they change (so the effect can be judged on the
// running game); Cancel puts the snapshot taken on show back.
class DisplayConfig final : public BaseDialog {
    Q_OBJECT

public:
    static DisplayConfig* NewInstance(QWidget* parent);
    ~DisplayConfig() override = default;

protected:
    void OnDialogShown() override;
    void OnDialogHidden() override;
    bool OnAccept() override;
    void reject() override;

private:
    explicit DisplayConfig(QWidget* parent);

    QWidget* CreateBasicTab();
    QWidget* CreateBitDepthTab();
    QWidget* CreateColorCorrectionTab();
    QWidget* CreateSpeedTab();
    QWidget* CreateOSDTab();
    QWidget* CreateZoomTab();

    // Populates the plugin selector from the plugins found in the plugin dir.
    void PopulatePluginOptions();
    void HidePluginOptions();
    void ShowPluginOptions();

    // Filter / plugin selector interaction.
    void OnFilterSelected(int index);
    void OnPluginSelected(int index);
    void OnPluginDirChanged(const QString& path);

    // Callbacks for the observers of kDispFilter / kDispIFB.
    void OnFilterChanged(config::Option* option);
    void OnInterframeChanged(config::Option* option);

    // Live-applied options: snapshot on show, restore on cancel.
    void SnapshotLiveOptions();
    void RestoreLiveOptions();
    void SyncLcdSliderEnable();

    void PopulateFullscreenModes();

    QTabWidget* notebook_;

    // Basic tab.
    std::vector<std::pair<QRadioButton*, config::RenderMethod>> render_method_radios_;
    // SDL render method sub-options (backend picker, pixel-art scaling), shown
    // while the SDL radio is selected.
    QWidget* sdl_options_ = nullptr;
    QComboBox* sdl_renderer_ = nullptr;
    QCheckBox* sdl_pixel_art_ = nullptr;
    void UpdateSdlOptionsVisibility();
    widgets::PathPicker* plugin_dir_picker_ = nullptr;
    QLabel* plugin_label_ = nullptr;
    QComboBox* filter_selector_ = nullptr;
    QComboBox* plugin_selector_ = nullptr;
    QComboBox* interframe_selector_ = nullptr;
    QCheckBox* bilinear_ = nullptr;
    QCheckBox* stretch_ = nullptr;
    QCheckBox* keep_on_top_ = nullptr;
    QSpinBox* max_threads_ = nullptr;
    bool plugin_options_visible_ = true;
    bool updating_selectors_ = false;

    // Color correction tab.
    std::vector<QRadioButton*> profile_radios_;
    QComboBox* gba_variant_ = nullptr;
    QComboBox* gb_variant_ = nullptr;
    QSlider* gba_darken_ = nullptr;
    QSlider* gbc_lighten_ = nullptr;

    // Zoom tab.
    QDoubleSpinBox* default_scale_ = nullptr;
    QSpinBox* max_scale_ = nullptr;
    QComboBox* fullscreen_mode_ = nullptr;

    struct LiveSnapshot {
        config::Filter filter;
        config::Interframe interframe;
        QString filter_plugin;
        uint32_t gba_variant;
        uint32_t gb_variant;
        uint32_t gba_darken;
        uint32_t gb_lighten;
        config::ColorCorrectionProfile profile;
        bool profile_auto;
    } live_snapshot_ = {};
    bool live_snapshot_taken_ = false;

    std::unique_ptr<config::OptionsObserver> filter_observer_;
    std::unique_ptr<config::OptionsObserver> interframe_observer_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_DISPLAY_CONFIG_H_
