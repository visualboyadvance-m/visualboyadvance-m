#include "wx/dialogs/base-dialog.h"

#include <wx/persist.h>
#include <wx/persist/toplevel.h>
#include <wx/xrc/xmlres.h>

#include "core/base/check.h"
#include "wx/config/option-proxy.h"
#include "wx/widgets/utils.h"

namespace dialogs {

namespace {

// Due to the way wxWidgets handles initialization, we need to use a custom
// persistent object to save and restore the dialog's position. In particular,
// when the dialog is constructed, the the main window is not (yet) in its final
// position. This means we need to differentiate between 2 states here:
// 1. The dialog was registered for the first time and never shown.
//    We don't want to save the position in this case.
// 2. The dialog was registered and shown at least once.
//    We want to save the position in this case. However, on restore, we need
//    to check we are not drawing out of bounds.
//
// We can't use the built-in wxPersistenceManager for this, as it doesn't allow
// us to differentiate between these 2 states.
class PersistentBaseDialog : public wxPersistentWindow<BaseDialog> {
public:
    PersistentBaseDialog(BaseDialog* window) : wxPersistentWindow<BaseDialog>(window) {}

private:
    // wxPersistentObject implementation.
    wxString GetKind() const override { return "Dialog"; }

    void Save() const override {
        if (!dialog_shown_) {
            // Do not update the position if the dialog was not shown.
            return;
        }

        const wxRect dialog_rect = this->Get()->GetRect();
        this->SaveValue("x", dialog_rect.x);
        this->SaveValue("y", dialog_rect.y);
        this->SaveValue("width", dialog_rect.width);
        this->SaveValue("height", dialog_rect.height);
    }

    bool Restore() override {
        dialog_shown_ = true;
        wxRect dialog_rect(0, 0, 0, 0);
        if (!this->RestoreValue("x", &dialog_rect.x)) {
            return false;
        };
        if (!this->RestoreValue("y", &dialog_rect.y)) {
            return false;
        };
        if (!this->RestoreValue("width", &dialog_rect.width)) {
            return false;
        };
        if (!this->RestoreValue("height", &dialog_rect.height)) {
            return false;
        };

        this->Get()->SetSize(dialog_rect);
        return true;
    }

    bool dialog_shown_ = false;
};

}  // namespace

// static
wxDialog* BaseDialog::LoadDialog(wxWindow* parent, const wxString& xrc_file) {
    VBAM_CHECK(parent);
    return new BaseDialog(parent, xrc_file);
}

BaseDialog::BaseDialog(wxWindow* parent, const wxString& xrc_file)
    : wxDialog(), keep_on_top_styler_(this) {
#if !wxCHECK_VERSION(3, 1, 0)
    // This needs to be set before loading any element on the window. This also
    // has no effect since wx 3.1.0, where it became the default.
    this->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
#endif

    VBAM_CHECK(wxXmlResource::Get()->LoadDialog(this, parent, xrc_file));

    // Bind the event handler.
    this->Bind(wxEVT_SHOW, &BaseDialog::OnBaseDialogShow, this);

    wxPersistenceManager::Get().Register(this, new PersistentBaseDialog(this));
}

wxWindow* BaseDialog::GetValidatedChild(const wxString& name) const {
    wxWindow* window = this->FindWindow(name);
    VBAM_CHECK(window);
    return window;
}

void BaseDialog::OnBaseDialogShow(wxShowEvent& event) {
    if (event.IsShown()) {
        // Restore the dialog saved position.
        if (wxPersistenceManager::Get().Restore(this)) {
            // Ensure we are not restoring the dialog out of bounds.
            if (!widgets::GetDisplayRect().Intersects(this->GetRect())) {
                this->RepositionDialog();
            }
        } else {
            // First-time use.
            this->RepositionDialog();
        }

        // Do not run this again.
        this->Unbind(wxEVT_SHOW, &BaseDialog::OnBaseDialogShow, this);
    }

    // Let the event propagate.
    event.Skip();
}

void BaseDialog::RepositionDialog() {
    // Re-position the dialog slightly to the bottom-right of the parent.
    const wxWindow* parent = this->GetParent();
    wxPoint parent_pos;
    if (parent) {
        parent_pos = parent->GetPosition();
    } else {
        parent_pos = wxPoint(OPTION(kGeomWindowX), OPTION(kGeomWindowY));
    }
    const wxPoint dialog_pos = parent_pos + wxPoint(40, 40);
    this->SetPosition(dialog_pos);
}

}  // namespace dialogs
