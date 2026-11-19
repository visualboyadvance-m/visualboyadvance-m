/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

// Minimal wxWidgets application demonstrating wxSparkle usage.
//
// Usage: wxsparkle-example [--check | --install] [--eddsa-key=KEY] [--lang=LANG] [appcast-url]
//
//   --check          check for updates (with UI) immediately after startup
//   --install        check for updates and install them without asking
//   --eddsa-key=KEY  EdDSA public key (base64) to verify updates with
//   --lang=LANG      UI language of wxSparkle dialogs, e.g. "de" or "pt_BR"
//
// Note that this example doesn't actually install anything: it uses the
// win_sparkle_set_user_run_installer_callback() to show where the verified
// update was downloaded to instead of launching it.

#include <wx/wx.h>
#include <wx/cmdline.h>

#include <winsparkle.h>

namespace
{

// Default feed used if no URL is passed on the command line. On macOS, the
// SUFeedURL key in Info.plist is used instead when the URL isn't set at all.
const char DEFAULT_APPCAST_URL[] = "https://winsparkle.org/example/appcast.xml";

enum
{
    ID_CHECK_UPDATES = wxID_HIGHEST + 1,
    ID_CHECK_AND_INSTALL,
    ID_CHECK_SILENT
};

class ExampleFrame : public wxFrame
{
public:
    ExampleFrame()
        : wxFrame(NULL, wxID_ANY, "wxSparkle Example", wxDefaultPosition, wxSize(480, 300))
    {
        wxMenu *fileMenu = new wxMenu;
        fileMenu->Append(wxID_EXIT);

        wxMenu *helpMenu = new wxMenu;
        helpMenu->Append(ID_CHECK_UPDATES, "&Check for Updates...");
        helpMenu->Append(ID_CHECK_AND_INSTALL, "Check and &Install Updates...");
        helpMenu->Append(ID_CHECK_SILENT, "Check for Updates &Without UI");
        helpMenu->Append(wxID_ABOUT);

        wxMenuBar *menuBar = new wxMenuBar;
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(helpMenu, "&Help");
        SetMenuBar(menuBar);

        wxPanel *panel = new wxPanel(this);
        wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(new wxStaticText(panel, wxID_ANY,
                       "This is wxSparkle example application, version 1.0.\n\n"
                       "Use Help > Check for Updates... to check for updates."),
                   wxSizerFlags(1).Expand().Border(wxALL, 20));
        sizer->Add(new wxButton(panel, ID_CHECK_UPDATES, "Check for Updates..."),
                   wxSizerFlags().Center().Border(wxALL, 20));
        panel->SetSizer(sizer);

        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
        Bind(wxEVT_MENU, [](wxCommandEvent&) { win_sparkle_check_update_with_ui(); }, ID_CHECK_UPDATES);
        Bind(wxEVT_BUTTON, [](wxCommandEvent&) { win_sparkle_check_update_with_ui(); }, ID_CHECK_UPDATES);
        Bind(wxEVT_MENU, [](wxCommandEvent&) { win_sparkle_check_update_with_ui_and_install(); }, ID_CHECK_AND_INSTALL);
        Bind(wxEVT_MENU, [](wxCommandEvent&) { win_sparkle_check_update_without_ui(); }, ID_CHECK_SILENT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&)
            {
                wxMessageBox(wxString::Format("wxSparkle %s example", WIN_SPARKLE_VERSION_STRING),
                             "About", wxOK | wxICON_INFORMATION, this);
            }, wxID_ABOUT);
    }
};


// Callbacks may be called from any thread, so only use thread-safe code:

int WIN_SPARKLE_CALL CanShutdown()
{
    return 1; // no unsaved documents in this example
}

void WIN_SPARKLE_CALL ShutdownRequest()
{
    wxTheApp->CallAfter([]
        {
            if ( wxWindow *top = wxTheApp->GetTopWindow() )
                top->Close();
        });
}

void WIN_SPARKLE_CALL DidFindUpdate()
{
    wxTheApp->CallAfter([] { wxLogStatus("Update found."); });
}

void WIN_SPARKLE_CALL DidNotFindUpdate()
{
    wxTheApp->CallAfter([] { wxLogStatus("No update found."); });
}

void WIN_SPARKLE_CALL UpdateError()
{
    wxTheApp->CallAfter([] { wxLogStatus("Update error."); });
}

int WIN_SPARKLE_CALL RunInstaller(const wchar_t *path)
{
    const wxString file(path);
    wxTheApp->CallAfter([file]
        {
            wxLogStatus("Update downloaded to %s", file);
            wxMessageBox("The update was downloaded and verified:\n\n" + file +
                         "\n\nA real application would launch it now (by returning 0 "
                         "from this callback, or by not setting it at all).",
                         "wxSparkle Example", wxOK | wxICON_INFORMATION);
        });
    return 1; // handled
}


class ExampleApp : public wxApp
{
public:
    bool OnInit() override
    {
        if ( !wxApp::OnInit() )
            return false;

        SetAppName("wxsparkle-example");
        SetAppDisplayName("wxSparkle Example");
        SetVendorName("wxSparkle");

        ExampleFrame *frame = new ExampleFrame;
        frame->CreateStatusBar();
        frame->Show();

        // Configure wxSparkle -- this must be done before win_sparkle_init():
        win_sparkle_set_appcast_url(m_appcastURL.empty() ? DEFAULT_APPCAST_URL
                                                         : m_appcastURL.utf8_str().data());
        win_sparkle_set_app_details(L"wxSparkle", L"wxSparkle Example", L"1.0");

        // Use EdDSA signatures in real applications, e.g.:
        //   win_sparkle_set_eddsa_public_key("...base64 public key from winsparkle-tool...");
        if ( !m_eddsaKey.empty() && !win_sparkle_set_eddsa_public_key(m_eddsaKey.utf8_str()) )
            wxLogError("Invalid EdDSA public key.");

        win_sparkle_set_user_run_installer_callback(&RunInstaller);

        // the UI language is detected automatically, but can be overridden
        if ( !m_lang.empty() )
            win_sparkle_set_lang(m_lang.utf8_str());

        win_sparkle_set_can_shutdown_callback(&CanShutdown);
        win_sparkle_set_shutdown_request_callback(&ShutdownRequest);
        win_sparkle_set_did_find_update_callback(&DidFindUpdate);
        win_sparkle_set_did_not_find_update_callback(&DidNotFindUpdate);
        win_sparkle_set_error_callback(&UpdateError);

        // Initialize wxSparkle as soon as the app itself is initialized,
        // right before entering the event loop:
        win_sparkle_init();

        if ( m_installNow )
            win_sparkle_check_update_with_ui_and_install();
        else if ( m_checkNow )
            win_sparkle_check_update_with_ui();

        return true;
    }

    int OnExit() override
    {
        win_sparkle_cleanup();
        return wxApp::OnExit();
    }

    void OnInitCmdLine(wxCmdLineParser& parser) override
    {
        wxApp::OnInitCmdLine(parser);
        parser.AddSwitch("", "check", "check for updates immediately");
        parser.AddSwitch("", "install", "check for updates and install them immediately");
        parser.AddOption("", "eddsa-key", "EdDSA public key to verify updates with");
        parser.AddOption("", "lang", "UI language of the update dialogs");
        parser.AddParam("appcast-url", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
    }

    bool OnCmdLineParsed(wxCmdLineParser& parser) override
    {
        m_checkNow = parser.Found("check");
        m_installNow = parser.Found("install");
        parser.Found("eddsa-key", &m_eddsaKey);
        parser.Found("lang", &m_lang);
        if ( parser.GetParamCount() > 0 )
            m_appcastURL = parser.GetParam(0);
        return wxApp::OnCmdLineParsed(parser);
    }

private:
    wxString m_appcastURL;
    wxString m_eddsaKey;
    wxString m_lang;
    bool m_checkNow = false;
    bool m_installNow = false;
};

} // anonymous namespace

wxIMPLEMENT_APP(ExampleApp);
