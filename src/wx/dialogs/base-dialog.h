#ifndef VBAM_WX_DIALOGS_BASE_DIALOG_H_
#define VBAM_WX_DIALOGS_BASE_DIALOG_H_

#include <functional>
#include <vector>

#include <wx/dialog.h>
#include <wx/event.h>

#include "wx/widgets/keep-on-top-styler.h"

#include "core/base/check.h"

class wxNotebook;
class wxPanel;

#if defined(__WXGTK__)
// Moving and resizing dialogs does not work properly on wxGTK.
#define WX_RESIZE_DIALOGS 0
#else
#define WX_RESIZE_DIALOGS 1
#endif

namespace dialogs {

class BaseDialog : public wxDialog {
public:
    static wxDialog* LoadDialog(wxWindow* parent, const wxString& xrc_file);

    ~BaseDialog() override = default;

    // Lazy-tab support. Dialogs that split a wxNotebook across multiple XRC
    // files override these so MainFrame::PreloadOneDialog can spread the
    // per-tab parse cost across idle ticks. Default: no lazy tabs.
    virtual int LazyTabCount() const;
    // Loads the given tab if not already loaded. Returns true if work was
    // done (i.e. the tab was just now parsed and wired up). Idempotent.
    virtual bool LoadLazyTab(int index);

    // Lighter-weight alternative to subclassing for old-style dialogs whose
    // initialization lives in guiinit.cpp. The init callback runs after
    // the tab's panel has been parsed and added to the notebook; it should
    // wire up validators/event handlers for that tab's controls.
    struct LazyTabSpec {
        wxString xrc_name;
        wxString tab_label;
        std::function<void(wxPanel*)> init;
    };
    void SetupLazyTabs(const wxString& notebook_name,
                       std::vector<LazyTabSpec> tabs);

    // Override to force-load all unloaded tabs before the dialog becomes
    // visible. Runs strictly before any wxEVT_SHOW handler so per-dialog
    // OnShow handlers see a fully populated tree.
    bool Show(bool show = true) override;

protected:
    BaseDialog(wxWindow* parent, const wxString& xrc_file);

    // Helper function to assert on the returned value.
    wxWindow* GetValidatedChild(const wxString& name) const;

    template <class T>
    T* GetValidatedChild(const wxString& name) const {
        T* child = wxDynamicCast(this->GetValidatedChild(name), T);
        VBAM_CHECK(child);
        return child;
    }

private:
    // Handler for the wxEVT_SHOW event.
    void OnBaseDialogShow(wxShowEvent& event);

    // Repositions the dialog to the bottom-right of the parent.
    void RepositionDialog();

    // Set to true once the dialog has been shown for the first time.
    bool dialog_shown_ = false;

    // SetupLazyTabs state.
    wxNotebook* lazy_notebook_ = nullptr;
    std::vector<LazyTabSpec> lazy_tabs_;
    std::vector<bool> lazy_tab_loaded_;

    const widgets::KeepOnTopStyler keep_on_top_styler_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_BASE_DIALOG_H_