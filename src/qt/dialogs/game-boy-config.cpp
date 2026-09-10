#include "qt/dialogs/game-boy-config.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "qt/config/option-proxy.h"
#include "qt/widgets/option-binding.h"

namespace dialogs {

namespace {

static constexpr size_t kNbPalettes = 3;

// These are the choices for canned colors; their order must match the names in
// the default palette selector.
// clang-format off
static constexpr std::array<std::array<uint16_t, 8>, 9> kDefaultPalettes = {{
    // Standard
    {0x7FFF, 0x56B5, 0x318C, 0x0000, 0x7FFF, 0x56B5, 0x318C, 0x0000},
    // Blue Sea
    {0x6200, 0x7E10, 0x7C10, 0x5000, 0x6200, 0x7E10, 0x7C10, 0x5000},
    // Dark Night
    {0x4008, 0x4000, 0x2000, 0x2008, 0x4008, 0x4000, 0x2000, 0x2008},
    // Green Forest
    {0x43F0, 0x03E0, 0x4200, 0x2200, 0x43F0, 0x03E0, 0x4200, 0x2200},
    // Hot Desert
    {0x43FF, 0x03FF, 0x221F, 0x021F, 0x43FF, 0x03FF, 0x221F, 0x021F},
    // Pink Dreams
    {0x621F, 0x7E1F, 0x7C1F, 0x2010, 0x621F, 0x7E1F, 0x7C1F, 0x2010},
    // Weird Colors
    {0x621F, 0x401F, 0x001F, 0x2010, 0x621F, 0x401F, 0x001F, 0x2010},
    // Real GB Colors
    {0x1B8E, 0x02C0, 0x0DA0, 0x1140, 0x1B8E, 0x02C0, 0x0DA0, 0x1140},
    // Real 'GB on GBASP' Colors
    {0x7BDE, 0x5778, 0x5640, 0x0000, 0x7BDE, 0x529C, 0x2990, 0x0000},
}};
// clang-format on

QColor ColourFromElement(uint16_t element) {
    return QColor((element << 3) & 0xf8, (element >> 2) & 0xf8, (element >> 7) & 0xf8);
}

uint16_t ElementFromColour(const QColor& colour) {
    return static_cast<uint16_t>(((colour.red() & 0xf8) >> 3) + ((colour.green() & 0xf8) << 2) +
                                 ((colour.blue() & 0xf8) << 7));
}

}  // namespace

// static
GameBoyConfig* GameBoyConfig::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new GameBoyConfig(parent);
}

GameBoyConfig::GameBoyConfig(QWidget* parent) : BaseDialog(parent, "GameBoyConfig") {
    setWindowTitle(tr("Game Boy options"));

    auto* layout = new QVBoxLayout(this);
    notebook_ = new QTabWidget(this);
    notebook_->addTab(CreateSystemTab(), tr("System"));
    notebook_->addTab(CreateBootRomTab(), tr("Boot ROM"));
    notebook_->addTab(CreateCustomColorsTab(), tr("Custom Colors"));
    layout->addWidget(notebook_);
    layout->addWidget(CreateOkCancel());
}

QWidget* GameBoyConfig::CreateSystemTab() {
    auto* page = new QWidget(notebook_);
    auto* form = new QFormLayout(page);

    auto* system = new QComboBox(page);
    // The order must match gbEmulatorType (kPrefEmulatorType, 0..5).
    system->addItems({tr("Automatic"), QStringLiteral("Game Boy Color"),
                      QStringLiteral("Super Game Boy"), QStringLiteral("Game Boy"),
                      QStringLiteral("Game Boy Advance"), QStringLiteral("Super Game Boy 2")});
    bindings().BindComboBoxInt(system, config::OptionID::kPrefEmulatorType);
    form->addRow(tr("Emulated system:"), system);

    // The borders selector controls kPrefBorderOn and kPrefBorderAutomatic.
    auto* borders = new QComboBox(page);
    borders->addItems({tr("Never"), tr("Always"), tr("Automatic")});
    bindings().Add(
        [borders] {
            if (!OPTION(kPrefBorderOn) && !OPTION(kPrefBorderAutomatic)) {
                borders->setCurrentIndex(0);
            } else if (OPTION(kPrefBorderOn)) {
                borders->setCurrentIndex(1);
            } else {
                borders->setCurrentIndex(2);
            }
        },
        [borders] {
            switch (borders->currentIndex()) {
                case 0:
                    OPTION(kPrefBorderOn) = false;
                    OPTION(kPrefBorderAutomatic) = false;
                    break;
                case 1:
                    OPTION(kPrefBorderOn) = true;
                    break;
                case 2:
                    OPTION(kPrefBorderOn) = false;
                    OPTION(kPrefBorderAutomatic) = true;
                    break;
            }
            return true;
        });
    form->addRow(tr("Display borders:"), borders);

    auto* colorizer = new QCheckBox(tr("Enable the colorizer hack (requires a restart)"), page);
    bindings().BindCheckBox(colorizer, config::OptionID::kGBColorizerHack);
    form->addRow(colorizer);

    auto* printer_group = new QGroupBox(tr("Game Boy Printer"), page);
    auto* printer_layout = new QVBoxLayout(printer_group);
    auto* printer = new QCheckBox(tr("Enable the Game Boy Printer"), printer_group);
    bindings().BindCheckBoxIntMask(printer, config::OptionID::kPrefGBPrinter, 1);
    printer_layout->addWidget(printer);
    auto* auto_page = new QCheckBox(tr("Automatically gather a full page before printing"),
                                    printer_group);
    bindings().BindCheckBox(auto_page, config::OptionID::kGBPrintAutoPage);
    printer_layout->addWidget(auto_page);
    auto* screen_cap = new QCheckBox(tr("Automatically save printouts as screen captures"),
                                     printer_group);
    bindings().BindCheckBox(screen_cap, config::OptionID::kGBPrintScreenCap);
    printer_layout->addWidget(screen_cap);
    form->addRow(printer_group);
    return page;
}

QWidget* GameBoyConfig::CreateBootRomTab() {
    auto* page = new QWidget(notebook_);
    auto* form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const QString filter = tr("BIOS files (*.bin *.rom *.gb *.gbc);;All files (*)");

    gb_bios_picker_ = new widgets::PathPicker(page, /*dir=*/false, filter);
    bindings().BindPathPicker(gb_bios_picker_, config::OptionID::kGBBiosFile);
    form->addRow(tr("Game Boy Boot ROM file:"), gb_bios_picker_);
    auto* use_gb = new QCheckBox(tr("Use the Game Boy Boot ROM"), page);
    bindings().BindCheckBox(use_gb, config::OptionID::kPrefUseBiosGB);
    form->addRow(use_gb);

    gbc_bios_picker_ = new widgets::PathPicker(page, /*dir=*/false, filter);
    bindings().BindPathPicker(gbc_bios_picker_, config::OptionID::kGBGBCBiosFile);
    form->addRow(tr("Game Boy Color Boot ROM file:"), gbc_bios_picker_);
    auto* use_gbc = new QCheckBox(tr("Use the Game Boy Color Boot ROM"), page);
    bindings().BindCheckBox(use_gbc, config::OptionID::kPrefUseBiosGBC);
    form->addRow(use_gbc);

    gb_bios_label_ = new QLabel(tr("(None)"), page);
    form->addRow(tr("Current Game Boy BIOS file:"), gb_bios_label_);
    gbc_bios_label_ = new QLabel(tr("(None)"), page);
    form->addRow(tr("Current Game Boy Color BIOS file:"), gbc_bios_label_);
    bindings().Add(
        [this] {
            const QString gb = OPTION(kGBBiosFile);
            gb_bios_label_->setText(gb.isEmpty() ? tr("(None)") : gb);
            const QString gbc = OPTION(kGBGBCBiosFile);
            gbc_bios_label_->setText(gbc.isEmpty() ? tr("(None)") : gbc);
        },
        [] { return true; });
    return page;
}

QWidget* GameBoyConfig::CreateCustomColorsTab() {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);
    inner_notebook_ = new QTabWidget(page);
    const QString labels[kNbPalettes] = {tr("Default"), tr("User 1"), tr("User 2")};
    palettes_.resize(kNbPalettes);
    for (size_t i = 0; i < kNbPalettes; i++) {
        inner_notebook_->addTab(CreatePaletteTab(inner_notebook_, i), labels[i]);
    }
    layout->addWidget(inner_notebook_);
    return page;
}

QWidget* GameBoyConfig::CreatePaletteTab(QWidget* parent, size_t palette_id) {
    VBAM_CHECK(palette_id < kNbPalettes);
    PalettePanel& panel = palettes_[palette_id];
    panel.option_id = static_cast<config::OptionID>(
        static_cast<size_t>(config::OptionID::kGBPalette0) + palette_id);

    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    panel.default_selector = new QComboBox(page);
    panel.default_selector->addItems(
        {tr("Custom"), tr("Standard"), tr("Blue Sea"), tr("Dark Night"), tr("Green Forest"),
         tr("Hot Desert"), tr("Pink Dreams"), tr("Weird Colors"), tr("Real Game Boy Colors"),
         tr("Real 'Game Boy on Game Boy Advance Special' Colors")});
    connect(panel.default_selector, QOverload<int>::of(&QComboBox::activated), this,
            [this, palette_id](int selection) {
                OnDefaultPaletteSelected(palettes_[palette_id], selection);
            });
    layout->addWidget(panel.default_selector);

    auto make_group = [this, page, &panel, palette_id](const QString& title, size_t first) {
        auto* group = new QGroupBox(title, page);
        auto* row = new QHBoxLayout(group);
        for (size_t i = first; i < first + 4; i++) {
            auto* button = new QPushButton(group);
            button->setAutoDefault(false);
            button->setMinimumSize(40, 24);
            panel.colour_buttons[i] = button;
            connect(button, &QPushButton::clicked, this,
                    [this, palette_id, i] { OnColourClicked(palettes_[palette_id], i); });
            row->addWidget(button);
        }
        return group;
    };
    layout->addWidget(make_group(tr("Background"), 0));
    layout->addWidget(make_group(tr("Sprites"), 4));

    auto* bottom = new QHBoxLayout();
    panel.use_palette = new QCheckBox(tr("Use this palette"), page);
    bindings().BindButtonSelected(panel.use_palette, config::OptionID::kPrefGBPaletteOption,
                                  static_cast<int>(palette_id));
    connect(panel.use_palette, &QCheckBox::toggled, this,
            [this, palette_id](bool checked) { OnUsePaletteToggled(palette_id, checked); });
    bottom->addWidget(panel.use_palette);
    bottom->addStretch(1);
    auto* reset = new QPushButton(tr("Restore"), page);
    reset->setAutoDefault(false);
    connect(reset, &QPushButton::clicked, this,
            [this, palette_id] { OnPaletteReset(palettes_[palette_id]); });
    bottom->addWidget(reset);
    layout->addLayout(bottom);

    // Load the working palette from the option on show, write it back on OK.
    bindings().Add(
        [this, palette_id] {
            PalettePanel& p = palettes_[palette_id];
            p.palette = config::Option::ByID(p.option_id)->GetGbPalette();
            UpdateColourButtons(p);
        },
        [this, palette_id] {
            PalettePanel& p = palettes_[palette_id];
            return config::Option::ByID(p.option_id)->SetGbPalette(p.palette);
        });
    return page;
}

void GameBoyConfig::UpdateColourButtons(PalettePanel& panel) {
    // Update all of the colour buttons based on the current palette values.
    for (size_t i = 0; i < panel.palette.size(); i++) {
        const QColor colour = ColourFromElement(panel.palette[i]);
        panel.colour_buttons[i]->setStyleSheet(
            QStringLiteral("background-color: %1; border: 1px solid #808080;").arg(colour.name()));
        panel.colour_buttons[i]->setToolTip(colour.name());
    }

    // See if the current palette corresponds to a default palette.
    for (size_t i = 0; i < kDefaultPalettes.size(); i++) {
        if (panel.palette == kDefaultPalettes[i]) {
            panel.default_selector->setCurrentIndex(static_cast<int>(i) + 1);
            return;
        }
    }

    // The configuration is not a default palette, set it to "Custom".
    panel.default_selector->setCurrentIndex(0);
}

void GameBoyConfig::OnColourClicked(PalettePanel& panel, size_t colour_index) {
    VBAM_CHECK(colour_index < panel.palette.size());
    const QColor colour = QColorDialog::getColor(ColourFromElement(panel.palette[colour_index]),
                                                 this, tr("Select a color"));
    if (!colour.isValid()) {
        return;
    }
    panel.palette[colour_index] = ElementFromColour(colour);
    UpdateColourButtons(panel);
}

void GameBoyConfig::OnDefaultPaletteSelected(PalettePanel& panel, int selection) {
    if (selection > 0 && static_cast<size_t>(selection - 1) < kDefaultPalettes.size()) {
        // Update the palette to one of the default palettes.
        panel.palette = kDefaultPalettes[selection - 1];
        UpdateColourButtons(panel);
    }
}

void GameBoyConfig::OnPaletteReset(PalettePanel& panel) {
    // Reset the palette to the last user-saved value.
    panel.palette = config::Option::ByID(panel.option_id)->GetGbPalette();
    UpdateColourButtons(panel);
}

void GameBoyConfig::OnUsePaletteToggled(size_t palette_id, bool checked) {
    if (!checked) {
        return;
    }
    // Only one palette can be in use: uncheck the others.
    for (size_t i = 0; i < palettes_.size(); i++) {
        if (i != palette_id && palettes_[i].use_palette) {
            palettes_[i].use_palette->setChecked(false);
        }
    }
}

}  // namespace dialogs
