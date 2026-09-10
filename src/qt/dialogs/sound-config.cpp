#include "qt/dialogs/sound-config.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "qt/audio/audio.h"
#include "qt/config/option-proxy.h"
#include "qt/widgets/option-binding.h"

namespace dialogs {

namespace {

// A horizontal slider with "min / max" captions under it.
QWidget* CaptionedSlider(QWidget* parent, QSlider* slider, const QString& left,
                         const QString& right, const QString& suffix,
                         config::OptionID id) {
    auto* container = new QWidget(parent);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widgets::WrapSliderWithValueLabel(
        slider, suffix, [slider, id] { widgets::ResetSliderToOptionDefault(slider, id); }));
    auto* captions = new QHBoxLayout();
    captions->addWidget(new QLabel(left, container));
    captions->addStretch(1);
    captions->addWidget(new QLabel(right, container));
    layout->addLayout(captions);
    return container;
}

}  // namespace

// static
SoundConfig* SoundConfig::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new SoundConfig(parent);
}

SoundConfig::SoundConfig(QWidget* parent)
    : BaseDialog(parent, "SoundConfig"), current_audio_api_(OPTION(kSoundAudioAPI)) {
    setWindowTitle(tr("Sound options"));

    auto* layout = new QVBoxLayout(this);
    notebook_ = new QTabWidget(this);
    notebook_->addTab(CreateBasicTab(), tr("Basic"));
    notebook_->addTab(CreateAdvancedTab(), tr("Advanced"));
    notebook_->addTab(CreateGameBoyTab(), tr("Game Boy"));
    notebook_->addTab(CreateGameBoyAdvanceTab(), tr("Game Boy Advance"));
    layout->addWidget(notebook_);
    layout->addWidget(CreateOkCancel());
}

QWidget* SoundConfig::CreateBasicTab() {
    auto* page = new QWidget(notebook_);
    auto* form = new QFormLayout(page);

    volume_slider_ = new QSlider(Qt::Horizontal, page);
    bindings().BindSlider(volume_slider_, config::OptionID::kSoundVolume);
    form->addRow(tr("Volume:"),
                 CaptionedSlider(page, volume_slider_, tr("Mute"), tr("Maximum"), QString(),
                                 config::OptionID::kSoundVolume));
    // Apply the volume once the drag settles rather than on OK, so it can be
    // set by ear. GameArea's audio_volume_observer_ pushes the option to the
    // backend. Cancel restores the snapshot taken on show.
    connect(volume_slider_, &QSlider::sliderReleased, this,
            [this] { OPTION(kSoundVolume) = volume_slider_->value(); });
    connect(volume_slider_, &QSlider::valueChanged, this, [this](int value) {
        if (!volume_slider_->isSliderDown()) {
            OPTION(kSoundVolume) = value;
        }
    });

    auto* rate = new QComboBox(page);
    bindings().BindComboBoxEnum(rate, config::OptionID::kSoundAudioRate);
    form->addRow(tr("Sample rate:"), rate);
    return page;
}

QWidget* SoundConfig::CreateAdvancedTab() {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);

    auto* api_group = new QGroupBox(tr("Audio API"), page);
    auto* api_layout = new QVBoxLayout(api_group);
    auto* radios = new QHBoxLayout();

    const QStringList labels = widgets::AudioApiLabels();
    for (size_t i = 0; i < config::kNbAudioApis; i++) {
        const auto api = static_cast<config::AudioApi>(i);
        auto* radio = new QRadioButton(labels.value(static_cast<int>(i)), api_group);
        bindings().BindButtonSelected(radio, config::OptionID::kSoundAudioAPI,
                                      static_cast<int>(i));
        connect(radio, &QRadioButton::toggled, this, [this, api](bool checked) {
            if (checked) {
                OnAudioApiChanged(api);
            }
        });
        radios->addWidget(radio);
        api_radios_.push_back({radio, api});
    }
    api_layout->addLayout(radios);

    auto* device_row = new QHBoxLayout();
    device_row->addWidget(new QLabel(tr("Device"), api_group));
    audio_device_selector_ = new QComboBox(api_group);
    bindings().BindComboBoxString(audio_device_selector_, config::OptionID::kSoundAudioDevice);
    device_row->addWidget(audio_device_selector_, 1);
    api_layout->addLayout(device_row);

    upmix_checkbox_ = new QCheckBox(tr("Enable stereo upmixing"), api_group);
#if defined(VBAM_ENABLE_XAUDIO2) || defined(VBAM_ENABLE_FAUDIO)
    bindings().BindCheckBox(upmix_checkbox_, config::OptionID::kSoundUpmix);
#else
    upmix_checkbox_->hide();
#endif
    api_layout->addWidget(upmix_checkbox_);

    hw_accel_checkbox_ = new QCheckBox(tr("Enable hardware acceleration"), api_group);
#if defined(_WIN32)
    bindings().BindCheckBox(hw_accel_checkbox_, config::OptionID::kSoundDSoundHWAccel);
#else
    hw_accel_checkbox_->hide();
#endif
    api_layout->addWidget(hw_accel_checkbox_);
    layout->addWidget(api_group);

    auto* buffers_group = new QGroupBox(tr("Number of sound buffers:"), page);
    auto* buffers_layout = new QVBoxLayout(buffers_group);
    buffers_slider_ = new QSlider(Qt::Horizontal, buffers_group);
    bindings().BindSlider(buffers_slider_, config::OptionID::kSoundBuffers);
    buffers_layout->addWidget(widgets::WrapSliderWithValueLabel(buffers_slider_, QString(), [this] {
        widgets::ResetSliderToOptionDefault(buffers_slider_, config::OptionID::kSoundBuffers);
    }));
    buffers_info_label_ = new QLabel(buffers_group);
    buffers_layout->addWidget(buffers_info_label_);
    connect(buffers_slider_, &QSlider::valueChanged, this, &SoundConfig::OnBuffersChanged);
    layout->addWidget(buffers_group);
    layout->addStretch(1);
    return page;
}

QWidget* SoundConfig::CreateGameBoyTab() {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);

    auto* enhance = new QCheckBox(tr("Enable sound effects (echo, stereo)"), page);
    bindings().BindCheckBox(enhance, config::OptionID::kSoundGBEnableEffects);
    layout->addWidget(enhance);

    auto* effects = new QWidget(page);
    auto* effects_layout = new QVBoxLayout(effects);
    effects_layout->setContentsMargins(0, 0, 0, 0);

    auto* echo_group = new QGroupBox(tr("Echo"), effects);
    auto* echo_layout = new QVBoxLayout(echo_group);
    auto* echo = new QSlider(Qt::Horizontal, echo_group);
    bindings().BindSlider(echo, config::OptionID::kSoundGBEcho);
    echo_layout->addWidget(CaptionedSlider(echo_group, echo, tr("None"), tr("Lots"), QString(),
                                           config::OptionID::kSoundGBEcho));
    effects_layout->addWidget(echo_group);

    auto* stereo_group = new QGroupBox(tr("Stereo"), effects);
    auto* stereo_layout = new QVBoxLayout(stereo_group);
    auto* stereo = new QSlider(Qt::Horizontal, stereo_group);
    bindings().BindSlider(stereo, config::OptionID::kSoundGBStereo);
    stereo_layout->addWidget(CaptionedSlider(stereo_group, stereo, tr("Center"),
                                             tr("Left / Right"), QString(),
                                             config::OptionID::kSoundGBStereo));
    effects_layout->addWidget(stereo_group);
    layout->addWidget(effects);
    connect(enhance, &QCheckBox::toggled, effects, &QWidget::setEnabled);
    effects->setEnabled(enhance->isChecked());
    bindings().Add([enhance, effects] { effects->setEnabled(enhance->isChecked()); },
                   [] { return true; });

    auto* surround = new QCheckBox(tr("Surround"), page);
    bindings().BindCheckBox(surround, config::OptionID::kSoundGBSurround);
    layout->addWidget(surround);

    auto* declick = new QCheckBox(tr("Declicking"), page);
    bindings().BindCheckBox(declick, config::OptionID::kSoundGBDeclicking);
    layout->addWidget(declick);
    layout->addStretch(1);
    return page;
}

QWidget* SoundConfig::CreateGameBoyAdvanceTab() {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);

    auto* filtering_group = new QGroupBox(tr("Sound filtering"), page);
    auto* filtering_layout = new QVBoxLayout(filtering_group);
    auto* filtering = new QSlider(Qt::Horizontal, filtering_group);
    bindings().BindSlider(filtering, config::OptionID::kSoundGBAFiltering);
    filtering_layout->addWidget(CaptionedSlider(filtering_group, filtering, tr("None"),
                                                tr("Maximum"), QString(),
                                                config::OptionID::kSoundGBAFiltering));
    layout->addWidget(filtering_group);

    auto* interpolation = new QCheckBox(tr("Sound interpolation"), page);
    bindings().BindCheckBox(interpolation, config::OptionID::kSoundGBAInterpolation);
    layout->addWidget(interpolation);
    layout->addStretch(1);
    return page;
}

void SoundConfig::OnDialogShown() {
    // Retake every time: the dialog instance is cached and reused.
    volume_on_show_ = OPTION(kSoundVolume);
    volume_snapshot_taken_ = true;

    OnBuffersChanged(buffers_slider_->value());
    OnAudioApiChanged(OPTION(kSoundAudioAPI));
}

void SoundConfig::OnDialogHidden() {
    volume_snapshot_taken_ = false;
}

void SoundConfig::reject() {
    RestoreVolume();
    BaseDialog::reject();
}

void SoundConfig::RestoreVolume() {
    if (volume_snapshot_taken_) {
        OPTION(kSoundVolume) = volume_on_show_;
    }
}

void SoundConfig::OnBuffersChanged(int buffers_count) {
    const double buffer_time = static_cast<double>(buffers_count) / 60.0 * 1000.0;
    buffers_info_label_->setText(
        tr("%1 frame = %2 ms").arg(buffers_count).arg(buffer_time, 0, 'f', 2));
}

void SoundConfig::OnAudioApiChanged(config::AudioApi audio_api) {
    audio_device_selector_->clear();

    bool audio_device_found = false;
    for (const auto& device : audio::EnumerateAudioDevices(audio_api)) {
        audio_device_selector_->addItem(device.name, device.id);
        if (!audio_device_found && audio_api == OPTION(kSoundAudioAPI) &&
            OPTION(kSoundAudioDevice).Get() == device.id) {
            audio_device_selector_->setCurrentIndex(audio_device_selector_->count() - 1);
            audio_device_found = true;
        }
    }
    if (!audio_device_found && audio_device_selector_->count() > 0) {
        audio_device_selector_->setCurrentIndex(0);
    }

    bool upmix = false;
#if defined(VBAM_ENABLE_XAUDIO2)
    upmix = upmix || audio_api == config::AudioApi::kXAudio2;
#endif
#if defined(VBAM_ENABLE_FAUDIO)
    upmix = upmix || audio_api == config::AudioApi::kFAudio;
#endif
    upmix_checkbox_->setEnabled(upmix);

#if defined(_WIN32)
    hw_accel_checkbox_->setEnabled(audio_api == config::AudioApi::kDirectSound);
#else
    hw_accel_checkbox_->setEnabled(false);
#endif

    current_audio_api_ = audio_api;
}

}  // namespace dialogs
