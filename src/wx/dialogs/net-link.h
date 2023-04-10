#ifndef VBAM_WX_DIALOGS_NET_LINK_H_
#define VBAM_WX_DIALOGS_NET_LINK_H_

#if !defined(NO_LINK)

#include <wx/dialog.h>

#include "../gba/GBALink.h"
#include "widgets/keep-on-top-styler.h"

namespace dialogs {

// Helper function to get the current LinkMode.
LinkMode GetConfiguredLinkMode();

// Manages the Network Link dialog.
class NetLink : public wxDialog {
public:
    static NetLink* NewInstance(wxWindow* parent);
    ~NetLink() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    NetLink(wxWindow* parent);

    // Modifies the dialog when the server option is selected.
    void OnServerSelected(wxCommandEvent& event);

    // Modifies the dialog when the client option is selected.
    void OnClientSelected(wxCommandEvent& event);

    // Custom validation for this dialog.
    void OnValidate(wxCommandEvent& event);

    wxWindow* server_panel_;
    wxWindow* client_panel_;
    wxWindow* ok_button_;
    const widgets::KeepOnTopStyler keep_on_top_styler_;
};

}  // namespace dialogs

#endif  // !defined(NO_LINK)

#endif  // VBAM_WX_DIALOGS_NET_LINK_H_
