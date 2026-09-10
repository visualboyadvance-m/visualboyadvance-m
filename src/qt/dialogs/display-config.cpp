#include "qt/dialogs/display-config.h"

#include "qt/renderers/sdl-panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLibrary>
#include <QRadioButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "components/filters_agb/filters_agb.h"
#include "components/filters_cgb/filters_cgb.h"
#include "core/base/check.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/opts.h"
#include "qt/rpi.h"
#include "qt/widgets/option-binding.h"
#include "qt/widgets/render-plugin.h"

namespace dialogs {

namespace {

// Labels for the LCD filter variants. The order must match GbaFilterVariant /
// GbcFilterVariant.
QStringList GbaVariantLabels() {
    return {QStringLiteral("GBA"), QStringLiteral("GBASP Backlit"), QStringLiteral("Micro"),
            QStringLiteral("DS"), QStringLiteral("DS-Lite"), QStringLiteral("NSO")};
}

QStringList GbcVariantLabels() {
    return {QStringLiteral("GBC"), QStringLiteral("NSO")};
}

}  // namespace

// static
DisplayConfig* DisplayConfig::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new DisplayConfig(parent);
}

DisplayConfig::DisplayConfig(QWidget* parent) : BaseDialog(parent, "DisplayConfig") {
    setWindowTitle(tr("Display options"));

    auto* layout = new QVBoxLayout(this);
    notebook_ = new QTabWidget(this);
    notebook_->addTab(CreateBasicTab(), tr("Basic"));
    notebook_->addTab(CreateBitDepthTab(), tr("Bit Depth"));
    notebook_->addTab(CreateColorCorrectionTab(), tr("Color Correction"));
    notebook_->addTab(CreateSpeedTab(), tr("Speed"));
    notebook_->addTab(CreateOSDTab(), tr("On-Screen Display"));
    notebook_->addTab(CreateZoomTab(), tr("Zoom"));
    layout->addWidget(notebook_);
    layout->addWidget(CreateOkCancel());

    filter_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kDispFilter,
        [this](config::Option* option) { OnFilterChanged(option); });
    interframe_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kDispIFB,
        [this](config::Option* option) { OnInterframeChanged(option); });
}

QWidget* DisplayConfig::CreateBasicTab() {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);

    // Output module.
    auto* output_group = new QGroupBox(tr("Output module"), page);
    auto* output_layout = new QVBoxLayout(output_group);
    auto* radios = new QHBoxLayout();
    const QStringList render_labels = widgets::RenderMethodLabels();
    for (size_t i = 0; i < config::kNbRenderMethods; i++) {
        const auto method = static_cast<config::RenderMethod>(i);
        auto* radio = new QRadioButton(render_labels.value(static_cast<int>(i)), output_group);
        bindings().BindButtonSelected(radio, config::OptionID::kDispRenderMethod,
                                      static_cast<int>(i));
        radios->addWidget(radio);
        render_method_radios_.push_back({radio, method});
    }
    radios->addStretch(1);
    output_layout->addLayout(radios);

    // SDL sub-options: which SDL render driver backs the SDL render method, and
    // SDL 3.4's pixel-art scale mode.
    sdl_options_ = new QWidget(output_group);
    auto* sdl_layout = new QHBoxLayout(sdl_options_);
    sdl_layout->setContentsMargins(0, 0, 0, 0);
    sdl_layout->addWidget(new QLabel(tr("SDL renderer:"), sdl_options_));
    sdl_renderer_ = new QComboBox(sdl_options_);
    for (const QString& name : SDLDrawingPanel::AvailableRenderers()) {
        sdl_renderer_->addItem(name == QLatin1String("default") ? tr("Default") : name, name);
    }
    bindings().BindComboBoxString(sdl_renderer_, config::OptionID::kSDLRenderer);
    sdl_layout->addWidget(sdl_renderer_, 1);
    sdl_pixel_art_ = new QCheckBox(tr("Pixel art scaling"), sdl_options_);
    bindings().BindCheckBox(sdl_pixel_art_, config::OptionID::kDispSDLPixelArt);
    sdl_layout->addWidget(sdl_pixel_art_);
    output_layout->addWidget(sdl_options_);
    for (const auto& radio : render_method_radios_) {
        connect(radio.first, &QRadioButton::toggled, this,
                &DisplayConfig::UpdateSdlOptionsVisibility);
    }
    UpdateSdlOptionsVisibility();

    bilinear_ = new QCheckBox(tr("Use bilinear filtering"), output_group);
    bindings().BindCheckBox(bilinear_, config::OptionID::kDispBilinear);
    output_layout->addWidget(bilinear_);

    stretch_ = new QCheckBox(tr("Retain aspect ratio"), output_group);
    bindings().BindCheckBox(stretch_, config::OptionID::kDispStretch);
    output_layout->addWidget(stretch_);

    keep_on_top_ = new QCheckBox(tr("Keep window on top"), output_group);
    bindings().BindCheckBox(keep_on_top_, config::OptionID::kDispKeepOnTop);
    output_layout->addWidget(keep_on_top_);

    auto* vsync = new QCheckBox(tr("Enable VSync"), output_group);
    bindings().BindCheckBox(vsync, config::OptionID::kPrefVsync);
    output_layout->addWidget(vsync);

    auto* threads_row = new QHBoxLayout();
    threads_row->addWidget(new QLabel(tr("Filter threads (0 = automatic):"), output_group));
    max_threads_ = new QSpinBox(output_group);
    bindings().BindSpinBox(max_threads_, config::OptionID::kDispMaxThreads);
    threads_row->addWidget(max_threads_);
    threads_row->addStretch(1);
    output_layout->addLayout(threads_row);
    layout->addWidget(output_group);

    // Filters.
    auto* filters_group = new QGroupBox(tr("Filters"), page);
    auto* filters_form = new QFormLayout(filters_group);
    filters_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    plugin_dir_picker_ = new widgets::PathPicker(filters_group, /*dir=*/true);
    // The plugin dir applies as soon as it changes (like the wx port): it
    // drives the plugin list below.
    connect(plugin_dir_picker_, &widgets::PathPicker::pathChanged, this,
            &DisplayConfig::OnPluginDirChanged);
    filters_form->addRow(tr("Plugin filter dir:"), plugin_dir_picker_);

    filter_selector_ = new QComboBox(filters_group);
    filter_selector_->addItems(widgets::FilterLabels());
    connect(filter_selector_, QOverload<int>::of(&QComboBox::activated), this,
            &DisplayConfig::OnFilterSelected);
    filters_form->addRow(tr("Display filter:"), filter_selector_);

    plugin_label_ = new QLabel(tr("Filter plugin:"), filters_group);
    plugin_selector_ = new QComboBox(filters_group);
    connect(plugin_selector_, QOverload<int>::of(&QComboBox::activated), this,
            &DisplayConfig::OnPluginSelected);
    filters_form->addRow(plugin_label_, plugin_selector_);

    interframe_selector_ = new QComboBox(filters_group);
    interframe_selector_->addItems(widgets::InterframeLabels());
    connect(interframe_selector_, QOverload<int>::of(&QComboBox::activated), this,
            [this](int index) {
                // Applied live.
                if (index >= 0 && static_cast<size_t>(index) < config::kNbInterframes) {
                    OPTION(kDispIFB) = static_cast<config::Interframe>(index);
                }
            });
    filters_form->addRow(tr("Interframe blending:"), interframe_selector_);
    layout->addWidget(filters_group);
    layout->addStretch(1);

    // The filter and interframe selectors are applied live; loading them from
    // the options happens in OnDialogShown() (the plugin list has to be
    // populated first).
    return page;
}

QWidget* DisplayConfig::CreateBitDepthTab() {
    auto* page = new QWidget(notebook_);
    auto* form = new QFormLayout(page);
    auto* bit_depth = new QComboBox(page);
    bit_depth->addItems({QStringLiteral("8"), QStringLiteral("16"), QStringLiteral("24"),
                         QStringLiteral("32")});
    bindings().BindComboBoxInt(bit_depth, config::OptionID::kBitDepth);
    form->addRow(tr("Bit Depth:"), bit_depth);
    return page;
}

QWidget* DisplayConfig::CreateColorCorrectionTab() {
    auto* page = new QWidget(notebook_);
    auto* form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // Profile radios: applied live; an explicit pick takes the profile out of
    // auto mode.
    auto* profiles = new QHBoxLayout();
    const QStringList profile_labels = widgets::ColorCorrectionProfileLabels();
    for (size_t i = 0; i < config::kNbColorCorrectionProfiles; i++) {
        auto* radio = new QRadioButton(profile_labels.value(static_cast<int>(i)), page);
        const auto profile = static_cast<config::ColorCorrectionProfile>(i);
        connect(radio, &QRadioButton::clicked, this, [profile](bool checked) {
            if (checked) {
                OPTION(kDispColorCorrectionAuto) = false;
                OPTION(kDispColorCorrectionProfile) = profile;
            }
        });
        profiles->addWidget(radio);
        profile_radios_.push_back(radio);
    }
    profiles->addStretch(1);
    form->addRow(tr("Profile:"), profiles);

    gba_variant_ = new QComboBox(page);
    gba_variant_->addItems(GbaVariantLabels());
    connect(gba_variant_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        OPTION(kGBALCDFilterVariant) = static_cast<uint32_t>(std::max(0, index));
        SyncLcdSliderEnable();
    });
    form->addRow(tr("LCD Correction for GBA:"), gba_variant_);

    gba_darken_ = new QSlider(Qt::Horizontal, page);
    gba_darken_->setRange(static_cast<int>(OPTION(kGBADarken).Min()),
                          static_cast<int>(OPTION(kGBADarken).Max()));
    connect(gba_darken_, &QSlider::valueChanged, this,
            [](int value) { OPTION(kGBADarken) = static_cast<uint32_t>(std::max(0, value)); });
    {
        auto* container = new QWidget(page);
        auto* v = new QVBoxLayout(container);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(widgets::WrapSliderWithValueLabel(gba_darken_, QStringLiteral("%"), [this] {
            widgets::ResetSliderToOptionDefault(gba_darken_, config::OptionID::kGBADarken);
        }));
        auto* captions = new QHBoxLayout();
        captions->addWidget(new QLabel(tr("Lighter"), container));
        captions->addStretch(1);
        captions->addWidget(new QLabel(tr("Very Dark"), container));
        v->addLayout(captions);
        form->addRow(tr("GBA Darken:"), container);
    }

    gb_variant_ = new QComboBox(page);
    gb_variant_->addItems(GbcVariantLabels());
    connect(gb_variant_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        OPTION(kGBLCDFilterVariant) = static_cast<uint32_t>(std::max(0, index));
        SyncLcdSliderEnable();
    });
    form->addRow(tr("LCD Correction for GBC:"), gb_variant_);

    gbc_lighten_ = new QSlider(Qt::Horizontal, page);
    gbc_lighten_->setRange(static_cast<int>(OPTION(kGBLighten).Min()),
                           static_cast<int>(OPTION(kGBLighten).Max()));
    connect(gbc_lighten_, &QSlider::valueChanged, this,
            [](int value) { OPTION(kGBLighten) = static_cast<uint32_t>(std::max(0, value)); });
    {
        auto* container = new QWidget(page);
        auto* v = new QVBoxLayout(container);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(widgets::WrapSliderWithValueLabel(gbc_lighten_, QStringLiteral("%"), [this] {
            widgets::ResetSliderToOptionDefault(gbc_lighten_, config::OptionID::kGBLighten);
        }));
        auto* captions = new QHBoxLayout();
        captions->addWidget(new QLabel(tr("Darker"), container));
        captions->addStretch(1);
        captions->addWidget(new QLabel(tr("Very Light"), container));
        v->addLayout(captions);
        form->addRow(tr("GBC Lighten:"), container);
    }

    auto* gba_lcd = new QCheckBox(tr("Enable the GBA LCD color filter"), page);
    bindings().BindCheckBox(gba_lcd, config::OptionID::kGBALCDFilter);
    form->addRow(gba_lcd);
    auto* gb_lcd = new QCheckBox(tr("Enable the GBC LCD color filter"), page);
    bindings().BindCheckBox(gb_lcd, config::OptionID::kGBLCDFilter);
    form->addRow(gb_lcd);
    return page;
}

QWidget* DisplayConfig::CreateSpeedTab() {
    auto* page = new QWidget(notebook_);
    auto* layout = new QVBoxLayout(page);
    auto* group = new QGroupBox(tr("Frame Skip"), page);
    auto* form = new QFormLayout(group);
    auto* frame_skip = new QSpinBox(group);
    bindings().BindSpinBox(frame_skip, config::OptionID::kPrefFrameSkip);
    frame_skip->setSpecialValueText(tr("Automatic"));
    form->addRow(tr("Number of frames to skip:"), frame_skip);
    auto* auto_skip = new QCheckBox(tr("Automatic frame skip"), group);
    bindings().BindCheckBox(auto_skip, config::OptionID::kPrefAutoFrameSkip);
    form->addRow(auto_skip);
    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget* DisplayConfig::CreateOSDTab() {
    auto* page = new QWidget(notebook_);
    auto* form = new QFormLayout(page);
    auto* speed = new QComboBox(page);
    speed->addItems({tr("None"), tr("Percentage"), tr("Detailed")});
    bindings().BindComboBoxInt(speed, config::OptionID::kPrefShowSpeed);
    form->addRow(tr("Speed indicator:"), speed);
    auto* no_status = new QCheckBox(tr("Disable on-screen status messages"), page);
    bindings().BindCheckBox(no_status, config::OptionID::kPrefDisableStatus);
    form->addRow(no_status);
    return page;
}

QWidget* DisplayConfig::CreateZoomTab() {
    auto* page = new QWidget(notebook_);
    auto* form = new QFormLayout(page);

    default_scale_ = new QDoubleSpinBox(page);
    default_scale_->setDecimals(1);
    default_scale_->setSingleStep(0.5);
    bindings().BindDoubleSpinBox(default_scale_, config::OptionID::kDispScale);
    form->addRow(tr("Default magnification:"), default_scale_);

    max_scale_ = new QSpinBox(page);
    bindings().BindSpinBox(max_scale_, config::OptionID::kPrefMaxScale);
    max_scale_->setSpecialValueText(tr("Unlimited"));
    form->addRow(tr("Maximum magnification factor:"), max_scale_);

    fullscreen_mode_ = new QComboBox(page);
    bindings().Add(
        [this] {
            PopulateFullscreenModes();
            const int index = fullscreen_mode_->findData(gopts.fs_mode);
            fullscreen_mode_->setCurrentIndex(index >= 0 ? index : 0);
        },
        [this] {
            gopts.fs_mode = fullscreen_mode_->currentData().toSize();
            return true;
        });
    form->addRow(tr("Fullscreen mode:"), fullscreen_mode_);
    return page;
}

void DisplayConfig::PopulateFullscreenModes() {
    fullscreen_mode_->clear();
    fullscreen_mode_->addItem(tr("Desktop resolution"), QSize());
    // Qt cannot enumerate the display modes; offer the sizes of the connected
    // screens.
    for (const QScreen* screen : QGuiApplication::screens()) {
        const QSize size = screen->size();
        if (fullscreen_mode_->findData(size) < 0) {
            fullscreen_mode_->addItem(tr("%1 x %2").arg(size.width()).arg(size.height()), size);
        }
    }
}

void DisplayConfig::OnDialogShown() {
    // Retake every time: the dialog instance is cached and reused.
    SnapshotLiveOptions();

    plugin_dir_picker_->SetPath(OPTION(kDispPluginDir).Get().isEmpty()
                                    ? vbamApp().GetPluginsDir()
                                    : OPTION(kDispPluginDir).Get());
    PopulatePluginOptions();

    updating_selectors_ = true;
    filter_selector_->setCurrentIndex(static_cast<int>(OPTION(kDispFilter).Get()));
    interframe_selector_->setCurrentIndex(static_cast<int>(OPTION(kDispIFB).Get()));
    const auto profile = OPTION(kDispColorCorrectionProfile).Get();
    for (size_t i = 0; i < profile_radios_.size(); i++) {
        profile_radios_[i]->setChecked(static_cast<size_t>(profile) == i);
    }
    gba_variant_->setCurrentIndex(static_cast<int>(OPTION(kGBALCDFilterVariant).Get()));
    gb_variant_->setCurrentIndex(static_cast<int>(OPTION(kGBLCDFilterVariant).Get()));
    gba_darken_->setValue(static_cast<int>(OPTION(kGBADarken).Get()));
    gbc_lighten_->setValue(static_cast<int>(OPTION(kGBLighten).Get()));
    updating_selectors_ = false;
    SyncLcdSliderEnable();
}

void DisplayConfig::OnDialogHidden() {
    live_snapshot_taken_ = false;
}

bool DisplayConfig::OnAccept() {
    // The plugin selection is only meaningful with the Plugin filter.
    if (filter_selector_->currentIndex() == static_cast<int>(config::Filter::kPlugin)) {
        const QString plugin = plugin_selector_->currentData().toString();
        OPTION(kDispFilterPlugin) = plugin;
        if (plugin.isEmpty()) {
            OPTION(kDispFilter) = config::Filter::kNone;
        }
    }
    return true;
}

void DisplayConfig::reject() {
    RestoreLiveOptions();
    BaseDialog::reject();
}

void DisplayConfig::SyncLcdSliderEnable() {
    // Grey out the slider for variants whose shader has no darken/lighten term.
    gba_darken_->setEnabled(gbafilter_variant_has_darken(std::max(0, gba_variant_->currentIndex())));
    gbc_lighten_->setEnabled(
        gbcfilter_variant_has_lighten(std::max(0, gb_variant_->currentIndex())));
}

void DisplayConfig::PopulatePluginOptions() {
    // Populate the plugin values, if any.
    const QString plugin_path = vbamApp().GetPluginsDir();
    QStringList plugins;
    QDir dir(plugin_path);
    for (const QFileInfo& info :
         dir.entryInfoList({QStringLiteral("*.rpi")}, QDir::Files, QDir::Name)) {
        plugins << info.absoluteFilePath();
    }

    if (plugins.isEmpty()) {
        HidePluginOptions();
        return;
    }

    plugin_selector_->clear();
    plugin_selector_->addItem(tr("None"), QString());

    const QString selected_plugin = OPTION(kDispFilterPlugin);
    bool is_plugin_selected = false;

    for (const QString& plugin : plugins) {
        QLibrary filter_plugin;
        const RENDER_PLUGIN_INFO* plugin_info =
            widgets::MaybeLoadFilterPlugin(plugin, &filter_plugin);
        if (!plugin_info) {
            continue;
        }
        const QString name = QString::fromUtf8(plugin_info->Name);
        filter_plugin.unload();

        plugin_selector_->addItem(QFileInfo(plugin).completeBaseName() + QStringLiteral(": ") + name,
                                  plugin);
        if (plugin == selected_plugin) {
            plugin_selector_->setCurrentIndex(plugin_selector_->count() - 1);
            is_plugin_selected = true;
        }
    }

    if (plugin_selector_->count() == 1) {
        vbam::LogWarning(tr("No usable rpi plugins found in %1").arg(plugin_path));
        HidePluginOptions();
        return;
    }

    if (!is_plugin_selected) {
        OPTION(kDispFilterPlugin) = QString();
        plugin_selector_->setCurrentIndex(0);
    }

    ShowPluginOptions();
}

void DisplayConfig::HidePluginOptions() {
    // Plugin directory picker is always visible so users can set the path.
    plugin_label_->hide();
    plugin_selector_->hide();
    plugin_options_visible_ = false;

    // Remove the Plugin option, which should be the last.
    if (filter_selector_->count() == static_cast<int>(config::kNbFilters)) {
        // Make sure we have not selected the plugin option.
        if (OPTION(kDispFilter) == config::Filter::kPlugin) {
            OPTION(kDispFilter) = config::Filter::kNone;
        }
        filter_selector_->removeItem(static_cast<int>(config::kNbFilters) - 1);
    }

    // Also erase the Plugin value to avoid issues down the line.
    OPTION(kDispFilterPlugin) = QString();
}

void DisplayConfig::ShowPluginOptions() {
    plugin_label_->show();
    plugin_selector_->show();
    plugin_options_visible_ = true;

    // Re-add the Plugin option, if needed.
    if (filter_selector_->count() != static_cast<int>(config::kNbFilters)) {
        filter_selector_->addItem(tr("Plugin"));
    }
}

void DisplayConfig::OnFilterSelected(int index) {
    if (updating_selectors_ || index < 0) {
        return;
    }
    const auto filter = static_cast<config::Filter>(index);
    const bool is_plugin = (filter == config::Filter::kPlugin);

    // Reset the plugin selector to "None" when a built-in filter is selected.
    if (!is_plugin) {
        plugin_selector_->setCurrentIndex(0);
        OPTION(kDispFilterPlugin) = QString();
        OPTION(kDispFilter) = filter;  // applied live
    } else if (plugin_selector_->currentIndex() > 0) {
        OPTION(kDispFilterPlugin) = plugin_selector_->currentData().toString();
        OPTION(kDispFilter) = config::Filter::kPlugin;
    }
    // Plugin selected with no plugin picked yet: applied once a plugin is
    // chosen (OnPluginSelected) or on OK.
}

void DisplayConfig::OnPluginSelected(int index) {
    if (updating_selectors_) {
        return;
    }
    // When a plugin is selected, automatically set the filter to "Plugin".
    if (index > 0) {
        filter_selector_->setCurrentIndex(static_cast<int>(config::Filter::kPlugin));
        OPTION(kDispFilterPlugin) = plugin_selector_->currentData().toString();
        OPTION(kDispFilter) = config::Filter::kPlugin;
    } else if (OPTION(kDispFilter) == config::Filter::kPlugin) {
        OPTION(kDispFilterPlugin) = QString();
        OPTION(kDispFilter) = config::Filter::kNone;
        filter_selector_->setCurrentIndex(static_cast<int>(config::Filter::kNone));
    }
}

void DisplayConfig::OnPluginDirChanged(const QString& new_path) {
    // Save the new path to config.
    OPTION(kDispPluginDir) = new_path;

    // Invalidate the app-level plugin cache so hotkey cycling picks up the new dir.
    vbamApp().InvalidatePluginCache();

    // Reload the plugin list.
    PopulatePluginOptions();
}

void DisplayConfig::OnFilterChanged(config::Option* option) {
    const config::Filter option_filter = option->GetFilter();
    const bool is_plugin = (option_filter == config::Filter::kPlugin);

    // Keep the selector in sync when the option changes from elsewhere (the
    // Change Pixel Filter shortcut).
    if (isVisible() && !updating_selectors_ &&
        filter_selector_->currentIndex() != static_cast<int>(option_filter) &&
        static_cast<int>(option_filter) < filter_selector_->count()) {
        updating_selectors_ = true;
        filter_selector_->setCurrentIndex(static_cast<int>(option_filter));
        updating_selectors_ = false;
    }

    // Plugin needs to be handled separately, in case it was removed. The panel
    // will eventually reset if it can't actually use the plugin.
    QString filter_name;
    if (is_plugin) {
        const int plugin_sel = plugin_selector_->currentIndex();
        if (plugin_sel > 0) {
            // Selector entries are formatted as "<filename>: <plugin name>";
            // prefer the plugin name, falling back to the whole entry.
            const QString entry_text = plugin_selector_->itemText(plugin_sel);
            const int sep = entry_text.indexOf(QStringLiteral(": "));
            filter_name = sep >= 0 ? entry_text.mid(sep + 2) : entry_text;
        } else {
            const QString plugin_path = OPTION(kDispFilterPlugin);
            filter_name = plugin_path.isEmpty() ? tr("Plugin")
                                                : QFileInfo(plugin_path).completeBaseName();
        }
    } else {
        filter_name = widgets::FilterLabels().value(static_cast<int>(option_filter));
    }

    systemScreenMessage(tr("Using pixel filter: %1").arg(filter_name));
}

void DisplayConfig::OnInterframeChanged(config::Option* option) {
    const config::Interframe interframe = option->GetInterframe();
    if (isVisible() && interframe_selector_->currentIndex() != static_cast<int>(interframe)) {
        interframe_selector_->setCurrentIndex(static_cast<int>(interframe));
    }
    systemScreenMessage(tr("interframe blend: %1")
                            .arg(widgets::InterframeLabels().value(static_cast<int>(interframe))));
}

void DisplayConfig::SnapshotLiveOptions() {
    live_snapshot_.filter = OPTION(kDispFilter);
    live_snapshot_.interframe = OPTION(kDispIFB);
    live_snapshot_.filter_plugin = OPTION(kDispFilterPlugin);
    live_snapshot_.gba_variant = OPTION(kGBALCDFilterVariant);
    live_snapshot_.gb_variant = OPTION(kGBLCDFilterVariant);
    live_snapshot_.gba_darken = OPTION(kGBADarken);
    live_snapshot_.gb_lighten = OPTION(kGBLighten);
    live_snapshot_.profile = OPTION(kDispColorCorrectionProfile);
    live_snapshot_.profile_auto = OPTION(kDispColorCorrectionAuto);
    live_snapshot_taken_ = true;
}

void DisplayConfig::RestoreLiveOptions() {
    if (!live_snapshot_taken_) {
        return;
    }
    OPTION(kDispFilterPlugin) = live_snapshot_.filter_plugin;
    OPTION(kDispFilter) = live_snapshot_.filter;
    OPTION(kDispIFB) = live_snapshot_.interframe;
    OPTION(kGBALCDFilterVariant) = live_snapshot_.gba_variant;
    OPTION(kGBLCDFilterVariant) = live_snapshot_.gb_variant;
    OPTION(kGBADarken) = live_snapshot_.gba_darken;
    OPTION(kGBLighten) = live_snapshot_.gb_lighten;
    OPTION(kDispColorCorrectionProfile) = live_snapshot_.profile;
    OPTION(kDispColorCorrectionAuto) = live_snapshot_.profile_auto;
}

void DisplayConfig::UpdateSdlOptionsVisibility() {
    if (!sdl_options_)
        return;
    bool sdl_selected = false;
    for (const auto& radio : render_method_radios_) {
        if (radio.second == config::RenderMethod::kSDL && radio.first->isChecked())
            sdl_selected = true;
    }
    sdl_options_->setVisible(sdl_selected);
}

}  // namespace dialogs
