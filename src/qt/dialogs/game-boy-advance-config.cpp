#include "qt/dialogs/game-boy-advance-config.h"

#include <zlib.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "core/base/system.h"
#include "core/gba/gba.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/game-area.h"
#include "qt/main-window.h"
#include "qt/opts.h"
#include "qt/widgets/option-binding.h"

namespace dialogs {

namespace {

bool GbaGameLoaded() {
    MainWindow* frame = vbamApp().frame;
    return frame && frame->GetPanel() && frame->GetPanel()->game_type() == IMAGE_GBA;
}

}  // namespace

// static
GameBoyAdvanceConfig* GameBoyAdvanceConfig::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new GameBoyAdvanceConfig(parent);
}

// static
QString GameBoyAdvanceConfig::GetOverrideId() {
    bool valid = true;
    for (int i = 0; i < 4; i++) {
        const uint8_t c = g_rom[0xac + i];
        if (c < 0x21 || c > 0x7e) {
            valid = false;
            break;
        }
    }
    if (valid) {
        return QString::fromLatin1(reinterpret_cast<const char*>(&g_rom[0xac]), 4);
    }
    const uint32_t romcrc = crc32(0L, g_rom, static_cast<uInt>(gbaGetRomSize()));
    return QStringLiteral("CRC_%1").arg(romcrc, 8, 16, QLatin1Char('0')).toUpper();
}

GameBoyAdvanceConfig::GameBoyAdvanceConfig(QWidget* parent)
    : BaseDialog(parent, "GameBoyAdvanceConfig") {
    setWindowTitle(tr("Game Boy Advance options"));

    auto* layout = new QVBoxLayout(this);
    notebook_ = new QTabWidget(this);
    notebook_->addTab(CreateSaveTypeTab(), tr("Save type"));
    notebook_->addTab(CreateBootRomTab(), tr("Boot ROM"));
    notebook_->addTab(CreateGameOverridesTab(), tr("Game Overrides"));
    layout->addWidget(notebook_);
    layout->addWidget(CreateOkCancel());
}

QWidget* GameBoyAdvanceConfig::CreateSaveTypeTab() {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);
    auto* group = new QGroupBox(tr("Cartridge"), page);
    auto* form = new QFormLayout(group);

    save_type_ = new QComboBox(group);
    // The order must match the GBA_SAVE_* enum (kPrefSaveType, 0..5).
    save_type_->addItems({tr("Automatic"), QStringLiteral("EEPROM"), tr("SRAM"), tr("Flash"),
                          tr("EEPROM + Sensor"), tr("None")});
    bindings().BindComboBoxInt(save_type_, config::OptionID::kPrefSaveType);
    connect(save_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &GameBoyAdvanceConfig::OnSaveTypeChanged);
    form->addRow(tr("Save type:"), save_type_);

    flash_size_ = new QComboBox(group);
    flash_size_->addItems({tr("64 K"), tr("128 K")});
    bindings().BindComboBoxInt(flash_size_, config::OptionID::kPrefFlashSize);
    form->addRow(tr("Flash size:"), flash_size_);

    detect_ = new QPushButton(tr("Detect Now"), group);
    detect_->setAutoDefault(false);
    connect(detect_, &QPushButton::clicked, this, &GameBoyAdvanceConfig::OnDetect);
    form->addRow(detect_);
    layout->addWidget(group);

    auto* options = new QGroupBox(tr("Options"), page);
    auto* options_layout = new QVBoxLayout(options);
    auto* rtc = new QCheckBox(tr("Enable the real time clock"), options);
    bindings().BindCheckBoxIntMask(rtc, config::OptionID::kPrefRTCEnabled, 1);
    options_layout->addWidget(rtc);
    auto* agb_print = new QCheckBox(tr("Enable AGB printer output"), options);
    bindings().BindCheckBox(agb_print, config::OptionID::kPrefAgbPrint);
    options_layout->addWidget(agb_print);
    auto* lcd = new QCheckBox(tr("Enable the GBA LCD color filter"), options);
    bindings().BindCheckBox(lcd, config::OptionID::kGBALCDFilter);
    options_layout->addWidget(lcd);
    layout->addWidget(options);
    layout->addStretch(1);
    return page;
}

QWidget* GameBoyAdvanceConfig::CreateBootRomTab() {
    auto* page = new QWidget(notebook_);
    auto* form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    bios_picker_ = new widgets::PathPicker(page, /*dir=*/false,
                                           tr("BIOS files (*.bin *.rom);;All files (*)"));
    bindings().BindPathPicker(bios_picker_, config::OptionID::kGBABiosFile);
    form->addRow(tr("BIOS file:"), bios_picker_);

    auto* use_bios = new QCheckBox(tr("Use the BIOS file"), page);
    bindings().BindCheckBox(use_bios, config::OptionID::kPrefUseBiosGBA);
    form->addRow(use_bios);

    bios_label_ = new QLabel(tr("(None)"), page);
    form->addRow(tr("Current BIOS file:"), bios_label_);
    bindings().Add(
        [this] {
            const QString bios = OPTION(kGBABiosFile);
            bios_label_->setText(bios.isEmpty() ? tr("(None)") : bios);
        },
        [] { return true; });
    return page;
}

QWidget* GameBoyAdvanceConfig::CreateGameOverridesTab() {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);
    game_settings_ = new QWidget(page);
    auto* form = new QFormLayout(game_settings_);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    game_code_ = new QLabel(game_settings_);
    form->addRow(tr("Game Code"), game_code_);

    comment_ = new QLineEdit(game_settings_);
    form->addRow(tr("Comment"), comment_);

    ov_rtc_ = new QComboBox(game_settings_);
    ov_rtc_->addItems({tr("Default"), tr("Disabled"), tr("Enabled")});
    form->addRow(tr("Real Time Clock:"), ov_rtc_);

    ov_save_type_ = new QComboBox(game_settings_);
    ov_save_type_->addItems({tr("Default"), tr("Automatic"), QStringLiteral("EEPROM"), tr("SRAM"),
                             tr("Flash"), tr("EEPROM + Sensor"), tr("None")});
    form->addRow(tr("Save Type:"), ov_save_type_);

    ov_flash_size_ = new QComboBox(game_settings_);
    ov_flash_size_->addItems({tr("Default"), tr("64 K"), tr("128 K")});
    form->addRow(tr("Flash Size:"), ov_flash_size_);

    ov_mirroring_ = new QComboBox(game_settings_);
    ov_mirroring_->addItems({tr("Default"), tr("Disabled"), tr("Enabled")});
    form->addRow(tr("Mirroring:"), ov_mirroring_);

    auto* defaults = new QPushButton(tr("Defaults"), game_settings_);
    defaults->setAutoDefault(false);
    connect(defaults, &QPushButton::clicked, this, [this] {
        ov_rtc_->setCurrentIndex(0);
        ov_save_type_->setCurrentIndex(0);
        ov_flash_size_->setCurrentIndex(0);
        ov_mirroring_->setCurrentIndex(0);
    });
    form->addRow(defaults);

    layout->addWidget(game_settings_);
    layout->addStretch(1);
    return page;
}

void GameBoyAdvanceConfig::OnSaveTypeChanged(int index) {
    // The flash size only applies to the automatic and flash save types.
    flash_size_->setEnabled(index == GBA_SAVE_AUTO || index == GBA_SAVE_FLASH);
}

void GameBoyAdvanceConfig::OnDetect() {
    if (!GbaGameLoaded()) {
        return;
    }
    const uint32_t sz = vbamApp().frame->GetPanel()->game_size();
    flashDetectSaveType(static_cast<int>(sz));
    save_type_->setCurrentIndex(coreOptions.saveType);

    if (coreOptions.saveType == GBA_SAVE_FLASH) {
        flash_size_->setCurrentIndex(g_flashSize == 0x20000 ? 1 : 0);
        flash_size_->setEnabled(true);
    } else {
        flash_size_->setEnabled(false);
    }
}

void GameBoyAdvanceConfig::OnDialogShown() {
    const bool gba = GbaGameLoaded();
    detect_->setEnabled(gba);
    game_settings_->setEnabled(gba);
    OnSaveTypeChanged(save_type_->currentIndex());
    LoadOverrides();
}

void GameBoyAdvanceConfig::ResetOverrideControls() {
    ov_rtc_->setCurrentIndex(0);
    ov_save_type_->setCurrentIndex(0);
    ov_flash_size_->setCurrentIndex(0);
    ov_mirroring_->setCurrentIndex(0);
}

void GameBoyAdvanceConfig::LoadOverrides() {
    if (!GbaGameLoaded()) {
        game_code_->clear();
        comment_->clear();
        override_comment_on_show_.clear();
        ResetOverrideControls();
        return;
    }

    const QString id = GetOverrideId();
    game_code_->setText(id);
    QString cmt = QString::fromLatin1(reinterpret_cast<const char*>(&g_rom[0xa0]), 12);

    QSettings* cfg = vbamApp().overrides();
    if (cfg && cfg->childGroups().contains(id)) {
        cfg->beginGroup(id);
        cmt = cfg->value(QStringLiteral("comment"), cmt).toString();
        comment_->setText(cmt);
        ov_rtc_->setCurrentIndex(cfg->value(QStringLiteral("rtcEnabled"), -1).toInt() + 1);
        ov_save_type_->setCurrentIndex(cfg->value(QStringLiteral("saveType"), -1).toInt() + 1);
        ov_flash_size_->setCurrentIndex(
            (cfg->value(QStringLiteral("flashSize"), -1).toInt() >> 17) + 1);
        ov_mirroring_->setCurrentIndex(
            cfg->value(QStringLiteral("mirroringEnabled"), -1).toInt() + 1);
        cfg->endGroup();
    } else {
        comment_->setText(cmt);
        ResetOverrideControls();
    }
    override_comment_on_show_ = cmt;
}

void GameBoyAdvanceConfig::SaveOverrides() {
    if (!GbaGameLoaded()) {
        return;
    }
    QSettings* cfg = vbamApp().overrides();
    if (!cfg) {
        return;
    }

    const QString id = GetOverrideId();
    bool chg;
    if (cfg->childGroups().contains(id)) {
        cfg->beginGroup(id);
        chg = comment_->text() != override_comment_on_show_ ||
              ov_rtc_->currentIndex() != cfg->value(QStringLiteral("rtcEnabled"), -1).toInt() + 1 ||
              ov_save_type_->currentIndex() !=
                  cfg->value(QStringLiteral("saveType"), -1).toInt() + 1 ||
              ov_flash_size_->currentIndex() !=
                  (cfg->value(QStringLiteral("flashSize"), -1).toInt() >> 17) + 1 ||
              ov_mirroring_->currentIndex() !=
                  cfg->value(QStringLiteral("mirroringEnabled"), -1).toInt() + 1;
        cfg->endGroup();
    } else {
        chg = ov_rtc_->currentIndex() != 0 || ov_save_type_->currentIndex() != 0 ||
              ov_flash_size_->currentIndex() != 0 || ov_mirroring_->currentIndex() != 0;
    }
    if (!chg) {
        return;
    }

    // Rewrite the game's group. Only the overridden values are stored, like
    // the wx port's vba-over.ini writer.
    cfg->remove(id);
    cfg->beginGroup(id);
    cfg->setValue(QStringLiteral("comment"), comment_->text());
    int sel;
    if ((sel = ov_rtc_->currentIndex()) > 0) {
        cfg->setValue(QStringLiteral("rtcEnabled"), sel - 1);
    }
    if ((sel = ov_save_type_->currentIndex()) > 0) {
        cfg->setValue(QStringLiteral("saveType"), sel - 1);
    }
    if ((sel = ov_flash_size_->currentIndex()) > 0) {
        cfg->setValue(QStringLiteral("flashSize"), 0x10000 << (sel - 1));
    }
    if ((sel = ov_mirroring_->currentIndex()) > 0) {
        cfg->setValue(QStringLiteral("mirroringEnabled"), sel - 1);
    }
    cfg->endGroup();
    cfg->sync();
}

bool GameBoyAdvanceConfig::OnAccept() {
    SaveOverrides();
    return true;
}

}  // namespace dialogs
