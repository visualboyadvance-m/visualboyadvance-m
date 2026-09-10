#include "qt/dialogs/general-config.h"

#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "core/base/system.h"
#include "qt/config/option-proxy.h"

namespace dialogs {

namespace {
constexpr int kThrottleStep = 25;
}  // namespace

// static
GeneralConfig* GeneralConfig::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new GeneralConfig(parent);
}

GeneralConfig::GeneralConfig(QWidget* parent) : BaseDialog(parent, "GeneralConfig") {
    setWindowTitle(tr("General options"));

    auto* layout = new QVBoxLayout(this);

    // General group.
    auto* general = new QGroupBox(tr("General"), this);
    auto* general_layout = new QFormLayout(general);

    auto* format_row = new QHBoxLayout();
    auto* png = new QRadioButton(QStringLiteral("PNG"), general);
    auto* bmp = new QRadioButton(QStringLiteral("BMP"), general);
    format_row->addWidget(png);
    format_row->addWidget(bmp);
    format_row->addStretch(1);
    bindings().BindRadioButtons({png, bmp}, config::OptionID::kPrefCaptureFormat);
    general_layout->addRow(tr("Screenshot Format:"), format_row);

    auto* rewind = new QSpinBox(general);
    bindings().BindSpinBox(rewind, config::OptionID::kGenRewindInterval);
    rewind->setSuffix(tr(" s"));
    rewind->setSpecialValueText(tr("Off"));
    general_layout->addRow(tr("Rewind interval:"), rewind);
    layout->addWidget(general);

    // Throttle group.
    auto* throttle = new QGroupBox(tr("Throttle"), this);
    auto* throttle_layout = new QHBoxLayout(throttle);
    throttle_layout->addWidget(new QLabel(tr("Percent of normal:"), throttle));
    throttle_spin_ = new QSpinBox(throttle);
    bindings().BindSpinBox(throttle_spin_, config::OptionID::kPrefThrottle);
    throttle_spin_->setSpecialValueText(tr("Unlimited"));
    throttle_layout->addWidget(throttle_spin_, 1);
    throttle_sel_ = new QComboBox(throttle);
    throttle_sel_->addItem(tr("Unlimited"));
    for (int pct = kThrottleStep; pct <= static_cast<int>(kMaxThrottlePercent);
         pct += kThrottleStep) {
        throttle_sel_->addItem(tr("%1%").arg(pct));
    }
    throttle_layout->addWidget(throttle_sel_, 1);
    layout->addWidget(throttle);

    connect(throttle_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &GeneralConfig::SetThrottleSel);
    connect(throttle_sel_, QOverload<int>::of(&QComboBox::activated), this,
            &GeneralConfig::SetThrottle);

    // Emulation group.
    auto* emulation = new QGroupBox(tr("Emulation"), this);
    auto* emulation_layout = new QVBoxLayout(emulation);
    struct BoolEntry {
        QString label;
        config::OptionID id;
    };
    const BoolEntry emulation_entries[] = {
        {tr("Pause when inactive"), config::OptionID::kPrefPauseWhenInactive},
        {tr("Automatically apply IPS / UPS / IPF patches"), config::OptionID::kPrefAutoPatch},
        {tr("Auto-load the most recent save state"), config::OptionID::kGenAutoLoadLastState},
        {tr("Automatically load and save the cheat list"),
         config::OptionID::kPrefAutoSaveLoadCheatList},
        {tr("Skip the BIOS intro"), config::OptionID::kPrefSkipBios},
    };
    for (const BoolEntry& entry : emulation_entries) {
        auto* check = new QCheckBox(entry.label, emulation);
        bindings().BindCheckBox(check, entry.id);
        emulation_layout->addWidget(check);
    }
    layout->addWidget(emulation);

    // Interface group.
    auto* ui = new QGroupBox(tr("Interface"), this);
    auto* ui_layout = new QVBoxLayout(ui);
    const BoolEntry ui_entries[] = {
        {tr("Show the status bar"), config::OptionID::kGenStatusBar},
        {tr("Freeze the recent ROM list"), config::OptionID::kGenFreezeRecent},
        {tr("Allow keyboard input while in the background"),
         config::OptionID::kUIAllowKeyboardBackgroundInput},
        {tr("Allow joystick input while in the background"),
         config::OptionID::kUIAllowJoystickBackgroundInput},
        {tr("Hide the menu bar while a game is running"), config::OptionID::kUIHideMenuBar},
        {tr("Suspend the screen saver while a game is running"),
         config::OptionID::kUISuspendScreenSaver},
    };
    for (const BoolEntry& entry : ui_entries) {
        auto* check = new QCheckBox(entry.label, ui);
        bindings().BindCheckBox(check, entry.id);
        ui_layout->addWidget(check);
    }
    layout->addWidget(ui);

    layout->addWidget(CreateOkCancel());
}

void GeneralConfig::OnDialogShown() {
    SetThrottleSel(throttle_spin_->value());
}

void GeneralConfig::SetThrottleSel(int value) {
    if (syncing_) {
        return;
    }
    syncing_ = true;
    if (value >= 0 && value <= static_cast<int>(kMaxThrottlePercent)) {
        throttle_sel_->setCurrentIndex(
            static_cast<int>(std::round(static_cast<double>(value) / kThrottleStep)));
    } else {
        throttle_sel_->setCurrentIndex(100 / kThrottleStep);
    }
    syncing_ = false;
}

void GeneralConfig::SetThrottle(int selection) {
    if (syncing_) {
        return;
    }
    syncing_ = true;
    const int val = selection * kThrottleStep;
    if (val <= static_cast<int>(kMaxThrottlePercent)) {
        throttle_spin_->setValue(val);
    } else {
        throttle_spin_->setValue(100);
    }
    syncing_ = false;
}

}  // namespace dialogs
