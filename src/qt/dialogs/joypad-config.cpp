#include "qt/dialogs/joypad-config.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "qt/config/option-proxy.h"
#include "qt/opts.h"
#include "qt/widgets/user-input-ctrl.h"

namespace dialogs {

namespace {

const std::vector<config::GameKey> kStandardKeys = {
    config::GameKey::Up,    config::GameKey::A,     config::GameKey::Down,
    config::GameKey::B,     config::GameKey::Left,  config::GameKey::L,
    config::GameKey::Right, config::GameKey::R,     config::GameKey::Select,
    config::GameKey::Start,
};

const std::vector<config::GameKey> kSpecialKeys = {
    config::GameKey::MotionUp,    config::GameKey::AutoA,
    config::GameKey::MotionDown,  config::GameKey::AutoB,
    config::GameKey::MotionLeft,  config::GameKey::Gameshark,
    config::GameKey::MotionRight, config::GameKey::Speed,
    config::GameKey::MotionIn,    config::GameKey::Capture,
    config::GameKey::MotionOut,
};

QString GameKeyLabel(const config::GameKey& key) {
    switch (key) {
        case config::GameKey::Up:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Up");
        case config::GameKey::Down:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Down");
        case config::GameKey::Left:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Left");
        case config::GameKey::Right:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Right");
        case config::GameKey::A:
            return QStringLiteral("A");
        case config::GameKey::B:
            return QStringLiteral("B");
        case config::GameKey::L:
            return QStringLiteral("L");
        case config::GameKey::R:
            return QStringLiteral("R");
        case config::GameKey::Select:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Select");
        case config::GameKey::Start:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Start");
        case config::GameKey::MotionUp:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Motion Up");
        case config::GameKey::MotionDown:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Motion Down");
        case config::GameKey::MotionLeft:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Motion Left / Dark");
        case config::GameKey::MotionRight:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Motion Right / Light");
        case config::GameKey::MotionIn:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Spin Left");
        case config::GameKey::MotionOut:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Spin Right");
        case config::GameKey::AutoA:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Autofire A");
        case config::GameKey::AutoB:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Autofire B");
        case config::GameKey::Speed:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Speed Up");
        case config::GameKey::Capture:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Screenshot");
        case config::GameKey::Gameshark:
            return QCoreApplication::translate("dialogs::JoypadConfig", "Gameshark");
    }
    VBAM_NOTREACHED_RETURN(QString());
}

}  // namespace

// static
JoypadConfig* JoypadConfig::NewInstance(QWidget* parent,
                                        const config::BindingsProvider bindings_provider) {
    VBAM_CHECK(parent);
    VBAM_CHECK(bindings_provider);
    return new JoypadConfig(parent, bindings_provider);
}

JoypadConfig::JoypadConfig(QWidget* parent, const config::BindingsProvider bindings_provider)
    : BaseDialog(parent, "JoypadConfig"), bindings_provider_(bindings_provider) {
    setWindowTitle(tr("Joypad options"));

    auto* layout = new QVBoxLayout(this);

    game_controller_mode_ = new QCheckBox(tr("SDL GameController Mode"), this);
    game_controller_mode_->setChecked(OPTION(kSDLGameControllerMode));
    connect(game_controller_mode_, &QCheckBox::clicked, this,
            &JoypadConfig::ToggleSDLGameControllerMode);
    layout->addWidget(game_controller_mode_);

    auto* autofire_row = new QHBoxLayout();
    autofire_row->addWidget(new QLabel(tr("Autofire Throttle (frames):"), this));
    autofire_throttle_ = new QSpinBox(this);
    bindings().BindSpinBox(autofire_throttle_, config::OptionID::kJoyAutofireThrottle);
    autofire_row->addWidget(autofire_throttle_);
    autofire_row->addStretch(1);
    layout->addLayout(autofire_row);

    notebook_ = new QTabWidget(this);
    for (const config::GameJoy& joypad : config::kAllGameJoys) {
        notebook_->addTab(CreatePlayerTab(joypad), tr("Player %1").arg(joypad.ux_index()));
    }
    layout->addWidget(notebook_);
    layout->addWidget(CreateOkCancel());
}

QWidget* JoypadConfig::CreatePlayerTab(const config::GameJoy& joypad) {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);

    auto* inner = new QTabWidget(page);
    inner->addTab(CreateSubTab(inner, joypad, kStandardKeys), tr("Standard"));
    inner->addTab(CreateSubTab(inner, joypad, kSpecialKeys), tr("Special"));
    layout->addWidget(inner);

    auto* bottom = new QHBoxLayout();
    auto* use_default = new QCheckBox(tr("Use as default"), page);
    default_checks_[joypad.index()] = use_default;
    bindings().BindButtonSelected(use_default, config::OptionID::kJoyDefault,
                                  static_cast<int>(joypad.ux_index()));
    connect(use_default, &QCheckBox::toggled, this,
            [this, joypad](bool checked) { OnDefaultToggled(joypad, checked); });
    bottom->addWidget(use_default);
    bottom->addStretch(1);

    auto* defaults = new QPushButton(tr("Defaults"), page);
    defaults->setAutoDefault(false);
    connect(defaults, &QPushButton::clicked, this, [this, joypad] { ResetToDefaults(joypad); });
    bottom->addWidget(defaults);

    auto* clear = new QPushButton(tr("Clear All"), page);
    clear->setAutoDefault(false);
    connect(clear, &QPushButton::clicked, this, [this, joypad] { ClearJoypad(joypad); });
    bottom->addWidget(clear);
    layout->addLayout(bottom);
    return page;
}

QWidget* JoypadConfig::CreateSubTab(QWidget* parent, const config::GameJoy& joypad,
                                    const std::vector<config::GameKey>& keys) {
    auto* page = new QWidget(parent);
    auto* grid = new QGridLayout(page);

    // Two columns of (label, control, clear) like the wx layout.
    int index = 0;
    for (const config::GameKey& key : keys) {
        const int row = index / 2;
        const int col = (index % 2) * 3;
        auto* control = new widgets::UserInputCtrl(page);
        control->SetMultiKey(true);
        controls_.emplace(config::GameCommand(joypad, key), control);

        auto* clear = new QPushButton(tr("Clear"), page);
        clear->setAutoDefault(false);
        connect(clear, &QPushButton::clicked, control, &widgets::UserInputCtrl::Clear);

        grid->addWidget(new QLabel(GameKeyLabel(key), page), row, col);
        grid->addWidget(control, row, col + 1);
        grid->addWidget(clear, row, col + 2);
        index++;
    }
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(4, 1);
    return page;
}

void JoypadConfig::OnDialogShown() {
    bindings_ = bindings_provider_()->Clone();
    game_controller_mode_->setChecked(OPTION(kSDLGameControllerMode));
    LoadControls();
}

void JoypadConfig::LoadControls() {
    loading_ = true;
    for (auto& entry : controls_) {
        entry.second->SetInputs(bindings_.InputsForCommand(config::Command(entry.first)));
    }
    loading_ = false;
}

bool JoypadConfig::OnAccept() {
    for (const auto& entry : controls_) {
        const config::Command command(entry.first);
        bindings_.ClearCommandAssignments(command);
        for (const auto& input : entry.second->inputs()) {
            bindings_.AssignInputToCommand(input, command);
        }
    }
    *bindings_provider_() = std::move(bindings_);
    bindings_ = config::Bindings();
    update_joypad_opts();
    return true;
}

void JoypadConfig::ResetToDefaults(const config::GameJoy& joypad) {
    for (const config::GameKey& game_key : config::kAllGameKeys) {
        const config::GameCommand command(joypad, game_key);
        auto iter = controls_.find(command);
        if (iter != controls_.end()) {
            iter->second->SetInputs(
                config::Bindings::DefaultInputsForCommand(config::Command(command)));
        }
    }
}

void JoypadConfig::ClearJoypad(const config::GameJoy& joypad) {
    for (const config::GameKey& game_key : config::kAllGameKeys) {
        auto iter = controls_.find(config::GameCommand(joypad, game_key));
        if (iter != controls_.end()) {
            iter->second->Clear();
        }
    }
}

void JoypadConfig::ClearAllJoypads() {
    for (const config::GameJoy& joypad : config::kAllGameJoys) {
        ClearJoypad(joypad);
    }
}

void JoypadConfig::ToggleSDLGameControllerMode(bool checked) {
    OPTION(kSDLGameControllerMode) = checked;
    ClearAllJoypads();
}

void JoypadConfig::OnDefaultToggled(const config::GameJoy& joypad, bool checked) {
    if (!checked || loading_) {
        return;
    }
    // Only one joypad can be the default: uncheck the others.
    for (const config::GameJoy& other : config::kAllGameJoys) {
        if (other != joypad && default_checks_[other.index()]) {
            default_checks_[other.index()]->setChecked(false);
        }
    }
}

}  // namespace dialogs
