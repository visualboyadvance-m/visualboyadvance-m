#include "wx/dialogs/display-config.h"

#include <wx/arrstr.h>
#include <wx/choice.h>
#include <wx/clntdata.h>
#include <wx/dir.h>
#include <wx/dynlib.h>
#include <wx/log.h>
#include <wx/object.h>
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

// Custom validator for the kDispScale option. We rely on the existing
// wxFloatingPointValidator validator for this.
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
        if (!wxFloatingPointValidator<double>::TransferFromWindow()) {
            wxLogWarning(_("Invalid value for Default magnification."));
            return false;
        }
        return option()->SetDouble(value_);
    }

    bool TransferToWindow() final {
        value_ = option()->GetDouble();
        return wxFloatingPointValidator<double>::TransferToWindow();
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
        return option()->SetString(selected_window_plugin);
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
      filter_observer_(config::OptionID::kDispFilter,
                       std::bind(&DisplayConfig::OnFilterChanged,
                                 this,
                                 std::placeholders::_1)),
      interframe_observer_(config::OptionID::kDispIFB,
                           std::bind(&DisplayConfig::OnInterframeChanged,
                                     this,
                                     std::placeholders::_1)) {
    GetValidatedChild("BitDepth")
        ->SetValidator(
            widgets::OptionChoiceValidator(config::OptionID::kBitDepth));

    // Speed
    GetValidatedChild("FrameSkip")
        ->SetValidator(
            widgets::OptionIntValidator(config::OptionID::kPrefFrameSkip));

    // On-Screen Display
    GetValidatedChild("SpeedIndicator")
        ->SetValidator(
            widgets::OptionChoiceValidator(config::OptionID::kPrefShowSpeed));

    // Zoom
    GetValidatedChild("DefaultScale")->SetValidator(ScaleValidator());

    // this was a choice, but I'd rather not have to make an off-by-one
    // validator just for this, and spinctrl is good enough.
    GetValidatedChild("MaxScale")
        ->SetValidator(wxGenericValidator(&gopts.max_scale));

    // Basic
    wxWindow *render_method = GetValidatedChild("OutputSimple");
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
    // wxGLCanvas segfaults on Wayland before wx 3.2.
    if (IsWayland()) {
        GetValidatedChild("OutputOpenGL")->Hide();
    } else {
        render_method = GetValidatedChild("OutputOpenGL");
        render_method->SetValidator(RenderValidator(config::RenderMethod::kOpenGL));
    }
#else
    render_method = GetValidatedChild("OutputOpenGL");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kOpenGL));
#endif  // NO_OGL

#if defined(__WXMSW__) && !defined(NO_D3D)
    // Enable the Direct3D option on Windows.
    render_method = GetValidatedChild("OutputDirect3D");
    render_method->SetValidator(RenderValidator(config::RenderMethod::kDirect3d));
#else
    GetValidatedChild("OutputDirect3D")->Hide();
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

    // Bind event handlers to all output module radio buttons
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

    filter_selector_ = GetValidatedChild<wxChoice>("Filter");
    filter_selector_->SetValidator(FilterValidator());
    filter_selector_->Bind(wxEVT_CHOICE, &DisplayConfig::UpdatePlugin, this,
                           GetId());

    // These are filled and/or hidden at dialog load time.
    plugin_label_ = GetValidatedChild<wxControl>("PluginLab");
    plugin_selector_ = GetValidatedChild<wxChoice>("Plugin");

    interframe_selector_ = GetValidatedChild<wxChoice>("IFB");
    interframe_selector_->SetValidator(InterframeValidator());

    Bind(wxEVT_SHOW, &DisplayConfig::OnDialogShowEvent, this, GetId());

    // Finally, fit everything nicely.
    Fit();
}

void DisplayConfig::OnDialogShowEvent(wxShowEvent& event) {
    wxCommandEvent dummy_event;

    if (event.IsShown()) {
        PopulatePluginOptions();

        // Set initial SDL options visibility based on current selection
        wxRadioButton* sdl_button = wxDynamicCast(GetValidatedChild("OutputSDL"), wxRadioButton);
        if (sdl_button && sdl_button->GetValue()) {
            ShowSDLOptions();
        } else {
            HideSDLOptions();
        }
    } else {
        StopPluginHandler();
    }

    FillRendererList(dummy_event);

    Fit();

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
        return;
    }

    plugin_selector_->Clear();
    plugin_selector_->Append(_("None"), new wxStringClientData());

    const wxString selected_plugin = OPTION(kDispFilterPlugin);
    bool is_plugin_selected = false;

    for (const wxString& plugin : plugins) {
        wxDynamicLibrary dyn_lib(plugin, wxDL_VERBATIM | wxDL_NOW);

        wxDynamicLibrary filter_plugin;
        const RENDER_PLUGIN_INFO* plugin_info =
            widgets::MaybeLoadFilterPlugin(plugin, &filter_plugin);
        if (!plugin_info) {
            continue;
        }

        wxFileName file_name(plugin);
        file_name.MakeRelativeTo(plugin_path);

        const int added_index = plugin_selector_->Append(
            file_name.GetName() + ": " +
                wxString(plugin_info->Name, wxConvUTF8),
            new wxStringClientData(plugin));

        if (plugin == selected_plugin) {
            plugin_selector_->SetSelection(added_index);
            is_plugin_selected = true;
        }
    }

    if (plugin_selector_->GetCount() == 1u) {
        wxLogWarning(wxString::Format(_("No usable rpi plugins found in %s"),
                                      plugin_path));
        HidePluginOptions();
        return;
    }

    if (!is_plugin_selected) {
        OPTION(kDispFilterPlugin) = wxEmptyString;
    }

    plugin_selector_->SetValidator(PluginSelectorValidator());
    ShowPluginOptions();
}

void DisplayConfig::StopPluginHandler() {
    // Reset the validator to stop handling events while this dialog is hidden.
    plugin_selector_->SetValidator(wxValidator());
}

void DisplayConfig::UpdatePlugin(wxCommandEvent& event) {
    const bool is_plugin = (static_cast<config::Filter>(event.GetSelection()) ==
                            config::Filter::kPlugin);
    plugin_label_->Enable(is_plugin);
    plugin_selector_->Enable(is_plugin);

    // Let the event propagate.
    event.Skip();
}

void DisplayConfig::OnFilterChanged(config::Option* option) {
    const config::Filter option_filter = option->GetFilter();
    const bool is_plugin = (option_filter == config::Filter::kPlugin);

    // Plugin needs to be handled separately, in case it was removed. The panel
    // will eventually reset if it can't actually use the plugin.
    wxString filter_name;
    if (is_plugin) {
        filter_name = _("Plugin");
    } else {
        filter_name =
            filter_selector_->GetString(static_cast<size_t>(option_filter));
    }

    systemScreenMessage(
        wxString::Format(_("Using pixel filter: %s"), filter_name));
}

void DisplayConfig::OnInterframeChanged(config::Option* option) {
    const config::Interframe interframe = option->GetInterframe();

    systemScreenMessage(wxString::Format(
        _("Using interframe blending: %s"),
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
    plugin_label_->Show();
    plugin_selector_->Show();

    // Re-add the Plugin option, if needed.
    if (filter_selector_->GetCount() != config::kNbFilters) {
        filter_selector_->Append(_("Plugin"));
    }
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

    Fit();

    // Let the event propagate
    event.Skip();
}

}  // namespace dialogs
