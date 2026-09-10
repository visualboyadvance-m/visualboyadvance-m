#include "qt/dialogs/speedup-config.h"

#include <cmath>

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "qt/config/option-proxy.h"

// This dialog has a spin control representing the speedup value as it feels
// like to the user. Values below 450 can be an actual throttle or a number of
// frames to skip depending on whether the Frame Skip checkbox is checked. The
// number of frames is the spin control value divided by 100, for example 300
// with the Frame Skip checkbox checked means 3 frames to skip. Values above 450
// are always the number of frames to skip, for example 900 means 9 frames to skip.
//
// The config option SpeedupThrottle is the throttle value, if frame skip is
// used it is 0. The config option SpeedupFrameSkip is the number of frames to
// skip, if throttle is used it is 0. The config option SpeedupThrottleFrameSkip
// represents the Frame Skip checkbox for values under 450, that is if they are
// interpreted as a frame skip or a throttle.

namespace dialogs {

// static
SpeedupConfig* SpeedupConfig::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new SpeedupConfig(parent);
}

SpeedupConfig::SpeedupConfig(QWidget* parent) : BaseDialog(parent, "SpeedupConfig") {
    setWindowTitle(tr("Speedup / Turbo options"));

    auto* layout = new QVBoxLayout(this);
    auto* group = new QGroupBox(tr("Speedup Throttle"), this);
    auto* group_layout = new QVBoxLayout(group);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Percent of normal:"), group));
    speedup_throttle_spin_ = new QSpinBox(group);
    speedup_throttle_spin_->setRange(static_cast<int>(OPTION(kPrefSpeedupThrottle).Min()),
                                     static_cast<int>(OPTION(kPrefSpeedupFrameSkip).Max()) * 100);
    speedup_throttle_spin_->setKeyboardTracking(true);
    row->addWidget(speedup_throttle_spin_, 1);
    group_layout->addLayout(row);

    auto* checks = new QHBoxLayout();
    frame_skip_cb_ = new QCheckBox(tr("Frame skip"), group);
    checks->addWidget(frame_skip_cb_);
    auto* mute = new QCheckBox(tr("Mute Sound"), group);
    bindings().BindCheckBox(mute, config::OptionID::kPrefSpeedupMute);
    checks->addWidget(mute);
    group_layout->addLayout(checks);

    layout->addWidget(group);
    layout->addWidget(CreateOkCancel());

    connect(speedup_throttle_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SpeedupConfig::OnSpinValueChanged);
    connect(speedup_throttle_spin_, &QSpinBox::editingFinished, this,
            &SpeedupConfig::OnEditingFinished);
    connect(frame_skip_cb_, &QCheckBox::clicked, this, &SpeedupConfig::ToggleSpeedupFrameSkip);

    bindings().Add([this] { LoadFromOptions(); }, [this] { return SaveToOptions(); });
}

void SpeedupConfig::LoadFromOptions() {
    const uint32_t opt_frame_skip = OPTION(kPrefSpeedupFrameSkip);
    const uint32_t opt_throttle = OPTION(kPrefSpeedupThrottle);
    const bool opt_throttle_frame_skip = OPTION(kPrefSpeedupThrottleFrameSkip);

    loading_ = true;
    prev_frame_skip_cb_ = opt_throttle_frame_skip;

    if (opt_frame_skip != 0) {
        speedup_throttle_spin_->setValue(static_cast<int>(opt_frame_skip * 100));
        prev_throttle_spin_ = opt_frame_skip * 100;

        frame_skip_cb_->setChecked(true);
        frame_skip_cb_->setEnabled(false);
    } else {
        speedup_throttle_spin_->setValue(static_cast<int>(opt_throttle));
        prev_throttle_spin_ = opt_throttle;

        frame_skip_cb_->setChecked(opt_throttle_frame_skip);
        frame_skip_cb_->setEnabled(opt_throttle != 0);
    }
    loading_ = false;
}

bool SpeedupConfig::SaveToOptions() {
    speedup_throttle_spin_->interpretText();
    const uint32_t val = static_cast<uint32_t>(std::max(0, speedup_throttle_spin_->value()));

    if (val == 0) {
        OPTION(kPrefSpeedupThrottle) = 0;
        OPTION(kPrefSpeedupFrameSkip) = 0;
        OPTION(kPrefSpeedupThrottleFrameSkip) = false;
    } else if (val <= OPTION(kPrefSpeedupThrottle).Max()) {
        OPTION(kPrefSpeedupThrottle) = val;
        OPTION(kPrefSpeedupFrameSkip) = 0;
        OPTION(kPrefSpeedupThrottleFrameSkip) = frame_skip_cb_->isChecked();
    } else {  // val > throttle_max
        OPTION(kPrefSpeedupThrottle) = 100;
        OPTION(kPrefSpeedupFrameSkip) = val / 100;
        OPTION(kPrefSpeedupThrottleFrameSkip) = false;
    }
    return true;
}

void SpeedupConfig::OnSpinValueChanged(int value) {
    if (loading_) {
        return;
    }
    uint32_t val = static_cast<uint32_t>(std::max(0, value));
    const uint32_t original_val = val;

    // Update checkbox state based on current value.
    if (val == 0) {
        frame_skip_cb_->setChecked(false);
        frame_skip_cb_->setEnabled(false);
    } else if (val <= OPTION(kPrefSpeedupThrottle).Max()) {
        frame_skip_cb_->setChecked(prev_frame_skip_cb_);
        frame_skip_cb_->setEnabled(true);
    } else {  // val > throttle_max
        frame_skip_cb_->setChecked(true);
        frame_skip_cb_->setEnabled(false);

        // Only snap to a multiple of 100 for stepping (arrows / wheel), not
        // while typing: the spin box has keyboard tracking, so text edits
        // arrive here too, and snapping them would interrupt typing.
        if (!speedup_throttle_spin_->hasFocus() ||
            std::abs(static_cast<int>(val) - static_cast<int>(prev_throttle_spin_)) == 1) {
            if (val > prev_throttle_spin_) {
                val += 100;
            }
            val = static_cast<uint32_t>(std::floor(static_cast<double>(val) / 100) * 100);
            const uint32_t max = static_cast<uint32_t>(speedup_throttle_spin_->maximum());
            if (val > max) {
                val = max;
            }
        }
    }

    if (val != original_val) {
        speedup_throttle_spin_->setValue(static_cast<int>(val));
    }
    prev_throttle_spin_ = val;
}

void SpeedupConfig::OnEditingFinished() {
    // Force apply value adjustments when editing is done.
    uint32_t val = static_cast<uint32_t>(std::max(0, speedup_throttle_spin_->value()));
    const uint32_t original_val = val;

    if (val == 0) {
        frame_skip_cb_->setChecked(false);
        frame_skip_cb_->setEnabled(false);
    } else if (val <= OPTION(kPrefSpeedupThrottle).Max()) {
        frame_skip_cb_->setChecked(prev_frame_skip_cb_);
        frame_skip_cb_->setEnabled(true);
    } else {  // val > throttle_max
        val = static_cast<uint32_t>(std::floor(static_cast<double>(val) / 100) * 100);
        const uint32_t max = static_cast<uint32_t>(speedup_throttle_spin_->maximum());
        if (val > max) {
            val = max;
        }
        frame_skip_cb_->setChecked(true);
        frame_skip_cb_->setEnabled(false);

        if (val != original_val) {
            speedup_throttle_spin_->setValue(static_cast<int>(val));
            prev_throttle_spin_ = val;
        }
    }
}

void SpeedupConfig::ToggleSpeedupFrameSkip() {
    prev_frame_skip_cb_ = frame_skip_cb_->isChecked();
}

}  // namespace dialogs
