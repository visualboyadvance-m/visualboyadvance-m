#ifndef VBAM_WX_DIALOGS_BASE_DIALOG_H_
#define VBAM_WX_DIALOGS_BASE_DIALOG_H_

#include <wx/dialog.h>
#include <wx/event.h>

#include "wx/widgets/keep-on-top-styler.h"

#include "core/base/check.h"

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

    const widgets::KeepOnTopStyler keep_on_top_styler_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_BASE_DIALOG_H_