// Built-in Lua editor implementation. See lua_editor.h.

#include "wx/lua/lua_editor.h"

#include <wx/artprov.h>
#include <wx/filedlg.h>
#include <wx/file.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/toolbar.h>
#include <wx/wfstream.h>

#include "wx/lua/lua_console.h"
#include "wx/lua/lua_engine.h"

#ifdef VBAM_HAVE_WXSTC
#include <wx/stc/stc.h>
#endif

namespace vbam {
namespace wx {

namespace {
LuaEditorFrame* g_editor = nullptr;

// FCEUX-flavored Lua keyword set. wxSTC's Scintilla lexer supports up
// to 4 keyword groups; group 0 is core/standard keywords, group 1 is
// "library" identifiers we want highlighted differently.
const char* kLuaKeywords =
    "and break do else elseif end false for function goto if in "
    "local nil not or repeat return then true until while";

const char* kLuaApiKeywords =
    "emu memory joypad gui savestate rom bit "
    // Common functions:
    "frameadvance pause unpause poweron softreset message print "
    "framecount romname getsystem registerbefore registerafter "
    "registerexit readbyte readbytesigned readword readwordsigned "
    "readdword readdwordsigned writebyte writeword writedword "
    "registerread registerwrite registerexec get set read write "
    "text box line pixel create save load registerload registersave "
    "band bor bxor bnot lshift rshift arshift rol ror tobit tohex";

#ifdef VBAM_HAVE_WXSTC
void ConfigureSTC(wxStyledTextCtrl* stc) {
    stc->SetLexer(wxSTC_LEX_LUA);
    stc->SetKeyWords(0, kLuaKeywords);
    stc->SetKeyWords(1, kLuaApiKeywords);

    wxFont font(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
    stc->StyleSetFont(wxSTC_STYLE_DEFAULT, font);
    stc->StyleClearAll();

    // Scintilla default Lua style mapping:
    //   wxSTC_LUA_DEFAULT     0   plain text
    //   wxSTC_LUA_COMMENT     1   --[[ block ]] / -- line
    //   wxSTC_LUA_COMMENTLINE 2
    //   wxSTC_LUA_NUMBER      4
    //   wxSTC_LUA_WORD        5   keyword group 0 (kLuaKeywords)
    //   wxSTC_LUA_STRING      6   "double" / 'single'
    //   wxSTC_LUA_LITERALSTRING 8 [[long]]
    //   wxSTC_LUA_OPERATOR    10
    //   wxSTC_LUA_IDENTIFIER  11
    //   wxSTC_LUA_WORD2       12  kLuaApiKeywords
    stc->StyleSetForeground(wxSTC_LUA_COMMENT,     wxColour( 96, 128,  96));
    stc->StyleSetForeground(wxSTC_LUA_COMMENTLINE, wxColour( 96, 128,  96));
    stc->StyleSetForeground(wxSTC_LUA_NUMBER,      wxColour(160,  64, 160));
    stc->StyleSetForeground(wxSTC_LUA_WORD,        wxColour(  0,   0, 192));
    stc->StyleSetBold      (wxSTC_LUA_WORD,        true);
    stc->StyleSetForeground(wxSTC_LUA_STRING,      wxColour(160, 64,   64));
    stc->StyleSetForeground(wxSTC_LUA_LITERALSTRING, wxColour(160, 64, 64));
    stc->StyleSetForeground(wxSTC_LUA_OPERATOR,    wxColour( 64,  64,  64));
    stc->StyleSetForeground(wxSTC_LUA_WORD2,       wxColour(  0, 128, 128));
    stc->StyleSetBold      (wxSTC_LUA_WORD2,       true);

    stc->SetMarginType (0, wxSTC_MARGIN_NUMBER);
    stc->SetMarginWidth(0, stc->TextWidth(wxSTC_STYLE_LINENUMBER, "_99999"));
    stc->SetUseTabs(false);
    stc->SetTabWidth(2);
    stc->SetIndent(2);
    stc->SetIndentationGuides(wxSTC_IV_LOOKBOTH);
    stc->SetEdgeColumn(80);
    stc->SetEdgeMode(wxSTC_EDGE_LINE);
}
#endif  // VBAM_HAVE_WXSTC

}  // namespace

LuaEditorFrame::LuaEditorFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Lua Editor",
              wxDefaultPosition, wxSize(820, 540),
              wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT) {
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    // Toolbar.
    toolbar_ = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxTB_HORIZONTAL | wxTB_TEXT);
    toolbar_->AddTool(kIdOpen,   _("Open"),
                      wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR));
    toolbar_->AddTool(kIdSave,   _("Save"),
                      wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR));
    toolbar_->AddTool(kIdSaveAs, _("Save As"),
                      wxArtProvider::GetBitmap(wxART_FILE_SAVE_AS, wxART_TOOLBAR));
    toolbar_->AddSeparator();
    toolbar_->AddTool(kIdRun,    _("Run"),
                      wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR));
    toolbar_->AddTool(kIdStop,   _("Stop"),
                      wxArtProvider::GetBitmap(wxART_DELETE, wxART_TOOLBAR));
    toolbar_->Realize();
    sizer->Add(toolbar_, 0, wxEXPAND);

    // Editor widget.
#ifdef VBAM_HAVE_WXSTC
    edit_ = new wxStyledTextCtrl(this, wxID_ANY);
    ConfigureSTC(edit_);
#else
    edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_RICH2 | wxTE_DONTWRAP);
    edit_->SetFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
#endif
    sizer->Add(edit_, 1, wxEXPAND | wxALL, 2);

    SetSizer(sizer);
    CreateStatusBar(2);
    SetStatusText(_("(unsaved)"));

    Bind(wxEVT_TOOL,         &LuaEditorFrame::DoOpen,   this, kIdOpen);
    Bind(wxEVT_TOOL,         &LuaEditorFrame::DoSave,   this, kIdSave);
    Bind(wxEVT_TOOL,         &LuaEditorFrame::DoSaveAs, this, kIdSaveAs);
    Bind(wxEVT_TOOL,         &LuaEditorFrame::DoRun,    this, kIdRun);
    Bind(wxEVT_TOOL,         &LuaEditorFrame::DoStop,   this, kIdStop);
    Bind(wxEVT_CLOSE_WINDOW, &LuaEditorFrame::OnClose,  this);
#ifdef VBAM_HAVE_WXSTC
    Bind(wxEVT_STC_MODIFIED, &LuaEditorFrame::OnTextModified, this);
#else
    edit_->Bind(wxEVT_TEXT,  &LuaEditorFrame::OnTextModified, this);
#endif

    SetText(
        "-- Lua scripting (FCEUX-style API)\n"
        "-- Quick reference:\n"
        "--   memory.readbyte(addr) / memory.writebyte(addr, v)\n"
        "--   joypad.set({A=true, start=true})\n"
        "--   gui.text(10, 10, 'hello')\n"
        "--   emu.registerbefore(function() ... end)\n"
        "\n"
        "emu.print('hello from lua ' .. _VERSION)\n");
    dirty_ = false;
    UpdateTitle();
}

LuaEditorFrame::~LuaEditorFrame() {
    if (g_editor == this) g_editor = nullptr;
}

wxString LuaEditorFrame::GetText() const {
#ifdef VBAM_HAVE_WXSTC
    return edit_->GetText();
#else
    return edit_->GetValue();
#endif
}

void LuaEditorFrame::SetText(const wxString& s) {
#ifdef VBAM_HAVE_WXSTC
    edit_->SetText(s);
    edit_->EmptyUndoBuffer();
#else
    edit_->ChangeValue(s);
#endif
}

bool LuaEditorFrame::ConfirmDiscardIfDirty() {
    if (!dirty_) return true;
    int res = wxMessageBox(_("Discard unsaved changes?"),
                           _("Lua Editor"),
                           wxYES_NO | wxICON_QUESTION, this);
    return res == wxYES;
}

bool LuaEditorFrame::SaveTo(const wxString& path) {
    wxFile f;
    if (!f.Create(path, true /*overwrite*/) && !f.Open(path, wxFile::write)) {
        wxLogError(_("Cannot write %s"), path);
        return false;
    }
    wxString text = GetText();
    auto utf8 = text.ToUTF8();
    if (!f.Write(utf8.data(), utf8.length())) {
        wxLogError(_("Write failed"));
        return false;
    }
    current_path_ = path;
    dirty_        = false;
    UpdateTitle();
    return true;
}

void LuaEditorFrame::UpdateTitle() {
    wxString name = current_path_.empty() ? wxString("(unsaved)")
                                          : current_path_;
    SetTitle("Lua Editor — " + name + (dirty_ ? "*" : ""));
    SetStatusText(name, 0);
}

void LuaEditorFrame::OpenFile(const wxString& path) {
    wxFile f(path, wxFile::read);
    if (!f.IsOpened()) { wxLogError(_("Cannot open %s"), path); return; }
    wxString text;
    char buf[4096];
    while (true) {
        ssize_t n = f.Read(buf, sizeof buf);
        if (n <= 0) break;
        text += wxString::FromUTF8(buf, n);
    }
    SetText(text);
    current_path_ = path;
    dirty_        = false;
    UpdateTitle();
}

void LuaEditorFrame::DoOpen(wxCommandEvent&) {
    if (!ConfirmDiscardIfDirty()) return;
    wxFileDialog dlg(this, _("Open Lua script"), "", "",
        _("Lua scripts (*.lua)|*.lua|All files|*"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    OpenFile(dlg.GetPath());
}

void LuaEditorFrame::DoSave(wxCommandEvent& evt) {
    if (current_path_.empty()) { DoSaveAs(evt); return; }
    SaveTo(current_path_);
}

void LuaEditorFrame::DoSaveAs(wxCommandEvent&) {
    wxFileDialog dlg(this, _("Save Lua script"), "", current_path_,
        _("Lua scripts (*.lua)|*.lua|All files|*"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    SaveTo(dlg.GetPath());
}

void LuaEditorFrame::DoRun(wxCommandEvent&) {
    // Make sure the console is up so script output is visible.
    auto* console = LuaConsoleEnsure(GetParent());
    console->Show();
    LuaConsoleHookLog();

    wxString text = GetText();
    std::string name = current_path_.empty()
        ? std::string("<editor>")
        : std::string(current_path_.mb_str());
    if (!LuaInstance().LoadString(std::string(text.mb_str()), name)) {
        wxLogError(_("Lua script failed to load — see console"));
    }
}

void LuaEditorFrame::DoStop(wxCommandEvent&) {
    LuaInstance().Stop();
    if (auto* c = LuaConsoleIfAny())
        c->Append("[lua] script stopped");
}

void LuaEditorFrame::OnTextModified(wxCommandEvent&) {
    if (!dirty_) {
        dirty_ = true;
        UpdateTitle();
    }
}

void LuaEditorFrame::OnClose(wxCloseEvent& evt) {
    if (evt.CanVeto() && dirty_ && !ConfirmDiscardIfDirty()) {
        evt.Veto();
        return;
    }
    if (evt.CanVeto()) {
        evt.Veto();
        Hide();
    } else {
        Destroy();
    }
}

LuaEditorFrame* LuaEditorEnsure(wxWindow* parent_for_first_create) {
    if (!g_editor)
        g_editor = new LuaEditorFrame(parent_for_first_create);
    return g_editor;
}

LuaEditorFrame* LuaEditorIfAny() { return g_editor; }

}  // namespace wx
}  // namespace vbam
