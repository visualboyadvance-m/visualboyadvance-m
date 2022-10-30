#include "dialogs/display-config.h"

#include <wx/arrstr.h>
#include <wx/choice.h>
#include <wx/clntdata.h>
#include <wx/dir.h>
#include <wx/dynlib.h>
#include <wx/log.h>
#include <wx/object.h>
#include <wx/radiobut.h>
#include <wx/spinctrl.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/xrc/xmlres.h>

#include "config/option.h"
#include "rpi.h"
#include "wayland.h"
#include "widgets/option-validator.h"
#include "widgets/render-plugin.h"
#include "widgets/wx/wxmisc.h"
#include "wxvbam.h"

namespace dialogs {

namespace {

// Validator for a wxTextCtrl with a double value.
class ScaleValidator : public widgets::OptionValidator {
public:
    ScaleValidator() : widgets::OptionValidator(config::OptionID::kDispScale) {}
    ~ScaleValidator() override = default;

private:
    // OptionValidator implementation.
    wxObject* Clone() const override { return new ScaleValidator(); }

    bool IsWindowValueValid() override {
        double value;
        if (!wxDynamicCast(GetWindow(), wxTextCtrl)
                 ->GetValue()
                 .ToDouble(&value)) {
            return false;
        }

        return true;
    }

    bool WriteToWindow() override {
        wxDynamicCast(GetWindow(), wxTextCtrl)
            ->SetValue(wxString::Format("%.1f", option()->GetDouble()));
        return true;
    }

    bool WriteToOption() override {
        double value;
        if (!wxDynamicCast(GetWindow(), wxTextCtrl)
                 ->GetValue()
                 .ToDouble(&value)) {
            return false;
        }

        return option()->SetDouble(value);
    }
};

// Validator for a wxChoice with a Filter value.
class FilterValidator : public widgets::OptionValidator {
public:
    FilterValidator() : OptionValidator(config::OptionID::kDispFilter) {
        Bind(wxEVT_CHOICE, &FilterValidator::OnChoice, this);
    }
    ~FilterValidator() override = default;

private:
    // wxChoice event handler.
    void OnChoice(wxCommandEvent&) { WriteToOption(); }

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

        if (static_cast<size_t>(selection) > config::kNbFilters) {
            return false;
        }

        return option()->SetFilter(static_cast<config::Filter>(selection));
    }
};

// Validator for a wxChoice with an Interframe value.
class InterframeValidator : public widgets::OptionValidator {
public:
    InterframeValidator() : OptionValidator(config::OptionID::kDispIFB) {
        Bind(wxEVT_CHOICE, &InterframeValidator::OnChoice, this);
    }
    ~InterframeValidator() override = default;

private:
    // wxChoice event handler.
    void OnChoice(wxCommandEvent&) { WriteToOption(); }

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

        if (static_cast<size_t>(selection) > config::kNbInterframes) {
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
        assert(render_method != config::RenderMethod::kLast);
        Bind(wxEVT_RADIOBUTTON, &RenderValidator::OnRadioButton, this);
    }
    ~RenderValidator() override = default;

private:
    // wxRadioButton event handler.
    void OnRadioButton(wxCommandEvent&) { WriteToOption(); }

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
        assert(plugin_selector);
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
        assert(plugin_selector);
        const wxString& selected_window_plugin =
            dynamic_cast<wxStringClientData*>(
                plugin_selector->GetClientObject(
                    plugin_selector->GetSelection()))
                ->GetData();
        return option()->SetString(selected_window_plugin);
    }
};

// Helper functions to assert on the returned value.
wxWindow* GetValidatedChild(const wxWindow* parent, const wxString& name) {
    wxWindow* window = parent->FindWindow(name);
    assert(window);
    return window;
}

template <class T>
T* GetValidatedChild(const wxWindow* parent, const wxString& name) {
    T* child = wxDynamicCast(GetValidatedChild(parent, name), T);
    assert(child);
    return child;
}

}  // namespace

// static
DisplayConfig* DisplayConfig::NewInstance(wxWindow* parent) {
    assert(parent);
    return new DisplayConfig(parent);
}

DisplayConfig::DisplayConfig(wxWindow* parent)
    : wxDialog(),
      filter_observer_(config::OptionID::kDispFilter,
                       std::bind(&DisplayConfig::OnFilterChanged,
                                 this,
                                 std::placeholders::_1)),
      interframe_observer_(config::OptionID::kDispIFB,
                           std::bind(&DisplayConfig::OnInterframeChanged,
                                     this,
                                     std::placeholders::_1)) {
#if !wxCHECK_VERSION(3, 1, 0)
    // This needs to be set before loading any element on the window. This also
    // has no effect since wx 3.1.0, where it became the default.
    this->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
#endif
    wxXmlResource::Get()->LoadDialog(this, parent, "DisplayConfig");

    // Speed
    // AutoSkip/FrameSkip are 2 controls for 1 value.  Needs post-process
    // to ensure checkbox not ignored
    GetValidatedChild(this, "FrameSkip")
        ->SetValidator(wxGenericValidator(&frameSkip));
    if (frameSkip >= 0) {
        systemFrameSkip = frameSkip;
    }

    // On-Screen Display
    GetValidatedChild(this, "SpeedIndicator")
        ->SetValidator(wxGenericValidator(&showSpeed));

    // Zoom
    GetValidatedChild(this, "DefaultScale")->SetValidator(ScaleValidator());

    // this was a choice, but I'd rather not have to make an off-by-one
    // validator just for this, and spinctrl is good enough.
    GetValidatedChild(this, "MaxScale")
        ->SetValidator(wxGenericValidator(&maxScale));

    // Basic
    GetValidatedChild(this, "OutputSimple")
        ->SetValidator(RenderValidator(config::RenderMethod::kSimple));

#if defined(__WXMAC__)
    GetValidatedChild(this, "OutputQuartz2D")
        ->SetValidator(RenderValidator(config::RenderMethod::kQuartz2d));
#else
    GetValidatedChild(this, "OutputQuartz2D")->Hide();
#endif

#ifdef NO_OGL
    GetValidatedChild(this, "OutputOpenGL")->Hide();
#elif defined(__WXGTK__) && !wxCHECK_VERSION(3, 2, 0)
    // wxGLCanvas segfaults on Wayland before wx 3.2.
    if (IsWayland()) {
        GetValidatedChild(this, "OutputOpenGL")->Hide();
    } else {
        GetValidatedChild(this, "OutputOpenGL")
            ->SetValidator(RenderValidator(config::RenderMethod::kOpenGL));
    }
#else
    GetValidatedChild(this, "OutputOpenGL")
        ->SetValidator(RenderValidator(config::RenderMethod::kOpenGL));
#endif  // NO_OGL

    // Direct3D is not implemented so hide the option on every platform.
    GetValidatedChild(this, "OutputDirect3D")->Hide();

    filter_selector_ = GetValidatedChild<wxChoice>(this, "Filter");
    filter_selector_->SetValidator(FilterValidator());

    // These are filled and/or hidden at dialog load time.
    plugin_label_ = GetValidatedChild<wxControl>(this, "PluginLab");
    plugin_selector_ = GetValidatedChild<wxChoice>(this, "Plugin");

    interframe_selector_ = GetValidatedChild<wxChoice>(this, "IFB");
    interframe_selector_->SetValidator(InterframeValidator());

    Bind(wxEVT_SHOW, &DisplayConfig::OnDialogShown, this, GetId());
    Bind(wxEVT_CLOSE_WINDOW, &DisplayConfig::OnDialogClosed, this, GetId());

    // Finally, fit everything nicely.
    Fit();
}

void DisplayConfig::OnDialogShown(wxShowEvent&) {
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

    const wxString& selected_plugin =
        config::OptDispFilterPlugin()->GetString();
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
        config::OptDispFilterPlugin()->SetString(wxEmptyString);
    }

    plugin_selector_->SetValidator(PluginSelectorValidator());
    ShowPluginOptions();

    Fit();
}

void DisplayConfig::OnDialogClosed(wxCloseEvent&) {
    // Reset the validator to stop handling events while this dialog is not
    // shown.
    plugin_selector_->SetValidator(wxValidator());
}

void DisplayConfig::OnFilterChanged(config::Option* option) {
    const config::Filter option_filter = option->GetFilter();
    const bool is_plugin = (option_filter == config::Filter::kPlugin);
    plugin_label_->Enable(is_plugin);
    plugin_selector_->Enable(is_plugin);

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
        if (config::OptDispFilter()->GetFilter() == config::Filter::kPlugin) {
            config::OptDispFilter()->SetFilter(config::Filter::kNone);
        }
        filter_selector_->Delete(config::kNbFilters - 1);
    }

    // Also erase the Plugin value to avoid issues down the line.
    config::OptDispFilterPlugin()->SetString(wxEmptyString);
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
