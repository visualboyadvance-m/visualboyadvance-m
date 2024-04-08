#ifndef VBAM_WX_DIALOGS_BASE_DIALOG_H_
#define VBAM_WX_DIALOGS_BASE_DIALOG_H_

#include <wx/dialog.h>
#include <wx/event.h>

#include "wx/widgets/keep-on-top-styler.h"

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
        assert(child);
        return child;
    }

private:
    // Handler for the wxEVT_SHOW event.
    void OnBaseDialogShow(wxShowEvent& event);

    // Repositions the dialog to the bottom-right of the parent.
    void RepositionDialog();

    const widgets::KeepOnTopStyler keep_on_top_styler_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_BASE_DIALOG_H_