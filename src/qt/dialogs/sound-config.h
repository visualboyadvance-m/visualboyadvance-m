#ifndef VBAM_QT_DIALOGS_SOUND_CONFIG_H_
#define VBAM_QT_DIALOGS_SOUND_CONFIG_H_

#include <vector>

#include "qt/config/option.h"
#include "qt/dialogs/base-dialog.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QSlider;
class QTabWidget;

namespace dialogs {

// Manages the sound configuration dialog.
class SoundConfig final : public BaseDialog {
    Q_OBJECT

public:
    static SoundConfig* NewInstance(QWidget* parent);
    ~SoundConfig() override = default;

protected:
    void OnDialogShown() override;
    void OnDialogHidden() override;
    void reject() override;

private:
    explicit SoundConfig(QWidget* parent);

    QWidget* CreateBasicTab();
    QWidget* CreateAdvancedTab();
    QWidget* CreateGameBoyTab();
    QWidget* CreateGameBoyAdvanceTab();

    // Refreshes the buffers information label.
    void OnBuffersChanged(int value);
    // Refreshes the audio device list for `api`.
    void OnAudioApiChanged(config::AudioApi api);
    void RestoreVolume();

    QTabWidget* notebook_;
    QSlider* volume_slider_;
    QSlider* buffers_slider_;
    QLabel* buffers_info_label_;
    QComboBox* audio_device_selector_;
    QCheckBox* upmix_checkbox_;
    QCheckBox* hw_accel_checkbox_;
    std::vector<std::pair<QRadioButton*, config::AudioApi>> api_radios_;

    config::AudioApi current_audio_api_;
    int volume_on_show_ = 0;
    bool volume_snapshot_taken_ = false;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_SOUND_CONFIG_H_
