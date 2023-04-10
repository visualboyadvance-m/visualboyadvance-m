#include "net-link.h"

#if !defined(NO_LINK)

#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/valnum.h>
#include <wx/xrc/xmlres.h>

#include "../System.h"
#include "config/option-proxy.h"
#include "dialogs/validated-child.h"
#include "widgets/option-validator.h"

namespace dialogs {

LinkMode GetConfiguredLinkMode() {
    switch (OPTION(kGBALinkType)) {
    case 0:
        return LINK_DISCONNECTED;

    case 1:
        if (OPTION(kGBALinkProto))
            return LINK_CABLE_IPC;
        else
            return LINK_CABLE_SOCKET;

    case 2:
        if (OPTION(kGBALinkProto))
            return LINK_RFU_IPC;
        else
            return LINK_RFU_SOCKET;

    case 3:
        return LINK_GAMECUBE_DOLPHIN;

    case 4:
        if (OPTION(kGBALinkProto))
            return LINK_GAMEBOY_IPC;
        else
            return LINK_GAMEBOY_SOCKET;

    default:
        return LINK_DISCONNECTED;
    }
}

// static
NetLink* NetLink::NewInstance(wxWindow* parent) {
    assert(parent);
    return new NetLink(parent);
}

NetLink::NetLink(wxWindow* parent) : wxDialog(), keep_on_top_styler_(this) {
#if !wxCHECK_VERSION(3, 1, 0)
    // This needs to be set before loading any element on the window. This also
    // has no effect since wx 3.1.0, where it became the default.
    this->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
#endif
    assert(wxXmlResource::Get()->LoadDialog(this, parent, "NetLink"));

    server_panel_ = GetValidatedChild(this, "NetLinkServerPanel");
    client_panel_ = GetValidatedChild(this, "NetLinkClientPanel");
    ok_button_ = this->FindWindow(wxID_OK);
    assert(ok_button_);

    wxWindow* server_button = GetValidatedChild(this, "Server");
    server_button->Bind(
        wxEVT_RADIOBUTTON,
        std::bind(&NetLink::OnServerSelected, this, std::placeholders::_1),
        server_button->GetId());
    wxWindow* client_button = GetValidatedChild(this, "Client");
    client_button->Bind(
        wxEVT_RADIOBUTTON,
        std::bind(&NetLink::OnClientSelected, this, std::placeholders::_1),
        client_button->GetId());

    // Server panel.
    GetValidatedChild(this, "Link2P")
        ->SetValidator(widgets::OptionIntValidator(
            config::OptionID::kPrefLinkNumPlayers, 2));
    GetValidatedChild(this, "Link3P")
        ->SetValidator(widgets::OptionIntValidator(
            config::OptionID::kPrefLinkNumPlayers, 3));
    GetValidatedChild(this, "Link4P")
        ->SetValidator(widgets::OptionIntValidator(
            config::OptionID::kPrefLinkNumPlayers, 4));
    GetValidatedChild(this, "ServerIP")
        ->SetValidator(
            widgets::OptionStringValidator(config::OptionID::kGBAServerIP));
    GetValidatedChild(this, "ServerPort")
        ->SetValidator(
            widgets::OptionSpinCtrlValidator(config::OptionID::kGBAServerPort));
    GetValidatedChild(this, "LinkTimeout")
        ->SetValidator(widgets::OptionSpinCtrlValidator(
            config::OptionID::kGBALinkTimeout));

    // Client panel.
    GetValidatedChild(this, "LinkHost")
        ->SetValidator(
            widgets::OptionStringValidator(config::OptionID::kGBALinkHost));
    GetValidatedChild(this, "LinkPort")
        ->SetValidator(
            widgets::OptionSpinCtrlValidator(config::OptionID::kGBALinkPort));

    // This should intercept wxID_OK before the dialog handler gets it.
    ok_button_->Bind(
        wxEVT_BUTTON,
        std::bind(&NetLink::OnValidate, this, std::placeholders::_1),
        ok_button_->GetId());

    this->Fit();
}

void NetLink::OnServerSelected(wxCommandEvent& event) {
    server_panel_->Enable(true);
    client_panel_->Enable(false);
    ok_button_->SetLabel(_("Start!"));

    // Let the event propagate.
    event.Skip();
}

void NetLink::OnClientSelected(wxCommandEvent& event) {
    server_panel_->Enable(false);
    client_panel_->Enable(true);
    ok_button_->SetLabel(_("Connect"));

    // Let the event propagate.
    event.Skip();
}

void NetLink::OnValidate(wxCommandEvent& event) {
    static const int kHostLength = 256;
    if (!Validate() || !TransferDataFromWindow()) {
        return;
    }

    const bool is_server_mode = server_panel_->IsEnabled();
    if (!is_server_mode) {
        const bool valid =
            SetLinkServerHost(OPTION(kGBALinkHost).Get().utf8_str());
        if (!valid) {
            wxMessageBox(_("You must enter a valid host name"),
                         _("Host name invalid"), wxICON_ERROR | wxOK);
            return;
        }
    }

    // Close any previous link.
    CloseLink();
    wxString connection_message;
    wxString title;
    SetLinkTimeout(OPTION(kGBALinkTimeout));
    EnableSpeedHacks(OPTION(kGBALinkFast));
    EnableLinkServer(is_server_mode, OPTION(kPrefLinkNumPlayers) - 1);

    if (is_server_mode) {
        // Server mode.
        IP_LINK_PORT = OPTION(kGBAServerPort);
        IP_LINK_BIND_ADDRESS = OPTION(kGBAServerIP).Get();

        std::array<char, kHostLength> host;
        GetLinkServerHost(host.data(), kHostLength);
        title.Printf(_("Waiting for clients..."));
        connection_message.Printf(_("Server IP address is: %s\n"),
                       wxString(host.data(), wxConvLibc).c_str());
    } else {
        // Client mode.
        title.Printf(_("Waiting for connection..."));
        connection_message.Printf(_("Connecting to %s\n"), OPTION(kGBALinkHost).Get());
    }

    ConnectionState state = InitLink(GetConfiguredLinkMode());

    // Display a progress dialog while the connection is establishing
    if (state == LINK_NEEDS_UPDATE) {
        wxProgressDialog pdlg(
            title, connection_message, 100, this,
            wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME);

        while (state == LINK_NEEDS_UPDATE) {
            // Ask the core for updates
            std::array<char, kHostLength> message;
            state = ConnectLinkUpdate(message.data(), kHostLength);
            connection_message = wxString(message.data(), wxConvLibc);

            // Does the user want to abort?
            if (!pdlg.Pulse(connection_message)) {
                state = LINK_ABORT;
            }
        }
    }

    // The user cancelled the connection attempt.
    if (state == LINK_ABORT) {
        CloseLink();
    }

    // Something failed during init.
    if (state == LINK_ERROR) {
        CloseLink();
        wxLogError(_("Error occurred.\nPlease try again."));
    }

    if (GetLinkMode() != LINK_DISCONNECTED) {
        connection_message.Replace("\n", " ");
        systemScreenMessage(connection_message);
        event.Skip();  // all OK
    }
}

}  // namespace dialogs

#endif  // !defined(NO_LINK)
