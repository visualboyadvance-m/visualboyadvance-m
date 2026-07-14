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

#if defined(HAVE_WAYLAND_SUPPORT)
#include <gdk/gdkwayland.h>
#include <wayland-client.h>
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
    // Render-method radio buttons. Record each radio that actually exists on
    // this build/platform so UpdateRenderMethodVisibility() can show/hide just
    // those (the statically #ifdef-hidden ones below are never recorded, so they
    // stay hidden).
#if defined(__WXMSW__)
    // On a machine with no hardware GPU only the Simple (software) renderer
    // works; every GPU-backed method fails to initialize. Offer only the Simple
    // radio there. Radios that aren't recorded below stay hidden, since
    // UpdateRenderMethodVisibility() only ever touches recorded ones.
    const bool gpu_absent = !VbamWindowsHasHardwareGpu();
#else
    const bool gpu_absent = false;
#endif

    render_method_radios_.clear();
    auto add_radio = [this, gpu_absent](const char* name, config::RenderMethod m) {
        if (gpu_absent && m != config::RenderMethod::kSimple) {
            GetValidatedChild(name)->Hide();
            return;
        }
        GetValidatedChild(name)->SetValidator(RenderValidator(m));
        render_method_radios_.push_back({name, m});
    };

    add_radio("OutputSimple", config::RenderMethod::kSimple);
    add_radio("OutputSDL", config::RenderMethod::kSDL);

    // Quartz2D is no longer a separate choice: on macOS the Simple renderer is
    // backed by the Quartz2D driver automatically, so there is no OutputQuartz2D
    // radio anymore.
#if defined(__WXMAC__) && !defined(NO_METAL)
    add_radio("OutputMetal", config::RenderMethod::kMetal);
#else
    GetValidatedChild("OutputMetal")->Hide();
#endif

#ifdef NO_OGL
    GetValidatedChild("OutputOpenGL")->Hide();
#elif defined(HAVE_WAYLAND_SUPPORT) && !defined(HAVE_WAYLAND_EGL)
    if (IsWayland()) {
        GetValidatedChild("OutputOpenGL")->Hide();
    } else {
        add_radio("OutputOpenGL", config::RenderMethod::kOpenGL);
    }
#else
    add_radio("OutputOpenGL", config::RenderMethod::kOpenGL);
#endif

#if defined(__WXMSW__) && !defined(NO_D3D)
    add_radio("OutputDirect3D", config::RenderMethod::kDirect3d);
#else
    GetValidatedChild("OutputDirect3D")->Hide();
#endif

#if defined(__WXMSW__) && !defined(NO_D3D12)
    // Direct3D 12 needs Windows 10+. On older Windows the renderer is disabled
    // (it falls back to Direct3D 9), so hide the radio there so it can't be
    // picked -- mirroring the run-time gating of the Vulkan radio below.
    if (VbamWindowsIsWin10OrGreater())
        add_radio("OutputDirect3D12", config::RenderMethod::kDirect3d12);
    else
        GetValidatedChild("OutputDirect3D12")->Hide();
#else
    GetValidatedChild("OutputDirect3D12")->Hide();
#endif

#ifndef NO_VULKAN
    // Offer Vulkan only when its loader is actually present at run time (it is
    // delay-loaded on Windows and may be absent). When it is not, hide the radio
    // so it cannot be selected; the renderer fallbacks skip it too.
    if (VbamVulkanRuntimeAvailable())
        add_radio("OutputVulkan", config::RenderMethod::kVulkan);
    else
        GetValidatedChild("OutputVulkan")->Hide();
#else
    GetValidatedChild("OutputVulkan")->Hide();
#endif

    // Hide renderers that can't present the active HDR / 10-bit deep-color
    // feature (and re-show them when it is turned off). Recomputed on every
    // HDR/deep-color toggle, so nothing stays hidden once the user unchecks.
    UpdateRenderMethodVisibility();

    sdlrenderer_label_ = GetValidatedChild<wxControl>("SDLRendererLab");
    sdlrenderer_selector_ = GetValidatedChild<wxChoice>("SDLRenderer");
    sdlrenderer_selector_->SetValidator(SDLDevicesValidator());

    sdlpixelart_checkbox_ = GetValidatedChild<wxControl>("SDLPixelArt");
    // The SDL3 pixel-art scaler is disabled until further investigation: hide the
    // checkbox unconditionally and do not bind it to the option, regardless of
    // ENABLE_SDL3 / HAVE_SDL3_PIXELART support.
    sdlpixelart_checkbox_->Hide();

    // Bind event handlers to all output module radio buttons.
    GetValidatedChild("OutputSimple")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
    GetValidatedChild("OutputSDL")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
#if defined(__WXMAC__) && !defined(NO_METAL)
    GetValidatedChild("OutputMetal")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
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
#if defined(__WXMSW__) && !defined(NO_D3D12)
    GetValidatedChild("OutputDirect3D12")->Bind(wxEVT_RADIOBUTTON,
        &DisplayConfig::UpdateSDLOptionsVisibility, this);
#endif
#ifndef NO_VULKAN
    GetValidatedChild("OutputVulkan")->Bind(wxEVT_RADIOBUTTON,
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

    // HDR / deep-color controls. HDR is for the HDR-capable platforms; X11
    // (Linux/BSD) gets the 10-bit deep-color SDR option instead.
    wxCheckBox*   hdr_check    = GetValidatedChild<wxCheckBox>("HDR");
    wxStaticText* hdr_warning  = GetValidatedChild<wxStaticText>("HDRNoEffectWarning");
    wxCheckBox*   deep_check   = GetValidatedChild<wxCheckBox>("DeepColor");
    wxStaticText* deep_warning = GetValidatedChild<wxStaticText>("DeepColorNoEffectWarning");

    bool is_x11 = false;
#ifdef __WXGTK__
    is_x11 = !IsWayland();
#endif

    // HDR vs 10-bit deep color: X11 (Linux/BSD) has no per-window HDR, so it gets
    // the 10-bit deep-color SDR toggle; the HDR-capable platforms get the HDR
    // toggle. Exactly one is shown.
    //
    // Both toggles are two-state: checked = "auto" (honor it whenever the display
    // and a renderer can present it), unchecked = "off". The default is auto.
    // Nothing turns the option off automatically -- a renderer that cannot present
    // the feature falls back at runtime but leaves the option in auto, so the
    // checkbox stays checked and a warning is shown. Only the user unchecking it
    // sets it off (and the warning then hides); re-checking returns it to auto and
    // re-probes.
    //
    // The checkbox is shown only when the startup capability probe
    // (hdr::HdrAvailable()/hdr::DeepColor10Available()) found the feature usable
    // on this platform and display. When the probe found nothing -- platform has
    // no per-window HDR, no 10-bit visual, etc. -- there is nothing to toggle, so
    // the checkbox is hidden and only the warning is shown. A mid-session renderer
    // fallback (the probe succeeded but every capable renderer later failed) is
    // distinct: there the checkbox stays visible so the user can turn it off.
    GameArea* ga = wxGetApp().frame ? wxGetApp().frame->GetPanel() : nullptr;
    if (is_x11) {
        hdr_check->Hide();
        hdr_warning->Hide();

        // Sets the deep-color warning label, overriding the XRC default only for
        // the XWayland-fallback case.
        auto set_deep_warning_label = [deep_warning]() {
            if (VbamWaylandNoNativeRenderer()) {
                // Wayland session with no native renderer fell back to XWayland,
                // which can present neither HDR nor 10-bit SDR.
                deep_warning->SetLabel(
                    _("Neither HDR nor 10-bit SDR is available: no native "
                      "Wayland (Vulkan) renderer was found, so the display fell "
                      "back to XWayland (8-bit SDR only)."));
            }
        };

        if (!hdr::DeepColor10Available()) {
            // Capability probe found no 10-bit support on this platform/display:
            // nothing to toggle, so hide the checkbox. An ordinary 8-bit screen
            // is not worth a warning -- there was never a 10-bit visual for the
            // runtime test to fail on. Only warn for the XWayland-fallback case
            // (no native renderer), which is an actionable "no native renderer"
            // condition rather than a plain non-10-bit display.
            deep_check->Hide();
            if (VbamWaylandNoNativeRenderer()) {
                set_deep_warning_label();
                deep_warning->Show(true);
            } else {
                deep_warning->Hide();
            }
        } else {
            // Applied live on toggle (no OK-deferred validator) so the renderer
            // checks run immediately and the user sees the result. Initialize the
            // checkbox from the saved option.
            deep_check->SetValue(OPTION(kDispDeepColor));

            // Warning shown only when deep color is on (auto) but it cannot
            // actually be presented (every capable renderer failed this session).
            auto update_deep_warning =
                [this, ga, deep_warning, set_deep_warning_label](bool checked) {
                const bool avail = ga ? ga->DeepColorEffectivelyAvailable()
                                      : hdr::DeepColor10Available();
                const bool show = checked && !avail;
                if (show)
                    set_deep_warning_label();
                deep_warning->Show(show);
                Layout();
            };
            update_deep_warning(OPTION(kDispDeepColor));
            deep_check->Bind(wxEVT_CHECKBOX,
                             [this, ga, update_deep_warning](wxCommandEvent& ev) {
                const bool on = ev.IsChecked();
                if (on && ga)
                    ga->ResetDeepColorRevert();  // clear session revert so re-probe is honored
                OPTION(kDispDeepColor) = on;     // apply now: resets the panel + re-runs the checks
                update_deep_warning(on);
                // Re-show/switch renderers; also re-selects the radio and syncs
                // the SDL backend picker (re-filtered, so a now-hidden backend
                // drops to "default").
                UpdateRenderMethodVisibility();
                ev.Skip();
            });
        }
    } else {
        deep_check->Hide();
        deep_warning->Hide();

        // All four HDR brightness/tone sliders (white, peak, highlight knee,
        // shadow contrast) feed only the HDR encode, so enable them only when HDR
        // is effectively active -- they're disabled in SDR (and 10-bit-SDR).
        //
        // The sliders live on the Color Correction tab, which is loaded lazily and
        // independently of this one -- during idle preload the Basic tab is built
        // first, so they may not exist yet. Look them up tolerantly (FindWindow,
        // not the asserting GetValidatedChild): when the Color Correction tab is
        // built it sets their initial enabled state itself, and a live HDR toggle
        // updates them here only if they are already present.
        auto enable_hdr_sliders = [this](bool active) {
            for (const char* n : {"HDRReferenceWhite", "HDRPeakBrightness",
                                  "HDRHighlightKnee", "HDRShadowContrast"})
                if (wxWindow* slider = FindWindow(n))
                    slider->Enable(active);
        };

        if (!hdr::HdrAvailable()) {
            // Capability probe found no HDR support on this platform/display:
            // nothing to toggle, so hide the checkbox but keep the warning.
            // Exception: when HDR is unavailable only because the user turned it
            // off (in KDE, via the Windows HDR toggle, or the macOS System
            // Settings HDR toggle), hide the warning too -- it's a deliberate
            // choice, not a missing capability, so a "no effect" warning would be
            // noise.
            hdr_check->Hide();
#if defined(__WXMSW__) && defined(WINXP)
            // The 32-bit XP-compat build has no HDR-capable renderer (no
            // D3D11/D3D12/Vulkan) and cannot probe the display's HDR state (the
            // DXGI 1.6 / DisplayConfig advanced-color APIs are absent from the
            // WINVER 0x0501 toolchain), so it can neither present HDR nor tell
            // "HDR off" from "no HDR support". Suppress the "no effect" warning
            // that the modern builds show for a genuinely HDR-less display --
            // here it would be constant noise with nothing the user can act on.
            hdr_warning->Hide();
#else
            hdr_warning->Show(!KdeHdrDisabled() && !hdr::WindowsHdrDisabled() &&
                              !hdr::MacosHdrDisabled());
#endif
            enable_hdr_sliders(false);
        } else {
            // Applied live on toggle (no OK-deferred validator) so the renderer
            // checks run immediately. Initialize from the saved option.
            hdr_check->SetValue(OPTION(kDispHDR));

            // While the profile is in auto mode, an HDR toggle retargets it
            // (Rec2020 for HDR, sRGB for SDR). SetValue() doesn't fire the radio
            // event, so this doesn't clear auto mode.
            // The profile radios are also on the (lazily loaded) Color Correction
            // tab, so look them up tolerantly. This only mirrors the auto-chosen
            // profile into the radio UI; the profile option itself is updated by
            // the auto-follow when kDispHDR changes, so when that tab is built it
            // selects the right radio from the option regardless.
            auto retarget_profile = [this](bool checked) {
                if (OPTION(kDispColorCorrectionAuto)) {
                    const char* name = checked ? "ColorProfileRec2020" : "ColorProfileSRGB";
                    if (auto* rb = wxDynamicCast(FindWindow(name), wxRadioButton))
                        rb->SetValue(true);
                }
            };
            // Warning shown only when HDR is on (auto) but it cannot actually be
            // presented (every HDR renderer failed this session).
            auto update_hdr_warning =
                [this, ga, hdr_warning, enable_hdr_sliders](bool checked) {
                const bool avail = ga ? ga->HdrEffectivelyAvailable() : hdr::HdrAvailable();
                hdr_warning->Show(checked && !avail);
                enable_hdr_sliders(checked && avail);
                Layout();
            };
            update_hdr_warning(OPTION(kDispHDR));
            hdr_check->Bind(wxEVT_CHECKBOX,
                            [this, ga, retarget_profile, update_hdr_warning](wxCommandEvent& ev) {
                const bool on = ev.IsChecked();
                if (on && ga)
                    ga->ResetHdrRevert();  // clear session revert so re-probe is honored
                OPTION(kDispHDR) = on;     // apply now: resets the panel + re-runs the checks
                retarget_profile(on);
                update_hdr_warning(on);
                // Re-show/switch renderers; also re-selects the radio and syncs
                // the SDL backend picker (re-filtered, so a now-hidden backend
                // drops to "default").
                UpdateRenderMethodVisibility();
                ev.Skip();
            });
        }
    }
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

    // An explicit pick takes the profile out of auto mode (where it otherwise
    // follows HDR: sRGB for SDR, Rec2020 for HDR).
    auto clear_auto = [](wxCommandEvent& ev) {
        OPTION(kDispColorCorrectionAuto) = false;
        ev.Skip();
    };
    color_profile_srgb->Bind(wxEVT_RADIOBUTTON, clear_auto);
    color_profile_dci->Bind(wxEVT_RADIOBUTTON, clear_auto);
    color_profile_rec2020->Bind(wxEVT_RADIOBUTTON, clear_auto);

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

    // HDR brightness sliders (the HDR enable checkbox lives on the Basic tab).
    for (const auto& entry : {
             std::make_pair("HDRReferenceWhite", config::OptionID::kDispHDRReferenceWhite),
             std::make_pair("HDRPeakBrightness", config::OptionID::kDispHDRPeakBrightness),
             std::make_pair("HDRHighlightKnee",  config::OptionID::kDispHDRHighlightKnee),
             std::make_pair("HDRShadowContrast", config::OptionID::kDispHDRShadowContrast),
         }) {
        wxSlider* slider = GetValidatedChild<wxSlider>(entry.first);
        // Cap the peak-brightness slider at the display's actual peak when known
        // (macOS): higher values only clip/compress, and the option default is the
        // slider max, so this also makes the default resolve to the display peak.
        // Set the range before the validator loads the value so it isn't clamped
        // to the stale XRC max. 0 = unknown -> keep the XRC range.
        if (entry.second == config::OptionID::kDispHDRPeakBrightness) {
            if (const uint32_t dp = hdr::DisplayPeakNits())
                slider->SetRange(slider->GetMin(), static_cast<int>(dp));
        }
        slider->SetValidator(widgets::OptionUnsignedValidator(entry.second));
        slider->SetToolTip(wxString::Format("%d", slider->GetValue()));
        slider->Bind(wxEVT_SLIDER, std::bind(UpdateSliderTooltip, slider, std::placeholders::_1));
        slider->Bind(wxEVT_ENTER_WINDOW, std::bind(UpdateSliderTooltipOnHover, slider, std::placeholders::_1));
    }

    // All four HDR brightness/tone sliders (white, peak, highlight knee, shadow
    // contrast) feed only the HDR encode (LumScale), so they have no effect in
    // SDR or 10-bit-SDR (deep color) modes. Set their initial enabled state to
    // match whether HDR is effectively active; on the HDR platforms the HDR
    // checkbox handler keeps it in sync as the user toggles, and on X11 (no HDR)
    // this leaves them disabled.
    {
        GameArea* ga = wxGetApp().frame ? wxGetApp().frame->GetPanel() : nullptr;
        const bool hdr_active = OPTION(kDispHDR) &&
            (ga ? ga->HdrEffectivelyAvailable() : hdr::HdrAvailable());
        for (const char* n : {"HDRReferenceWhite", "HDRPeakBrightness",
                              "HDRHighlightKnee", "HDRShadowContrast"})
            GetValidatedChild(n)->Enable(hdr_active);
    }
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

        // BaseDialog restores any persisted size on this same wxEVT_SHOW, after
        // this handler returns (its handler runs later in the chain). That
        // restored size can be too narrow for the render-method radio row, or
        // too tall (it reserved space for the SDL sub-options that are hidden
        // this time). Defer the size reconciliation so it runs after the restore.
        CallAfter(&DisplayConfig::AdjustSizeOnShow);
    } else {
        StopPluginHandler();
    }

    // Let the event propagate.
    event.Skip();
}

void DisplayConfig::AdjustSizeOnShow() {
    if (render_method_radios_.empty())
        return;
    wxWindow* radio = GetValidatedChild(render_method_radios_.front().first);
    if (!radio)
        return;

    // Lay out the page first so its sizer recomputes its min size after the
    // show handler hid/showed the SDL sub-options; GetBestSize() reads that
    // cached sizer min size, which a bare InvalidateBestSize() does not refresh.
    // Without this the fitting height still includes the hidden SDL options and
    // the slack is not removed until some later Layout (e.g. clicking a radio).
    if (wxWindow* page = radio->GetParent()) {
        page->Layout();
        page->Refresh();
    }

    // GetBestSize() on the dialog is derived from the notebook, which caches
    // each page's best size; invalidate that cache up the chain so the fitting
    // size reflects the currently visible widgets (radios shown, SDL sub-options
    // shown/hidden).
    for (wxWindow* w = radio; w; w = w->GetParent()) {
        w->InvalidateBestSize();
        if (w == this)
            break;
    }

    const wxSize best = GetBestSize();
    const wxSize current = GetSize();
    // Width: grow if the radio row would be clipped, but keep a user-widened
    // dialog (never shrink). Height: size to content, which removes any leftover
    // vertical space a persisted size reserved for the now-hidden SDL options
    // (and grows if the visible content needs more).
    const int width =
        current.GetWidth() < best.GetWidth() ? best.GetWidth() : current.GetWidth();
    const int height = best.GetHeight();
    if (width != current.GetWidth() || height != current.GetHeight())
        SetSize(width, height);
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

void DisplayConfig::UpdateRenderMethodVisibility() {
    // When HDR or 10-bit deep color is available and on, hide every renderer that
    // can't present the active feature (and any capable renderer that failed this
    // session). When neither is on, RenderMethodHiddenFor*() return false for
    // every method, so all applicable renderers are shown -- restoring radios
    // that an earlier "on" state had hidden. Only the radios recorded as existing
    // on this build/platform are touched; statically hidden ones stay hidden.
    GameArea* ga = wxGetApp().frame ? wxGetApp().frame->GetPanel() : nullptr;

    auto is_visible = [&](config::RenderMethod m) {
        for (const auto& r : render_method_radios_)
            if (r.second == m)
                return !(ga && (ga->RenderMethodHiddenForHdr(m) ||
                                ga->RenderMethodHiddenForDeepColor(m)));
        return false;  // not an applicable radio on this build/platform
    };

    // The render method is applied on OK, not live, so OPTION(kDispRenderMethod)
    // can lag a manual radio click. Use the checked radio as the source of truth
    // for the active selection (fall back to the option if none is checked yet),
    // so a manual selection isn't reverted to the stale option below.
    config::RenderMethod current = OPTION(kDispRenderMethod);
    for (const auto& radio : render_method_radios_)
        if (auto* rb = wxDynamicCast(GetValidatedChild(radio.first), wxRadioButton))
            if (rb->GetValue()) { current = radio.second; break; }

    bool current_hidden = false;
    for (const auto& radio : render_method_radios_) {
        const bool hide = ga && (ga->RenderMethodHiddenForHdr(radio.second) ||
                                 ga->RenderMethodHiddenForDeepColor(radio.second));
        GetValidatedChild(radio.first)->Show(!hide);
        if (radio.second == current && hide)
            current_hidden = true;
    }

    // The active renderer was just hidden -- it can't present the feature that's
    // now on. Move the selection to one that can: the SDL renderer falls back to
    // Vulkan (its native equivalent), anything else to the next available
    // renderer in priority order. Setting the option drives both the radio
    // selection (via the option observer) and the panel.
    if (current_hidden) {
        static const config::RenderMethod kPriority[] = {
#ifndef NO_VULKAN
            config::RenderMethod::kVulkan,
#endif
#if defined(__WXMSW__) && !defined(NO_D3D12)
            config::RenderMethod::kDirect3d12,
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
            config::RenderMethod::kDirect3d,
#endif
#if defined(__WXMAC__) && !defined(NO_METAL)
            config::RenderMethod::kMetal,
#endif
            config::RenderMethod::kOpenGL,
            config::RenderMethod::kSimple,
            config::RenderMethod::kSDL,
        };
        config::RenderMethod target = current;
#ifndef NO_VULKAN
        if (current == config::RenderMethod::kSDL &&
            is_visible(config::RenderMethod::kVulkan)) {
            target = config::RenderMethod::kVulkan;
        } else
#endif
        {
            for (const config::RenderMethod m : kPriority)
                if (m != current && is_visible(m)) { target = m; break; }
        }
        if (target != current) {
            // Leaving SDL because this build can't present the feature (e.g. an
            // SDL2 build with 10-bit deep color on): reset its backend to
            // "default" so returning to SDL later starts from a clean, working
            // backend rather than the incapable one that was selected.
            if (current == config::RenderMethod::kSDL)
                OPTION(kSDLRenderer) = wxString("default");
            // Forced switch (the selected renderer can't present the feature now
            // on): apply it live and reflect it in the radios -- SetSelection()
            // fires no wxEVT_RADIOBUTTON, so set them explicitly.
            OPTION(kDispRenderMethod) = target;
            for (const auto& radio : render_method_radios_)
                if (auto* rb = wxDynamicCast(GetValidatedChild(radio.first), wxRadioButton))
                    rb->SetValue(radio.second == target);
            current = target;
        }
    }

    // Sync the SDL sub-options to the active selection (the checked radio, which
    // `current` tracks). Don't re-sync the radios from the option otherwise: the
    // render method is applied on OK, so the option can lag a manual radio click,
    // and forcing the radios from it would revert the user's selection. The SDL
    // sub-options are created later in InitBasicTab, so the selector may be null
    // on the initial call (OnDialogShowEvent sets their first-time visibility).
    if (sdlrenderer_selector_) {
        if (current == config::RenderMethod::kSDL) {
            ShowSDLOptions();
            wxCommandEvent dummy;
            FillRendererList(dummy);
        } else {
            HideSDLOptions();
        }
    }

    // Re-fit so the dialog grows/shrinks to the radios just shown/hidden (and
    // the SDL sub-options toggled above).
    if (!render_method_radios_.empty())
        RefitForVisibilityChange(
            GetValidatedChild(render_method_radios_.front().first));
}

void DisplayConfig::RefitForVisibilityChange(wxWindow* changed) {
    if (!changed)
        return;

    // Lay out and repaint the page that owns `changed` first. A dialog-level
    // Fit()/Layout() only re-runs a notebook page's sizer when the dialog's
    // computed size changes; when a toggle changes a page's content without
    // changing the overall best size, that cascade never fires and the affected
    // widgets stay unplaced (drawing garbled / overlapping until a stray
    // repaint). Lay out and repaint the page explicitly so the toggle alone
    // reflects the change.
    if (wxWindow* page = changed->GetParent()) {
        page->Layout();
        page->Refresh();
    }

    // Fit() below computes the dialog's fitting size from the notebook, which
    // caches each page's best size. After a visibility change that cache is
    // stale (it still reflects the pre-change layout), so the dialog neither
    // grows nor shrinks. Invalidate the cached best size up the chain --
    // changed -> sizers' window -> page -> notebook -> dialog -- so Fit() sees
    // the new content.
    for (wxWindow* w = changed; w; w = w->GetParent()) {
        w->InvalidateBestSize();
        if (w == this)
            break;
    }

    // Re-fit the frame to the new content. Match the existing platform
    // convention used elsewhere in this dialog.
#ifdef __WXMAC__
    Layout();
#else
    Fit();
#endif
}

void DisplayConfig::FillRendererList(wxCommandEvent& event) {
#ifndef ENABLE_SDL3
    SDL_RendererInfo render_info;
#endif
    int num_drivers = 0;

#if defined(HAVE_WAYLAND_SUPPORT) && defined(ENABLE_SDL3)
    // On Wayland, every SDL video init in this process must share GDK's
    // wl_display. SDL reads this global property only at the first video init;
    // if we let SDL open its own connection here (just to enumerate render
    // drivers), that stale display sticks. The SDL panel would then hand SDL a
    // wl_surface (its subsurface) created on GDK's display while SDL's event
    // queue belongs to its own -- and wl_proxy_set_queue aborts the process.
    // Import GDK's display first so the enumeration init and the later panel
    // init agree on one display. (The panel sets the same property; this just
    // covers the case where the dialog brings video up first.)
    if (IsWayland()) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
        if (wl_display* wl = gdk_wayland_display_get_wl_display(gdk_display_get_default()))
            SDL_SetPointerProperty(SDL_GetGlobalProperties(),
                SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, wl);
    }
#endif

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_Init(SDL_INIT_VIDEO);

    num_drivers = SDL_GetNumRenderDrivers();

    sdlrenderer_selector_->Clear();
    sdlrenderer_selector_->Append("default", new wxStringClientData("default"));
    sdlrenderer_selector_->SetSelection(0);

    // When HDR is on, only the HDR-capable SDL3 render backends can present it;
    // hide the legacy (SDL2-era) drivers that can only do SDR.
    const bool sdl_hdr_only =
#ifdef ENABLE_SDL3
        hdr::HdrAvailable() && OPTION(kDispHDR);
#else
        false;
#endif
    auto sdl_driver_is_hdr_capable = [](const wxString& name) {
        // On Wayland we drive HDR through the compositor with a 10-bit PQ
        // surface, and only SDL's "vulkan" backend actually presents that into
        // our borrowed subsurface -- "gpu" advertises support but shows a black
        // frame. So offer only vulkan for SDL HDR there.
        if (IsWayland())
            return name == "vulkan";
        return name == "direct3d11" || name == "direct3d12" ||
               name == "vulkan" || name == "metal" || name == "gpu";
    };

    // When 10-bit deep color is on (X11 only), keep only the SDL backends that
    // can actually render to a 10-bit target; drop the rest. Of the SDL render
    // backends only "vulkan" advertises a 2101010 texture format on X11: the
    // "opengl"/"opengles2" renderers expose 8-bit formats only, "software"
    // likewise, and the "gpu" (SDL_GPU) backend can't present into our borrowed
    // X11 window at all -- SDL_GPU builds its own VkSurfaceKHR, and for a window
    // SDL did not create SDL_WindowSupportsGPUSwapchainComposition reports every
    // composition (even SDR) unsupported, so ClaimWindowForGPUDevice fails ("Device
    // does not support requested swapchain composition"). The native "vulkan"
    // render backend uses the video subsystem's surface path, which does handle
    // an external window, so it is the only working 10-bit SDL backend here.
    // (The panel substitutes vulkan if one of those is somehow selected, but
    // there's no reason to offer a backend that only works by being silently
    // replaced.)
    GameArea* ga = wxGetApp().frame ? wxGetApp().frame->GetPanel() : nullptr;
    const bool sdl_deep_color_only =
#ifdef ENABLE_SDL3
        OPTION(kDispDeepColor) &&
        (ga ? ga->DeepColorEffectivelyAvailable() : hdr::DeepColor10Available());
#else
        ((void)ga, false);
#endif
    auto sdl_driver_is_deep_color_capable = [](const wxString& name) {
        return name == "vulkan";
    };

    // Normalize a saved backend that is exactly what "default" resolves to at
    // runtime back to "default", so picking "default" doesn't come back pinned to
    // the specific backend it happened to run as. "default" resolves to:
    //   * vulkan in the high-bit-depth modes (HDR / 10-bit deep color) -- the
    //     panel forces it; and
    //   * SDL's own first/most-preferred render driver otherwise (what
    //     SDL_CreateRenderer(NULL) selects -- e.g. opengl on X11).
    wxString eff_default;
    if (sdl_hdr_only || sdl_deep_color_only) {
        eff_default = wxString("vulkan");
    } else if (num_drivers > 0) {
#ifdef ENABLE_SDL3
        eff_default = wxString(SDL_GetRenderDriver(0));
#else
        SDL_RendererInfo first_info;
        SDL_GetRenderDriverInfo(0, &first_info);
        eff_default = wxString(first_info.name);
#endif
    }
    if (!eff_default.empty() && OPTION(kSDLRenderer) == eff_default) {
        OPTION(kSDLRenderer) = wxString("default");
    }

    // Track whether the saved backend survived the filtering above. "default" is
    // always offered; a named backend that got filtered out (e.g. opengl/gpu once
    // deep color or HDR is on) is no longer selectable, so we fall the option
    // back to "default" below.
    bool selected_shown = (OPTION(kSDLRenderer) == wxString("default"));

    for (int i = 0; i < num_drivers; i++) {
#ifdef ENABLE_SDL3
        // Two SDL backends don't work into our borrowed Wayland subsurface and
        // are never offered there: "gpu" presents a black frame, and "opengles2"
        // fails to create (EGL_BAD_SURFACE). "vulkan" (accelerated) and "opengl"
        // (GL) both work.
        if (IsWayland() && (wxString(SDL_GetRenderDriver(i)) == "gpu" ||
                            wxString(SDL_GetRenderDriver(i)) == "opengles2"))
            continue;
#if defined(__WXMAC__)
        // SDL's "vulkan" backend on macOS is MoltenVK loaded through the Vulkan
        // Portability dynamic library, which is absent in our build -- the
        // renderer fails to create ("Failed to load Vulkan Portability library").
        // Metal is the accelerated/HDR backend here, so never offer "vulkan".
        // "opengles2" likewise fails to create (macOS has no native OpenGL ES);
        // desktop "opengl" is the working GL backend, so never offer opengles2.
        if (wxString(SDL_GetRenderDriver(i)) == "vulkan" ||
            wxString(SDL_GetRenderDriver(i)) == "opengles2")
            continue;
#endif
#if defined(WINXP)
        // The 32-bit XP-compat build targets legacy Windows/hardware where these
        // SDL render backends rely on APIs or GPU features that are absent
        // (Direct3D 11/12, Vulkan, OpenGL ES 2, SDL_GPU), so never offer them.
        {
            const wxString drv(SDL_GetRenderDriver(i));
            if (drv == "direct3d11" || drv == "direct3d12" ||
                drv == "opengles2" || drv == "vulkan" || drv == "gpu")
                continue;
        }
#endif
        if (sdl_hdr_only && !sdl_driver_is_hdr_capable(wxString(SDL_GetRenderDriver(i))))
            continue;
        if (sdl_deep_color_only && !sdl_driver_is_deep_color_capable(wxString(SDL_GetRenderDriver(i))))
            continue;

        sdlrenderer_selector_->Append(SDL_GetRenderDriver(i), new wxStringClientData(SDL_GetRenderDriver(i)));

        if (OPTION(kSDLRenderer) == wxString(SDL_GetRenderDriver(i))) {
            // Use the actual appended index: filtered-out drivers above mean it
            // is not i+1.
            sdlrenderer_selector_->SetSelection(sdlrenderer_selector_->GetCount() - 1);
            selected_shown = true;
        }
#else
        (void)sdl_hdr_only; (void)sdl_driver_is_hdr_capable;
        (void)sdl_deep_color_only; (void)sdl_driver_is_deep_color_capable;
        SDL_GetRenderDriverInfo(i, &render_info);

#if defined(WINXP)
        // See the SDL3-branch note: the XP-compat build cannot use these
        // backends, so never offer them.
        {
            const wxString drv(render_info.name);
            if (drv == "direct3d11" || drv == "direct3d12" ||
                drv == "opengles2" || drv == "vulkan" || drv == "gpu")
                continue;
        }
#endif

        sdlrenderer_selector_->Append(render_info.name, new wxStringClientData(render_info.name));

        if (OPTION(kSDLRenderer) == wxString(render_info.name)) {
            sdlrenderer_selector_->SetSelection(sdlrenderer_selector_->GetCount() - 1);
            selected_shown = true;
        }
#endif
    }

    // The saved SDL backend is no longer offered (hidden for the active HDR /
    // deep-color mode): fall back to "default", which the panel resolves to a
    // working backend (vulkan for the high-bit-depth modes). Apply it to the
    // option too so the choice is consistent and persists, not just the UI.
    if (!selected_shown) {
        OPTION(kSDLRenderer) = wxString("default");
        sdlrenderer_selector_->SetSelection(0);
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
    // Pixel-art checkbox intentionally left hidden (disabled until further
    // investigation); see InitBasicTab.
}

void DisplayConfig::UpdateSDLOptionsVisibility(wxCommandEvent& event) {
    // Check if SDL output module is selected
    wxRadioButton* sdl_button = wxDynamicCast(GetValidatedChild("OutputSDL"), wxRadioButton);
    if (sdl_button && sdl_button->GetValue()) {
        ShowSDLOptions();
        // Populate the renderer list now that SDL has just been selected. It is
        // filled on selection rather than at dialog-open (where OnDialogShowEvent
        // only fills it if SDL was already the active renderer) because
        // FillRendererList calls SDL_Init, which can disturb a live OpenGL panel.
        wxCommandEvent dummy_event;
        FillRendererList(dummy_event);
    } else {
        HideSDLOptions();
    }

    // Re-fit so the dialog gains room for the renderer dropdown when SDL options
    // are shown and gives it back when hidden. A plain Fit() can't shrink/grow
    // here because the notebook caches the page's pre-toggle best size; the
    // helper invalidates that cache up the chain first.
    RefitForVisibilityChange(sdlrenderer_selector_);

    // Let the event propagate
    event.Skip();
}

}  // namespace dialogs