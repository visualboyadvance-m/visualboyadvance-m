#include "wx/dialogs/base-dialog.h"

#include <functional>

#include <wx/notebook.h>
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

#if WX_RESIZE_DIALOGS
        const wxRect dialog_rect = this->Get()->GetRect();
        this->SaveValue("x", dialog_rect.x);
        this->SaveValue("y", dialog_rect.y);
        this->SaveValue("width", dialog_rect.width);
        this->SaveValue("height", dialog_rect.height);
        const long dpi = this->Get()->GetDPIScaleFactor() * 100;
        this->SaveValue("dpi", dpi);
#endif  // WX_RESIZE_DIALOGS
    }

    bool Restore() override {
        dialog_shown_ = true;

#if WX_RESIZE_DIALOGS
        const long current_dpi = this->Get()->GetDPIScaleFactor() * 100;
        long saved_dpi = 1;
        if (!this->RestoreValue("dpi", &saved_dpi)) {
            return false;
        };
        if (saved_dpi != current_dpi) {
            // DPI changed, do not restore.
            return false;
        }

        // Restore position and size.
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
#endif  // WX_RESIZE_DIALOGS
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

#ifdef WINXP
    // Force white background on all notebook pages on Windows XP where dark mode is unsupported.
    // This function recursively finds all notebooks, including nested ones.
    std::function<void(wxWindow*)> fix_notebooks = [&fix_notebooks](wxWindow* window) {
        wxWindowList& children = window->GetChildren();
        for (wxWindowList::iterator it = children.begin(); it != children.end(); ++it) {
            wxWindow* child = *it;
            if (wxNotebook* notebook = wxDynamicCast(child, wxNotebook)) {
                for (size_t i = 0; i < notebook->GetPageCount(); ++i) {
                    wxWindow* page = notebook->GetPage(i);
                    if (page) {
                        page->SetBackgroundColour(*wxWHITE);
                        // Recursively check for nested notebooks
                        fix_notebooks(page);
                    }
                }
            } else {
                // Recursively check children
                fix_notebooks(child);
            }
        }
    };
    fix_notebooks(this);
#endif

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
    // Let the event propagate.
    event.Skip();

    if (!event.IsShown()) {
        return;
    }

    if (!dialog_shown_) {
        // First time call.
        dialog_shown_ = true;

        // Restore the dialog saved position.
        if (wxPersistenceManager::Get().Restore(this)) {
            // Ensure we are not restoring the dialog out of bounds.
            if (!widgets::GetDisplayRect().Intersects(this->GetRect())) {
                this->RepositionDialog();
            }
        } else {
            // First time this dialog is shown.
            this->RepositionDialog();
        }

        return;
    } 

    // The screen setup might have changed, check we are not out of bounds.
    if (!widgets::GetDisplayRect().Intersects(this->GetRect())) {
        this->RepositionDialog();
    }
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
