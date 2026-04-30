#include "wx/lua/lua_console.h"

#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "wx/lua/lua_engine.h"

namespace vbam {
namespace wx {

namespace {
LuaConsoleFrame* g_console = nullptr;
}

LuaConsoleFrame::LuaConsoleFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Lua Console",
              wxDefaultPosition, wxSize(640, 380),
              wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT) {
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    log_  = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_DONTWRAP);
    repl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    sizer->Add(log_,  1, wxEXPAND | wxALL, 4);
    sizer->Add(repl_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);
    SetSizer(sizer);

    // Use a monospaced font for both controls — script output is
    // mostly hex addresses / aligned tables.
    wxFont mono(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
    log_->SetFont(mono);
    repl_->SetFont(mono);

    repl_->Bind(wxEVT_TEXT_ENTER, &LuaConsoleFrame::OnReplEnter, this);
    Bind     (wxEVT_CLOSE_WINDOW, &LuaConsoleFrame::OnClose,     this);
}

LuaConsoleFrame::~LuaConsoleFrame() {
    if (g_console == this) g_console = nullptr;
}

void LuaConsoleFrame::Append(const wxString& line) {
    // Free-thread safe: CallAfter marshals onto the UI thread.
    auto* self = this;
    CallAfter([self, line]() {
        if (!self->log_) return;
        self->log_->AppendText(line);
        if (line.empty() || line.Last() != '\n')
            self->log_->AppendText("\n");
    });
}

void LuaConsoleFrame::OnReplEnter(wxCommandEvent&) {
    wxString line = repl_->GetValue();
    repl_->SetValue("");
    if (line.IsEmpty()) return;

    Append("> " + line);
    std::string result = LuaInstance().EvalRepl(std::string(line.mb_str()));
    if (!result.empty()) Append(wxString::FromUTF8(result.c_str()));
}

void LuaConsoleFrame::OnClose(wxCloseEvent& evt) {
    if (evt.CanVeto()) {
        // Hide instead of destroying so the same frame can be
        // reopened with its log history intact.
        evt.Veto();
        Hide();
    } else {
        Destroy();
    }
}

LuaConsoleFrame* LuaConsoleEnsure(wxWindow* parent_for_first_create) {
    if (!g_console)
        g_console = new LuaConsoleFrame(parent_for_first_create);
    return g_console;
}

LuaConsoleFrame* LuaConsoleIfAny() { return g_console; }

void LuaConsoleHookLog() {
    LuaInstance().SetLogSink([](const std::string& s) {
        if (auto* c = LuaConsoleIfAny())
            c->Append(wxString::FromUTF8(s.c_str()));
        else
            std::fprintf(stderr, "[lua] %s\n", s.c_str());
    });
}

}  // namespace wx
}  // namespace vbam
