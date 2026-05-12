#ifndef VBAM_WX_DIALOGS_DISPLAY_CONFIG_H_
#define VBAM_WX_DIALOGS_DISPLAY_CONFIG_H_

#include <vector>

#include <wx/filepicker.h>

#include "wx/dialogs/base-dialog.h"
#include "wx/config/option-observer.h"

// Forward declarations.
class wxChoice;
class wxControl;
class wxNotebook;
class wxWindow;

namespace config {
class Option;
}

namespace dialogs {

// Manages the display configuration dialog.
class DisplayConfig : public BaseDialog {
public:
    static DisplayConfig* NewInstance(wxWindow* parent);
    ~DisplayConfig() override = default;

    // BaseDialog lazy-tab interface.
    int LazyTabCount() const override { return kTabCount; }
    bool LoadLazyTab(int index) override;

private:
    enum Tab {
        kTabBasic = 0,
        kTabBitDepth,
        kTabColorCorrection,
        kTabSpeed,
        kTabOSD,
        kTabZoom,
        kTabCount,
    };

    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    DisplayConfig(wxWindow* parent);

    // Per-tab init: called from LoadLazyTab once the tab's panel is parsed
    // and added to the notebook.
    void InitBasicTab();
    void InitBitDepthTab();
    void InitColorCorrectionTab();
    void InitSpeedTab();
    void InitOSDTab();
    void InitZoomTab();

    // Handler for the wxEVT_SHOW event.
    void OnDialogShowEvent(wxShowEvent& event);

    // Populates the plugin options.
    void PopulatePluginOptions();

    // Stops handling the plugin options.
    void StopPluginHandler();

    // Update the plugin display when filter changes.
    void UpdatePlugin(wxCommandEvent& event);

    // Update the filter when plugin selection changes.
    void OnPluginSelected(wxCommandEvent& event);

    // Displays the new filter name on the screen.
    void OnFilterChanged(config::Option* option);

    // Displays the new interframe name on the screen.
    void OnInterframeChanged(config::Option* option);

    // Renderer changed
    void FillRendererList(wxCommandEvent& event);

    // Hides/Shows the plugin-related filter options.
    void HidePluginOptions();
    void ShowPluginOptions();

    // Hides/Shows the SDL-related output options.
    void HideSDLOptions();
    void ShowSDLOptions();

    // Update SDL options visibility based on output module selection.
    void UpdateSDLOptionsVisibility(wxCommandEvent& event);

    // Handler for plugin directory change.
    void OnPluginDirChanged(wxFileDirPickerEvent& event);

    wxNotebook* notebook_ = nullptr;
    std::vector<bool> tab_loaded_;

    wxControl* plugin_dir_label_ = nullptr;
    wxDirPickerCtrl* plugin_dir_picker_ = nullptr;
    wxControl* plugin_label_ = nullptr;
    wxChoice* plugin_selector_ = nullptr;
    wxChoice* filter_selector_ = nullptr;
    wxChoice* interframe_selector_ = nullptr;
    wxChoice* sdlrenderer_selector_ = nullptr;
    wxControl* sdlrenderer_label_ = nullptr;
    wxControl* sdlpixelart_checkbox_ = nullptr;
    const config::OptionsObserver filter_observer_;
    const config::OptionsObserver interframe_observer_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_DISPLAY_CONFIG_H_
