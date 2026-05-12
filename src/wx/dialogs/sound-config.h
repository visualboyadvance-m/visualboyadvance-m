#ifndef VBAM_WX_DIALOGS_SOUND_CONFIG_H_
#define VBAM_WX_DIALOGS_SOUND_CONFIG_H_

#include <vector>

#include "wx/config/option.h"
#include "wx/dialogs/base-dialog.h"

// Forward declarations.
class wxChoice;
class wxCheckBox;
class wxControl;
class wxNotebook;
class wxSlider;
class wxWindow;

namespace dialogs {

// Manages the sound configuration dialog.
class SoundConfig : public BaseDialog {
public:
    static SoundConfig* NewInstance(wxWindow* parent);
    ~SoundConfig() override = default;

    int LazyTabCount() const override { return kTabCount; }
    bool LoadLazyTab(int index) override;

private:
    enum Tab {
        kTabBasic = 0,
        kTabAdvanced,
        kTabGameBoy,
        kTabGameBoyAdvance,
        kTabCount,
    };

    SoundConfig(wxWindow* parent);

    void InitBasicTab();
    void InitAdvancedTab();
    void InitGameBoyTab();
    void InitGameBoyAdvanceTab();

    void OnShow(wxShowEvent& event);

    // Populate `buffers_info_label_` with the current buffer information.
    void OnBuffersChanged(wxCommandEvent& event);

    // Refresh `audio_device_selector_`.
    void OnAudioApiChanged(wxCommandEvent& event, config::AudioApi api);

    wxNotebook* notebook_ = nullptr;
    std::vector<bool> tab_loaded_;

    wxSlider* buffers_slider_ = nullptr;
    wxControl* buffers_info_label_ = nullptr;
    wxChoice* audio_device_selector_ = nullptr;
    wxCheckBox* upmix_checkbox_ = nullptr;
    wxCheckBox* hw_accel_checkbox_ = nullptr;
    config::AudioApi current_audio_api_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_SOUND_CONFIG_H_
