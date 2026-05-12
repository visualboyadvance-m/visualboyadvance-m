#include "wx/dialogs/display-config.h"

#include <memory>
#include <vector>

#include <wx/arrstr.h>
#include <wx/choice.h>
#include <wx/clntdata.h>
#include <wx/dir.h>
#include <wx/dynlib.h>
#include <wx/filepicker.h>
#include <wx/log.h>
#include <wx/notebook.h>
#include <wx/object.h>
#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/slider.h>
#include <wx/textctrl.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#endif
#include <wx/valnum.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <wx/xrc/xmlres.h>

#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/dialogs/base-dialog.h"
#include "wx/rpi.h"
#include "wx/wayland.h"
#include "wx/widgets/option-validator.h"
#include "wx/widgets/render-plugin.h"
#include "wx/wxvbam.h"

#ifdef VBAM_RPI_PROXY_SUPPORT
#include "wx/rpi-proxy/RpiProxyClient.h"
#endif

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

namespace dialogs {

namespace {

class SDLDevicesValidator : public widgets::OptionValidator {
public:
    SDLDevicesValidator() : widgets::OptionValidator(config::OptionID::kSDLRenderer) {}
    ~SDLDevicesValidator() override = default;

private:
    // OptionValidator implementation.
    wxObject* Clone() const override { return new SDLDevicesValidator(); }

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

// Custom validator for the kDispScale option.
//
// We extend wxFloatingPointValidator only for its UI plumbing (Clone(), the
// numeric-key filter); parsing and formatting of the value are handled here so
// the validator accepts both '.' and ',' as decimal separators regardless of
// the user's locale. See
// https://github.com/visualboyadvance-m/visualboyadvance-m/issues/836
class ScaleValidator : public wxFloatingPointValidator<double>,
                       public config::Option::Observer {
public:
    ScaleValidator()
        : wxFloatingPointValidator<double>(&value_),
          config::Option::Observer(config::OptionID::kDispScale),
          value_(option()->GetDouble()) {
        // Let wxFloatingPointValidator do the hard work.
        SetMin(option()->GetDoubleMin());
        SetMax(option()->GetDoubleMax());
        SetPrecision(1);
    }
    ~ScaleValidator() override = default;

    // Returns a copy of the object.
    wxObject* Clone() const override { return new ScaleValidator(); }
private:
    // wxValidator implementation.
    bool TransferFromWindow() final {
        wxTextCtrl* ctrl = wxDynamicCast(GetWindow(), wxTextCtrl);
        if (!ctrl) {
            // Not a text control? Fall back to base behavior.
            if (!wxFloatingPointValidator<double>::TransferFromWindow()) {
                wxLogWarning(_("Invalid value for Default magnification."));
                return false;
            }
            return option()->SetDouble(value_);
        }

        wxString text = ctrl->GetValue();
        text.Trim(true).Trim(false);
        if (text.empty()) {
            wxLogWarning(_("Invalid value for Default magnification."));
            return false;
        }

        // Accept either the C locale separator '.' or any system-locale
        // separator ',' . wxFloatingPointValidator's own round-trip
        // (format-with-locale -> parse-with-locale) is unreliable across
        // wxWidgets versions when the user's display locale uses ',' and
        // LC_NUMERIC disagrees -- the same string the validator wrote can
        // be rejected on parse, which silently breaks the OK button.
        double parsed = 0.0;
        bool ok = text.ToDouble(&parsed);          // locale-aware
        if (!ok) ok = text.ToCDouble(&parsed);     // C locale
        if (!ok) {
            wxString normalized = text;
            normalized.Replace(wxT(","), wxT("."));
            ok = normalized.ToCDouble(&parsed);
        }
        if (!ok) {
            wxLogWarning(_("Invalid value for Default magnification."));
            return false;
        }
        if (parsed < option()->GetDoubleMin() ||
            parsed > option()->GetDoubleMax()) {
            wxLogWarning(_("Invalid value for Default magnification."));
            return false;
        }
        value_ = parsed;
        return option()->SetDouble(value_);
    }

    bool TransferToWindow() final {
        value_ = option()->GetDouble();
        // Format with the C locale ('.') unconditionally so the field never
        // contains a separator the parse step might reject. wxString::FromCDouble
        // is locale-independent.
        wxTextCtrl* ctrl = wxDynamicCast(GetWindow(), wxTextCtrl);
        if (!ctrl) return wxFloatingPointValidator<double>::TransferToWindow();
        ctrl->ChangeValue(wxString::FromCDouble(value_, 1));
        return true;
    };

#if WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    void SetWindow(wxWindow* window) final {
        wxFloatingPointValidator<double>::SetWindow(window);
    }
#endif

    // config::Option::Observer implementation.
    void OnValueChanged() final { TransferToWindow(); }

    // A local value used for configuration.
    double value_;
};

// Validator for a wxChoice with a Filter value.
class FilterValidator : public widgets::OptionValidator {
public:
    FilterValidator() : OptionValidator(config::OptionID::kDispFilter) {}
    ~FilterValidator() override = default;

private:
    // OptionValidator implementation.
    wxObject* Clone() const override { return new FilterValidator(); }

    bool IsWindowValueValid() override { return true; }

    bool WriteToWindow() override {
        wxDynamicCast(GetWindow(), wxChoice)
            ->SetSelection(static_cast<int>(option()->GetFilter()));
        return true;
    }

    bool WriteToOption() override {
        const int selection =
            wxDynamicCast(GetWindow(), wxChoice)->GetSelection();
        if (selection == wxNOT_FOUND) {
            return false;
        }

        if (static_cast<size_t>(selection) >= option()->GetEnumMax()) {
            return false;
        }

        return option()->SetFilter(static_cast<config::Filter>(selection));
    }
};

// Validator for a wxChoice with an Interframe value.
class InterframeValidator : public widgets::OptionValidator {
public:
    InterframeValidator() : OptionValidator(config::OptionID::kDispIFB) {}
    ~InterframeValidator() override = default;

private:
    // OptionValidator implementation.
    wxObject* Clone() const override { return new InterframeValidator(); }

    bool IsWindowValueValid() override { return true; }

    bool WriteToWindow() override {
        wxDynamicCast(GetWindow(), wxChoice)
            ->SetSelection(static_cast<int>(option()->GetInterframe()));
        return true;
    }

    bool WriteToOption() override {
        const int selection =
            wxDynamicCast(GetWindow(), wxChoice)->GetSelection();
        if (selection == wxNOT_FOUND) {
            return false;
        }

        if (static_cast<size_t>(selection) >= option()->GetEnumMax()) {
            return false;
        }

        return option()->SetInterframe(
            static_cast<config::Interframe>(selection));
    }
};

// Validator for a wxRadioButton with a RenderMethod value.
class RenderValidator : public widgets::OptionValidator {
public:
    explicit RenderValidator(config::RenderMethod render_method)
        : OptionValidator(config::OptionID::kDispRenderMethod),
          render_method_(render_method) {
        VBAM_CHECK(render_method != config::RenderMethod::kLast);
    }
    ~RenderValidator() override = default;

private:
    // OptionValidator implementation.
    wxObject* Clone() const override {
        return new RenderValidator(render_method_);
    }

    bool IsWindowValueValid() override { return true; }

    bool WriteToWindow() override {
        wxDynamicCast(GetWindow(), wxRadioButton)
            ->SetValue(option()->GetRenderMethod() == render_method_);
        return true;
    }

    bool WriteToOption() override {
        if (wxDynamicCast(GetWindow(), wxRadioButton)->GetValue()) {
            return option()->SetRenderMethod(render_method_);
        }

        return true;
    }

    const config::RenderMethod render_method_;
};

// Validator for a wxRadioButton with a ColorCorrectionProfile value.
class ColorCorrectionProfileValidator : public widgets::OptionValidator {
public:
    explicit ColorCorrectionProfileValidator(config::ColorCorrectionProfile color_correction_profile)
        : OptionValidator(config::OptionID::kDispColorCorrectionProfile),
          color_correction_profile_(color_correction_profile) {
        VBAM_CHECK(color_correction_profile != config::ColorCorrectionProfile::kLast);
    }
    ~ColorCorrectionProfileValidator() override = default;

private:
    // OptionValidator implementation.
    wxObject* Clone() const override {
        return new ColorCorrectionProfileValidator(color_correction_profile_);
    }

    bool IsWindowValueValid() override { return true; }

    bool WriteToWindow() override {
        wxDynamicCast(GetWindow(), wxRadioButton)
            ->SetValue(option()->GetColorCorrectionProfile() == color_correction_profile_);
        return true;
    }

    bool WriteToOption() override {
        if (wxDynamicCast(GetWindow(), wxRadioButton)->GetValue()) {
            return option()->SetColorCorrectionProfile(color_correction_profile_);
        }

        return true;
    }

    const config::ColorCorrectionProfile color_correction_profile_;
};

class PluginSelectorValidator : public widgets::OptionValidator {
public:
    PluginSelectorValidator()
        : widgets::OptionValidator(config::OptionID::kDispFilterPlugin) {}
    ~PluginSelectorValidator() override = default;

private:
    // widgets::OptionValidator implementation.
    wxObject* Clone() const override { return new PluginSelectorValidator(); }

    bool IsWindowValueValid() override { return true; }

    bool WriteToWindow() override {
        wxChoice* plugin_selector = wxDynamicCast(GetWindow(), wxChoice);
        VBAM_CHECK(plugin_selector);
        const wxString selected_plugin = option()->GetString();
        for (size_t i = 0; i < plugin_selector->GetCount(); i++) {
            const wxString& plugin_data =
                dynamic_cast<wxStringClientData*>(
                    plugin_selector->GetClientObject(i))
                    ->GetData();
            if (plugin_data == selected_plugin) {
                plugin_selector->SetSelection(i);
                return true;
            }
        }
        return false;
    }

    bool WriteToOption() override {
        wxChoice* plugin_selector = wxDynamicCast(GetWindow(), wxChoice);
        VBAM_CHECK(plugin_selector);
        const wxString& selected_window_plugin =
            dynamic_cast<wxStringClientData*>(
                plugin_selector->GetClientObject(
                    plugin_selector->GetSelection()))
                ->GetData();
        bool result = option()->SetString(selected_window_plugin);

        // Also set the filter type to Plugin when a plugin is selected
        if (!selected_window_plugin.empty()) {
            OPTION(kDispFilter) = config::Filter::kPlugin;
        }
        return result;
    }
};

}  // namespace

// Helper function to update slider tooltip with its current value
static void UpdateSliderTooltip(wxSlider* slider, wxCommandEvent& event) {
    if (slider) {
        slider->SetToolTip(wxString::Format("%d", slider->GetValue()));
    }
    event.Skip();
}

// Helper function to update slider tooltip on mouse enter
static void UpdateSliderTooltipOnHover(wxSlider* slider, wxMouseEvent& event) {
    if (slider) {
        slider->SetToolTip(wxString::Format("%d", slider->GetValue()));
    }
    event.Skip();
}

// static
DisplayConfig* DisplayConfig::NewInstance(wxWindow* parent) {
    VBAM_CHECK(parent);
    return new DisplayConfig(parent);
}

DisplayConfig::DisplayConfig(wxWindow* parent)
    : BaseDialog(parent, "DisplayConfig"),
      tab_loaded_(kTabCount, false),
      filter_observer_(config::OptionID::kDispFilter,
                       std::bind(&DisplayConfig::OnFilterChanged,
                                 this,
                                 std::placeholders::_1)),
      interframe_observer_(config::OptionID::kDispIFB,
                           std::bind(&DisplayConfig::OnInterframeChanged,
                                     this,
                                     std::placeholders::_1)) {
    notebook_ = GetValidatedChild<wxNotebook>("DisplayConfigNotebook");

    Bind(wxEVT_SHOW, &DisplayConfig::OnDialogShowEvent, this, GetId());
}

bool DisplayConfig::LoadLazyTab(int index) {
    if (index < 0 || index >= kTabCount || tab_loaded_[index]) {
        return false;
    }

    // Tabs must be added in order (0, 1, 2, ...) because we use
    // wxNotebook::AddPage. The preload queue and OnFirstShow both load in
    // ascending order, so this holds.
    if (index != static_cast<int>(notebook_->GetPageCount())) {
        // Out-of-order request: load any earlier missing tabs first.
        for (int i = 0; i < index; ++i) {
            if (!tab_loaded_[i]) {
                LoadLazyTab(i);
            }
        }
    }

    struct TabSpec {
        const wxChar* xrc_name;
        const wxChar* tab_label;
        void (DisplayConfig::*init)();
    };
    static const TabSpec kSpecs[kTabCount] = {
        {wxT("DisplayConfigBasicPanel"),           wxT("Basic"),           &DisplayConfig::InitBasicTab},
        {wxT("DisplayConfigBitDepthPanel"),        wxT("Bit Depth"),       &DisplayConfig::InitBitDepthTab},
        {wxT("DisplayConfigColorCorrectionPanel"), wxT("Color Correction"),&DisplayConfig::InitColorCorrectionTab},
        {wxT("DisplayConfigSpeedPanel"),           wxT("Speed"),           &DisplayConfig::InitSpeedTab},
        {wxT("DisplayConfigOSDPanel"),             wxT("On-Screen Display"),&DisplayConfig::InitOSDTab},
        {wxT("DisplayConfigZoomPanel"),            wxT("Zoom"),            &DisplayConfig::InitZoomTab},
    };

    const TabSpec& spec = kSpecs[index];
    wxPanel* panel = wxXmlResource::Get()->LoadPanel(notebook_, spec.xrc_name);
    if (!panel) {
        wxLogError(_("Failed to load DisplayConfig tab '%s'"), spec.xrc_name);
        return false;
    }
    notebook_->AddPage(panel, wxGetTranslation(spec.tab_label));
    tab_loaded_[index] = true;
    (this->*spec.init)();

    // Re-fit: page count just changed.
    Fit();
    return true;
}

void DisplayConfig::InitBasicTab() {
    // Render-method radio buttons.
    wxWindow* render_method = GetValidatedChild("OutputSimple");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kSimple));

    render_method = GetValidatedChild("OutputSDL");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kSDL));

#if defined(__WXMAC__)
    render_method = GetValidatedChild("OutputQuartz2D");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kQuartz2d));
#ifndef NO_METAL
    render_method = GetValidatedChild("OutputMetal");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kMetal));
#else
    GetValidatedChild("OutputMetal")->Hide();
#endif
#else
    GetValidatedChild("OutputQuartz2D")->Hide();
    GetValidatedChild("OutputMetal")->Hide();
#endif

#ifdef NO_OGL
    GetValidatedChild("OutputOpenGL")->Hide();
#elif defined(HAVE_WAYLAND_SUPPORT) && !defined(HAVE_WAYLAND_EGL)
    if (IsWayland()) {
        GetValidatedChild("OutputOpenGL")->Hide();
    } else {
        render_method = GetValidatedChild("OutputOpenGL");
        render_method->SetValidator(RenderValidator(config::RenderMethod::kOpenGL));
    }
#else
    render_method = GetValidatedChild("OutputOpenGL");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kOpenGL));
#endif

#if defined(__WXMSW__) && !defined(NO_D3D)
    render_method = GetValidatedChild("OutputDirect3D");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kDirect3d));
#else
    GetValidatedChild("OutputDirect3D")->Hide();
#endif

#if defined(__WXMSW__) && !defined(NO_D3D12)
    render_method = GetValidatedChild("OutputDirect3D12");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kDirect3d12));
#else
    GetValidatedChild("OutputDirect3D12")->Hide();
#endif

#ifndef NO_VULKAN
    render_method = GetValidatedChild("OutputVulkan");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kVulkan));
#else
    GetValidatedChild("OutputVulkan")->Hide();
#endif

    sdlrenderer_label_ = GetValidatedChild<wxControl>("SDLRendererLab");
    sdlrenderer_selector_ = GetValidatedChild<wxChoice>("SDLRenderer");
    sdlrenderer_selector_->SetValidator(SDLDevicesValidator());

    sdlpixelart_checkbox_ = GetValidatedChild<wxControl>("SDLPixelArt");
#if !defined(ENABLE_SDL3) || !defined(HAVE_SDL3_PIXELART)
    sdlpixelart_checkbox_->Hide();
#else
    wxDynamicCast(sdlpixelart_checkbox_, wxCheckBox)
        ->SetValidator(
            widgets::OptionBoolValidator(config::OptionID::kDispSDLPixelArt));
#endif

    // Bind event handlers to all output module radio buttons.
    GetValidatedChild("OutputSimple")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
    GetValidatedChild("OutputSDL")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
#ifdef __WXMAC__
    GetValidatedChild("OutputQuartz2D")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
#ifndef NO_METAL
    GetValidatedChild("OutputMetal")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
#endif
#endif
#ifndef NO_OGL
#if defined(HAVE_WAYLAND_SUPPORT) && !defined(HAVE_WAYLAND_EGL)
    if (!IsWayland()) {
        GetValidatedChild("OutputOpenGL")->Bind(wxEVT_RADIOBUTTON,
            &DisplayConfig::UpdateSDLOptionsVisibility, this);
    }
#else
    GetValidatedChild("OutputOpenGL")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
#endif
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
    GetValidatedChild("OutputDirect3D")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
#endif

    // Filter / plugin selectors.
    filter_selector_ = GetValidatedChild<wxChoice>("Filter");
    filter_selector_->SetValidator(FilterValidator());
    filter_selector_->Bind(wxEVT_CHOICE, &DisplayConfig::UpdatePlugin, this);

    plugin_dir_label_ = GetValidatedChild<wxControl>("PluginDirLab");
    plugin_dir_picker_ = GetValidatedChild<wxDirPickerCtrl>("PluginDir");
    plugin_label_ = GetValidatedChild<wxControl>("PluginLab");
    plugin_selector_ = GetValidatedChild<wxChoice>("Plugin");
    plugin_selector_->Bind(wxEVT_CHOICE, &DisplayConfig::OnPluginSelected, this);

    const wxString config_plugin_dir = OPTION(kDispPluginDir);
    if (!config_plugin_dir.empty()) {
        plugin_dir_picker_->SetPath(config_plugin_dir);
    } else {
        plugin_dir_picker_->SetPath(wxGetApp().GetPluginsDir());
    }

    plugin_dir_picker_->Bind(wxEVT_DIRPICKER_CHANGED, &DisplayConfig::OnPluginDirChanged, this);

    interframe_selector_ = GetValidatedChild<wxChoice>("IFB");
    interframe_selector_->SetValidator(InterframeValidator());
}

void DisplayConfig::InitBitDepthTab() {
    GetValidatedChild("BitDepth")
        ->SetValidator(
            widgets::OptionChoiceValidator(config::OptionID::kBitDepth));
}

void DisplayConfig::InitColorCorrectionTab() {
    wxWindow* color_profile_srgb = GetValidatedChild("ColorProfileSRGB");
    color_profile_srgb->SetValidator(
        ColorCorrectionProfileValidator(config::ColorCorrectionProfile::kSRGB));

    wxWindow* color_profile_dci = GetValidatedChild("ColorProfileDCI");
    color_profile_dci->SetValidator(
        ColorCorrectionProfileValidator(config::ColorCorrectionProfile::kDCI));

    wxWindow* color_profile_rec2020 = GetValidatedChild("ColorProfileRec2020");
    color_profile_rec2020->SetValidator(
        ColorCorrectionProfileValidator(config::ColorCorrectionProfile::kRec2020));

    switch (OPTION(kDispColorCorrectionProfile)) {
        case config::ColorCorrectionProfile::kSRGB:
            wxDynamicCast(color_profile_srgb, wxRadioButton)->SetValue(true);
            break;
        case config::ColorCorrectionProfile::kDCI:
            wxDynamicCast(color_profile_dci, wxRadioButton)->SetValue(true);
            break;
        case config::ColorCorrectionProfile::kRec2020:
            wxDynamicCast(color_profile_rec2020, wxRadioButton)->SetValue(true);
            break;
        default:
            wxLogError(_("Invalid color correction profile"));
    }

    wxSlider* gba_darken_slider = GetValidatedChild<wxSlider>("GBADarken");
    gba_darken_slider->SetValidator(widgets::OptionUnsignedValidator(config::OptionID::kGBADarken));
    gba_darken_slider->SetToolTip(wxString::Format("%d", gba_darken_slider->GetValue()));
    gba_darken_slider->Bind(wxEVT_SLIDER, std::bind(UpdateSliderTooltip, gba_darken_slider, std::placeholders::_1));
    gba_darken_slider->Bind(wxEVT_ENTER_WINDOW, std::bind(UpdateSliderTooltipOnHover, gba_darken_slider, std::placeholders::_1));

    wxSlider* gbc_lighten_slider = GetValidatedChild<wxSlider>("GBCLighten");
    gbc_lighten_slider->SetValidator(widgets::OptionUnsignedValidator(config::OptionID::kGBLighten));
    gbc_lighten_slider->SetToolTip(wxString::Format("%d", gbc_lighten_slider->GetValue()));
    gbc_lighten_slider->Bind(wxEVT_SLIDER, std::bind(UpdateSliderTooltip, gbc_lighten_slider, std::placeholders::_1));
    gbc_lighten_slider->Bind(wxEVT_ENTER_WINDOW, std::bind(UpdateSliderTooltipOnHover, gbc_lighten_slider, std::placeholders::_1));
}

void DisplayConfig::InitSpeedTab() {
    GetValidatedChild("FrameSkip")
        ->SetValidator(
            widgets::OptionIntValidator(config::OptionID::kPrefFrameSkip));
}

void DisplayConfig::InitOSDTab() {
    GetValidatedChild("SpeedIndicator")
        ->SetValidator(
            widgets::OptionChoiceValidator(config::OptionID::kPrefShowSpeed));
}

void DisplayConfig::InitZoomTab() {
    GetValidatedChild("DefaultScale")->SetValidator(ScaleValidator());
    GetValidatedChild("MaxScale")
        ->SetValidator(wxGenericValidator(&gopts.max_scale));
}

void DisplayConfig::OnDialogShowEvent(wxShowEvent& event) {
    wxCommandEvent dummy_event;

    if (event.IsShown()) {
        PopulatePluginOptions();

        // Set initial SDL options visibility based on current selection
        wxRadioButton* sdl_button = wxDynamicCast(GetValidatedChild("OutputSDL"), wxRadioButton);
        if (sdl_button && sdl_button->GetValue()) {
            ShowSDLOptions();
            // Only fill renderer list when SDL output is selected
            // FillRendererList calls SDL_Init which can corrupt OpenGL rendering
            FillRendererList(dummy_event);
        } else {
            HideSDLOptions();
        }

        Fit();
    } else {
        StopPluginHandler();
    }

    // Let the event propagate.
    event.Skip();
}

void DisplayConfig::PopulatePluginOptions() {
    // Populate the plugin values, if any.
    wxArrayString plugins;
    const wxString plugin_path = wxGetApp().GetPluginsDir();

    wxDir::GetAllFiles(plugin_path, &plugins, "*.rpi",
                       wxDIR_FILES | wxDIR_DIRS);

    if (plugins.empty()) {
        HidePluginOptions();
        Layout();
        Fit();
        return;
    }

    plugin_selector_->Clear();
    plugin_selector_->Append(_("None"), new wxStringClientData());

    const wxString selected_plugin = OPTION(kDispFilterPlugin);
    bool is_plugin_selected = false;

    // Maps plugin path -> name for all valid plugins (direct + proxy)
    struct PluginEntry {
        wxString path;
        wxString name;
    };
    std::vector<PluginEntry> valid_entries;

#ifdef VBAM_RPI_PROXY_SUPPORT
    // Collect 32-bit plugins that need proxy validation
    std::vector<wxString> proxy_paths;
#endif

    for (const wxString& plugin : plugins) {
        // First try direct loading (works for same-architecture plugins)
        wxDynamicLibrary filter_plugin;
        const RENDER_PLUGIN_INFO* plugin_info =
            widgets::MaybeLoadFilterPlugin(plugin, &filter_plugin);
        if (plugin_info) {
            valid_entries.push_back({plugin, wxString(plugin_info->Name, wxConvUTF8)});
            continue;
        }

#ifdef VBAM_RPI_PROXY_SUPPORT
        if (widgets::PluginNeedsProxy(plugin)) {
            proxy_paths.push_back(plugin);
        }
#endif
    }

#ifdef VBAM_RPI_PROXY_SUPPORT
    // Batch validate all 32-bit plugins in a single IPC round trip
    if (!proxy_paths.empty()) {
        rpi_proxy::RpiProxyClient enumeration_proxy;
        auto results = enumeration_proxy.EnumeratePlugins(proxy_paths);
        for (const auto& r : results) {
            if (r.valid) {
                valid_entries.push_back({r.path, r.name});
            }
        }
    }
#endif

    for (const auto& entry : valid_entries) {
        wxFileName file_name(entry.path);
        file_name.MakeRelativeTo(plugin_path);

        const int added_index = plugin_selector_->Append(
            file_name.GetName() + ": " + entry.name,
            new wxStringClientData(entry.path));

        if (entry.path == selected_plugin) {
            plugin_selector_->SetSelection(added_index);
            is_plugin_selected = true;
        }
    }

    if (plugin_selector_->GetCount() == 1u) {
        wxLogWarning(wxString::Format(_("No usable rpi plugins found in %s"),
                                      plugin_path));
        HidePluginOptions();
        Layout();
        Fit();
        return;
    }

    if (!is_plugin_selected) {
        OPTION(kDispFilterPlugin) = wxEmptyString;
    }

    plugin_selector_->SetValidator(PluginSelectorValidator());
    ShowPluginOptions();
    Layout();
    Fit();
}

void DisplayConfig::StopPluginHandler() {
    // Reset the validator to stop handling events while this dialog is hidden.
    plugin_selector_->SetValidator(wxValidator());
}

void DisplayConfig::UpdatePlugin(wxCommandEvent& event) {
    const bool is_plugin = (static_cast<config::Filter>(event.GetSelection()) ==
                            config::Filter::kPlugin);

    // Reset plugin selector to "None" when a built-in filter is selected
    // This only affects the UI; config is written by validators on dialog close
    if (!is_plugin) {
        plugin_selector_->SetSelection(0);  // 0 = "None" in GUI
    }

    // Let the event propagate.
    event.Skip();
}

void DisplayConfig::OnPluginSelected(wxCommandEvent& event) {
    // When a plugin is selected, automatically set Filter to "Plugin"
    // Check if selection is not "None" (index 0)
    if (plugin_selector_->GetSelection() > 0) {
        filter_selector_->SetSelection(static_cast<int>(config::Filter::kPlugin));
    }

    // Let the event propagate.
    event.Skip();
}

void DisplayConfig::OnFilterChanged(config::Option* option) {
    // The Basic tab owns the Filter/Plugin selectors. The observer can fire
    // before that tab has been lazy-loaded (e.g. another part of the app
    // mutates kDispFilter while we're still preloading).
    if (!filter_selector_ || !plugin_selector_) {
        return;
    }

    const config::Filter option_filter = option->GetFilter();
    const bool is_plugin = (option_filter == config::Filter::kPlugin);

    // Plugin needs to be handled separately, in case it was removed. The panel
    // will eventually reset if it can't actually use the plugin.
    wxString filter_name;
    if (is_plugin) {
        const int plugin_sel = plugin_selector_->GetSelection();
        if (plugin_sel > 0) {
            // Selector entries are formatted as "<filename>: <plugin name>";
            // prefer the plugin name, falling back to the whole entry.
            const wxString entry_text = plugin_selector_->GetString(plugin_sel);
            const size_t sep = entry_text.find(wxT(": "));
            if (sep != wxString::npos) {
                filter_name = entry_text.substr(sep + 2);
            } else {
                filter_name = entry_text;
            }
        } else {
            // Selector isn't populated yet (dialog never shown). Fall back to
            // the file name from the configured plugin path.
            const wxString plugin_path = OPTION(kDispFilterPlugin);
            if (!plugin_path.empty()) {
                filter_name = wxFileName(plugin_path).GetName();
            } else {
                filter_name = _("Plugin");
            }
        }
    } else {
        filter_name =
            filter_selector_->GetString(static_cast<size_t>(option_filter));
    }

    systemScreenMessage(
        wxString::Format(_("Using pixel filter: %s"), filter_name));
}

void DisplayConfig::OnInterframeChanged(config::Option* option) {
    if (!interframe_selector_) {
        return;
    }
    const config::Interframe interframe = option->GetInterframe();

    systemScreenMessage(wxString::Format(
        _("interframe blend: %s"),
        interframe_selector_->GetString(static_cast<size_t>(interframe))));
}

void DisplayConfig::FillRendererList(wxCommandEvent& event) {
#ifndef ENABLE_SDL3
    SDL_RendererInfo render_info;
#endif
    int num_drivers = 0;

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_Init(SDL_INIT_VIDEO);

    num_drivers = SDL_GetNumRenderDrivers();

    sdlrenderer_selector_->Clear();
    sdlrenderer_selector_->Append("default", new wxStringClientData("default"));
    sdlrenderer_selector_->SetSelection(0);

    for (int i = 0; i < num_drivers; i++) {
#ifdef ENABLE_SDL3
        sdlrenderer_selector_->Append(SDL_GetRenderDriver(i), new wxStringClientData(SDL_GetRenderDriver(i)));

        if (OPTION(kSDLRenderer) == wxString(SDL_GetRenderDriver(i))) {
            sdlrenderer_selector_->SetSelection(i+1);
        }
#else
        SDL_GetRenderDriverInfo(i, &render_info);

        sdlrenderer_selector_->Append(render_info.name, new wxStringClientData(render_info.name));

        if (OPTION(kSDLRenderer) == wxString(render_info.name)) {
            sdlrenderer_selector_->SetSelection(i+1);
        }
#endif
    }

    // Let the event propagate.
    event.Skip();
}

void DisplayConfig::HidePluginOptions() {
    // Plugin directory picker is always visible so users can set the path
    plugin_label_->Hide();
    plugin_selector_->Hide();

    // Remove the Plugin option, which should be the last.
    if (filter_selector_->GetCount() == config::kNbFilters) {
        // Make sure we have not selected the plugin option. The validator
        // will take care of updating the selector value.
        if (OPTION(kDispFilter) == config::Filter::kPlugin) {
            OPTION(kDispFilter) = config::Filter::kNone;
        }
        filter_selector_->Delete(config::kNbFilters - 1);
    }

    // Also erase the Plugin value to avoid issues down the line.
    OPTION(kDispFilterPlugin) = wxEmptyString;
}

void DisplayConfig::ShowPluginOptions() {
    // Plugin directory picker is always visible
    plugin_label_->Show();
    plugin_selector_->Show();

    // Re-add the Plugin option, if needed.
    if (filter_selector_->GetCount() != config::kNbFilters) {
        filter_selector_->Append(_("Plugin"));
    }
}

void DisplayConfig::OnPluginDirChanged(wxFileDirPickerEvent& event) {
    const wxString& new_path = event.GetPath();

    // Save the new path to config
    OPTION(kDispPluginDir) = new_path;

    // Invalidate the app-level plugin cache so hotkey cycling picks up the new dir
    wxGetApp().InvalidatePluginCache();

    // Reload the plugin list
    PopulatePluginOptions();

    // Let the event propagate.
    event.Skip();
}

void DisplayConfig::HideSDLOptions() {
    sdlrenderer_label_->Hide();
    sdlrenderer_selector_->Hide();
    sdlpixelart_checkbox_->Hide();
}

void DisplayConfig::ShowSDLOptions() {
    sdlrenderer_label_->Show();
    sdlrenderer_selector_->Show();
#if defined(ENABLE_SDL3) && defined(HAVE_SDL3_PIXELART)
    sdlpixelart_checkbox_->Show();
#endif
}

void DisplayConfig::UpdateSDLOptionsVisibility(wxCommandEvent& event) {
    // Check if SDL output module is selected
    wxRadioButton* sdl_button = wxDynamicCast(GetValidatedChild("OutputSDL"), wxRadioButton);
    if (sdl_button && sdl_button->GetValue()) {
        ShowSDLOptions();
    } else {
        HideSDLOptions();
    }

#ifdef __WXMAC__
    Layout();
#else
    Fit();
#endif

    // Let the event propagate
    event.Skip();
}

}  // namespace dialogs