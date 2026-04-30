// Built-in Lua editor for the wx frontend.
//
// One floating wxFrame holds:
//   - a syntax-highlighted text editor (wxStyledTextCtrl when
//     wxWidgets ships with the STC component; plain wxTextCtrl
//     otherwise — VBAM_HAVE_WXSTC is the build-time switch);
//   - a toolbar with Open / Save / Save As / Run / Stop buttons;
//   - a status bar showing the current file path and line/col.
//
// "Run" hands the buffer to LuaEngine::LoadString. "Stop" calls
// LuaEngine::Stop. Output goes to the Lua console window if open.

#ifndef VBAM_WX_LUA_LUA_EDITOR_H_
#define VBAM_WX_LUA_LUA_EDITOR_H_

#include <wx/frame.h>

class wxTextCtrl;
class wxToolBar;

#ifdef VBAM_HAVE_WXSTC
class wxStyledTextCtrl;
#endif

namespace vbam {
namespace wx {

class LuaEditorFrame : public wxFrame {
public:
    explicit LuaEditorFrame(wxWindow* parent);
    ~LuaEditorFrame() override;

    // Replace the editor's contents and reset the dirty/path state.
    void OpenFile(const wxString& path);

private:
    enum {
        kIdOpen = 1, kIdSave, kIdSaveAs, kIdRun, kIdStop, kIdToolbarFirst,
    };

    void DoOpen   (wxCommandEvent& evt);
    void DoSave   (wxCommandEvent& evt);
    void DoSaveAs (wxCommandEvent& evt);
    void DoRun    (wxCommandEvent& evt);
    void DoStop   (wxCommandEvent& evt);
    void OnTextModified(wxCommandEvent& evt);
    void OnClose  (wxCloseEvent&  evt);

    wxString GetText() const;
    void     SetText(const wxString& s);
    bool     ConfirmDiscardIfDirty();
    bool     SaveTo (const wxString& path);
    void     UpdateTitle();

#ifdef VBAM_HAVE_WXSTC
    wxStyledTextCtrl* edit_ = nullptr;
#else
    wxTextCtrl*       edit_ = nullptr;
#endif
    wxToolBar* toolbar_     = nullptr;
    wxString   current_path_;
    bool       dirty_ = false;
};

LuaEditorFrame* LuaEditorEnsure(wxWindow* parent_for_first_create);
LuaEditorFrame* LuaEditorIfAny();

}  // namespace wx
}  // namespace vbam

#endif  // VBAM_WX_LUA_LUA_EDITOR_H_
