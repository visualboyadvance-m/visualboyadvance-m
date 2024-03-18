#ifndef VBAM_WX_DIALOGS_DISPLAY_CONFIG_H_
#define VBAM_WX_DIALOGS_DISPLAY_CONFIG_H_

#include <wx/dialog.h>
#include <wx/event.h>

#include "wx/config/option-observer.h"
#include "wx/widgets/keep-on-top-styler.h"

// Forward declarations.
class wxChoice;
class wxControl;
class wxWindow;

namespace config {
class Option;
}

namespace dialogs {

// Manages the display configuration dialog.
class DisplayConfig : public wxDialog {
public:
    static DisplayConfig* NewInstance(wxWindow* parent);
    ~DisplayConfig() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    DisplayConfig(wxWindow* parent);

    // Handler for the wxEVT_SHOW event.
    void OnDialogShowEvent(wxShowEvent& event);

    // Populates the plugin options.
    void PopulatePluginOptions();

    // Stops handling the plugin options.
    void StopPluginHandler();

    // Update the plugin display.
    void UpdatePlugin(wxCommandEvent& event);

    // Displays the new filter name on the screen.
    void OnFilterChanged(config::Option* option);

    // Displays the new interframe name on the screen.
    void OnInterframeChanged(config::Option* option);

    // Hides/Shows the plugin-related filter options.
    void HidePluginOptions();
    void ShowPluginOptions();

    wxControl* plugin_label_;
    wxChoice* plugin_selector_;
    wxChoice* filter_selector_;
    wxChoice* interframe_selector_;
    const config::OptionsObserver filter_observer_;
    const config::OptionsObserver interframe_observer_;
    const widgets::KeepOnTopStyler keep_on_top_styler_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_DISPLAY_CONFIG_H_
