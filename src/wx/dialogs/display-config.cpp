#include "wx/dialogs/display-config.h"

#include <wx/arrstr.h>
#include <wx/choice.h>
#include <wx/clntdata.h>
#include <wx/dir.h>
#include <wx/dynlib.h>
#include <wx/log.h>
#include <wx/object.h>
#include <wx/radiobut.h>
#include <wx/textctrl.h>
#include <wx/valnum.h>

#include <wx/xrc/xmlres.h>

#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/dialogs/base-dialog.h"
#include "wx/rpi.h"
#include "wx/widgets/option-validator.h"
#include "wx/widgets/render-plugin.h"
#include "wx/wxvbam.h"

namespace dialogs {

namespace {

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
    GetValidatedChild("OutputSimple")
        ->SetValidator(RenderValidator(config::RenderMethod::kSimple));

#if defined(__WXMAC__)
    GetValidatedChild("OutputQuartz2D")
        ->SetValidator(RenderValidator(config::RenderMethod::kQuartz2d));
#else
    GetValidatedChild("OutputQuartz2D")->Hide();
#endif

#ifdef NO_OGL
    GetValidatedChild("OutputOpenGL")->Hide();
#elif defined(HAVE_WAYLAND_SUPPORT) && !defined(HAVE_WAYLAND_EGL)
    // wxGLCanvas segfaults on Wayland before wx 3.2.
    if (IsWayland()) {
        GetValidatedChild("OutputOpenGL")->Hide();
    } else {
        GetValidatedChild("OutputOpenGL")
            ->SetValidator(RenderValidator(config::RenderMethod::kOpenGL));
    }
#else
    GetValidatedChild("OutputOpenGL")
        ->SetValidator(RenderValidator(config::RenderMethod::kOpenGL));
#endif  // NO_OGL

#if defined(__WXMSW__) && !defined(NO_D3D)
    // Enable the Direct3D option on Windows.
    GetValidatedChild("OutputDirect3D")
        ->SetValidator(RenderValidator(config::RenderMethod::kDirect3d));
#else
    GetValidatedChild("OutputDirect3D")->Hide();
#endif

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
    if (event.IsShown()) {
        PopulatePluginOptions();
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

    Fit();
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

}  // namespace dialogs
