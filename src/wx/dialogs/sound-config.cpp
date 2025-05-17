#include "wx/dialogs/sound-config.h"

#include <wx/arrstr.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clntdata.h>
#include <wx/event.h>
#include <wx/radiobut.h>
#include <wx/slider.h>

#include <wx/string.h>
#include <wx/xrc/xmlres.h>
#include <functional>

#include "core/base/check.h"
#include "wx/audio/audio.h"
#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/dialogs/base-dialog.h"
#include "wx/widgets/option-validator.h"

namespace dialogs {

namespace {

// Validator for the sound rate choice.
class SoundRateValidator : public widgets::OptionValidator {
public:
    SoundRateValidator() : widgets::OptionValidator(config::OptionID::kSoundAudioRate) {}
    ~SoundRateValidator() override = default;

private:
    // widgets::OptionValidator implementation.
    wxObject* Clone() const override { return new SoundRateValidator(); }

    bool IsWindowValueValid() override { return true; }

    bool WriteToWindow() override {
        wxChoice* choice = wxDynamicCast(GetWindow(), wxChoice);
        VBAM_CHECK(choice);
        choice->SetSelection(static_cast<int>(option()->GetAudioRate()));
        return true;
    }

    bool WriteToOption() override {
        const wxChoice* choice = wxDynamicCast(GetWindow(), wxChoice);
        VBAM_CHECK(choice);
        const int selection = choice->GetSelection();
        if (selection == wxNOT_FOUND) {
            return false;
        }

        if (static_cast<size_t>(selection) >= option()->GetEnumMax()) {
            return false;
        }

        return option()->SetAudioRate(static_cast<config::AudioRate>(choice->GetSelection()));
    }
};

// Validator for a wxRadioButton with an AudioApi value.
class AudioApiValidator : public widgets::OptionValidator {
public:
    explicit AudioApiValidator(config::AudioApi audio_api)
        : OptionValidator(config::OptionID::kSoundAudioAPI), audio_api_(audio_api) {
        VBAM_CHECK(audio_api < config::AudioApi::kLast);
    }
    ~AudioApiValidator() override = default;

private:
    // OptionValidator implementation.
    wxObject* Clone() const override { return new AudioApiValidator(audio_api_); }

    bool IsWindowValueValid() override { return true; }

    bool WriteToWindow() override {
        wxDynamicCast(GetWindow(), wxRadioButton)->SetValue(option()->GetAudioApi() == audio_api_);
        return true;
    }

    bool WriteToOption() override {
        if (wxDynamicCast(GetWindow(), wxRadioButton)->GetValue()) {
            return option()->SetAudioApi(audio_api_);
        }

        return true;
    }

    const config::AudioApi audio_api_;
};

// Validator for the audio device wxChoice.
class AudioDeviceValidator : public widgets::OptionValidator {
public:
    AudioDeviceValidator() : widgets::OptionValidator(config::OptionID::kSoundAudioDevice) {}
    ~AudioDeviceValidator() override = default;

private:
    // OptionValidator implementation.
    wxObject* Clone() const override { return new AudioDeviceValidator(); }

    bool IsWindowValueValid() override { return true; }

    bool WriteToWindow() override {
        wxChoice* choice = wxDynamicCast(GetWindow(), wxChoice);
        VBAM_CHECK(choice);

        const wxString& device_id = option()->GetString();
        for (size_t i = 0; i < choice->GetCount(); i++) {
            const wxString& choide_id =
                dynamic_cast<wxStringClientData*>(choice->GetClientObject(i))->GetData();
            if (device_id == choide_id) {
                choice->SetSelection(i);
                return true;
            }
        }

        choice->SetSelection(0);
        return true;
    }

    bool WriteToOption() override {
        const wxChoice* choice = wxDynamicCast(GetWindow(), wxChoice);
        VBAM_CHECK(choice);
        const int selection = choice->GetSelection();
        if (selection == wxNOT_FOUND) {
            return option()->SetString(wxEmptyString);
        }

        return option()->SetString(
            dynamic_cast<wxStringClientData*>(choice->GetClientObject(selection))->GetData());
    }
};

}  // namespace

// static
SoundConfig* SoundConfig::NewInstance(wxWindow* parent) {
    VBAM_CHECK(parent);
    return new SoundConfig(parent);
}

SoundConfig::SoundConfig(wxWindow* parent) : BaseDialog(parent, "SoundConfig") {
    // Volume slider configuration.
    wxSlider* volume_slider = GetValidatedChild<wxSlider>("Volume");
    volume_slider->SetValidator(widgets::OptionIntValidator(config::OptionID::kSoundVolume));
    GetValidatedChild("Volume100")
        ->Bind(wxEVT_BUTTON, std::bind(&wxSlider::SetValue, volume_slider, 100));

    // Sound quality.
    GetValidatedChild("Rate")->SetValidator(SoundRateValidator());

    // Audio API selection.
    wxWindow* audio_api_button = GetValidatedChild("OpenAL");
    audio_api_button->SetValidator(AudioApiValidator(config::AudioApi::kOpenAL));
    audio_api_button->Bind(wxEVT_RADIOBUTTON,
                           std::bind(&SoundConfig::OnAudioApiChanged, this, std::placeholders::_1,
                                     config::AudioApi::kOpenAL));

    audio_api_button = GetValidatedChild("SDL");
    audio_api_button->SetValidator(AudioApiValidator(config::AudioApi::kSDL));
    audio_api_button->Bind(wxEVT_RADIOBUTTON,
                           std::bind(&SoundConfig::OnAudioApiChanged, this, std::placeholders::_1,
                                     config::AudioApi::kSDL));

    audio_api_button = GetValidatedChild("DirectSound");
#if defined(__WXMSW__)
    audio_api_button->SetValidator(AudioApiValidator(config::AudioApi::kDirectSound));
    audio_api_button->Bind(wxEVT_RADIOBUTTON,
                           std::bind(&SoundConfig::OnAudioApiChanged, this, std::placeholders::_1,
                                     config::AudioApi::kDirectSound));
#else
    audio_api_button->Hide();
#endif

    audio_api_button = GetValidatedChild("XAudio2");
#if defined(VBAM_ENABLE_XAUDIO2)
    audio_api_button->SetValidator(AudioApiValidator(config::AudioApi::kXAudio2));
    audio_api_button->Bind(wxEVT_RADIOBUTTON,
                           std::bind(&SoundConfig::OnAudioApiChanged, this, std::placeholders::_1,
                                     config::AudioApi::kXAudio2));
#else
    audio_api_button->Hide();
#endif

    audio_api_button = GetValidatedChild("FAudio");
#if defined(VBAM_ENABLE_FAUDIO)
    audio_api_button->SetValidator(AudioApiValidator(config::AudioApi::kFAudio));
    audio_api_button->Bind(wxEVT_RADIOBUTTON,
                           std::bind(&SoundConfig::OnAudioApiChanged, this, std::placeholders::_1,
                                     config::AudioApi::kFAudio));
#else
    audio_api_button->Hide();
#endif

    audio_api_button = GetValidatedChild("CoreAudio");
#if defined(__WXMAC__)
    audio_api_button->SetValidator(AudioApiValidator(config::AudioApi::kCoreAudio));
    audio_api_button->Bind(wxEVT_RADIOBUTTON,
                           std::bind(&SoundConfig::OnAudioApiChanged, this, std::placeholders::_1,
                                     config::AudioApi::kCoreAudio));
#else
    audio_api_button->Hide();
#endif

    // Upmix configuration.
    upmix_checkbox_ = GetValidatedChild<wxCheckBox>("Upmix");
#if defined(VBAM_ENABLE_XAUDIO2) || defined(VBAM_ENABLE_FAUDIO)
    upmix_checkbox_->SetValidator(widgets::OptionBoolValidator(config::OptionID::kSoundUpmix));
#else
    upmix_checkbox_->Hide();
#endif

    // DSound HW acceleration.
    hw_accel_checkbox_ = GetValidatedChild<wxCheckBox>("HWAccel");
#if defined(__WXMSW__)
    hw_accel_checkbox_->SetValidator(
        widgets::OptionBoolValidator(config::OptionID::kSoundDSoundHWAccel));
#else
    hw_accel_checkbox_->Hide();
#endif

    // Buffers configuration.
    buffers_info_label_ = GetValidatedChild<wxControl>("BuffersInfo");
    buffers_slider_ = GetValidatedChild<wxSlider>("Buffers");
    buffers_slider_->SetValidator(widgets::OptionIntValidator(config::OptionID::kSoundBuffers));
    buffers_slider_->Bind(wxEVT_SLIDER, &SoundConfig::OnBuffersChanged, this);

    // Game Boy configuration.
    GetValidatedChild("GBEcho")->SetValidator(
        widgets::OptionIntValidator(config::OptionID::kSoundGBEcho));
    GetValidatedChild("GBStereo")
        ->SetValidator(widgets::OptionIntValidator(config::OptionID::kSoundGBStereo));

    // Game Boy Advance configuration.
    GetValidatedChild("GBASoundFiltering")
        ->SetValidator(widgets::OptionIntValidator(config::OptionID::kSoundGBAFiltering));

    // Audio Device configuration.
    audio_device_selector_ = GetValidatedChild<wxChoice>("Device");
    audio_device_selector_->SetValidator(AudioDeviceValidator());

    this->Bind(wxEVT_SHOW, &SoundConfig::OnShow, this);

    // Finally, fit everything nicely.
    Fit();
}

void SoundConfig::OnShow(wxShowEvent& event) {
    wxCommandEvent dummy_event;

    // Referesh the buffers information.
    OnBuffersChanged(dummy_event);

    // Refresh the audio device selector.
    OnAudioApiChanged(dummy_event, OPTION(kSoundAudioAPI));

    // Let the event propagate.
    event.Skip();
}

void SoundConfig::OnBuffersChanged(wxCommandEvent& event) {
    const int buffers_count = buffers_slider_->GetValue();
    const double buffer_time = static_cast<double>(buffers_count) / 60.0 * 1000.0;
    buffers_info_label_->SetLabel(
        wxString::Format(_("%d frame = %.2f ms"), buffers_count, buffer_time));

    // Let the event propagate.
    event.Skip();
}

void SoundConfig::OnAudioApiChanged(wxCommandEvent& event, config::AudioApi audio_api) {
    audio_device_selector_->Clear();

    bool audio_device_found = false;
    for (const auto& device : audio::EnumerateAudioDevices(audio_api)) {
        const int i =
            audio_device_selector_->Append(device.name, new wxStringClientData(device.id));

        if (!audio_device_found && audio_api == OPTION(kSoundAudioAPI) &&
            OPTION(kSoundAudioDevice) == device.id) {
            audio_device_selector_->SetSelection(i);
            audio_device_found = true;
        }
    }

    if (!audio_device_found) {
        audio_device_selector_->SetSelection(0);
    }

#if defined(VBAM_ENABLE_XAUDIO2) && defined(VBAM_ENABLE_FAUDIO)
    upmix_checkbox_->Enable(audio_api == config::AudioApi::kXAudio2 ||
                            audio_api == config::AudioApi::kFAudio);
#elif defined(VBAM_ENABLE_XAUDIO2)
    upmix_checkbox_->Enable(audio_api == config::AudioApi::kXAudio2);
#elif defined(VBAM_ENABLE_FAUDIO)
    upmix_checkbox_->Enable(audio_api == config::AudioApi::kFAudio);
#else
    upmix_checkbox_->Enable(false);
#endif

#if defined(__WXMSW__)
    hw_accel_checkbox_->Enable(audio_api == config::AudioApi::kDirectSound);
#else
    hw_accel_checkbox_->Enable(false);
#endif

    current_audio_api_ = audio_api;

    // Let the event propagate.
    event.Skip();
}

}  // namespace dialogs
