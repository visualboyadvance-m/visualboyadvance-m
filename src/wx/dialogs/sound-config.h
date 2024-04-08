#ifndef VBAM_WX_DIALOGS_SOUND_CONFIG_H_
#define VBAM_WX_DIALOGS_SOUND_CONFIG_H_

#include "wx/config/option.h"
#include "wx/dialogs/base-dialog.h"

// Forward declarations.
class wxChoice;
class wxCheckBox;
class wxControl;
class wxSlider;
class wxWindow;

namespace dialogs {

// Manages the sound configuration dialog.
class SoundConfig : public BaseDialog {
public:
    static SoundConfig* NewInstance(wxWindow* parent);
    ~SoundConfig() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    SoundConfig(wxWindow* parent);

    void OnShow(wxShowEvent& event);

    // Populate `buffers_info_label_` with the current buffer information.
    void OnBuffersChanged(wxCommandEvent& event);

    // Refresh `audio_device_selector_`.
    void OnAudioApiChanged(wxCommandEvent& event, config::AudioApi api);

    wxSlider* buffers_slider_;
    wxControl* buffers_info_label_;
    wxChoice* audio_device_selector_;
    wxCheckBox* upmix_checkbox_;
    wxCheckBox* hw_accel_checkbox_;
    config::AudioApi current_audio_api_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_SOUND_CONFIG_H_
