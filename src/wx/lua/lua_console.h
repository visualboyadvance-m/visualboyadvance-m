// Lua console / log window for the wx frontend.
//
// One floating wxFrame holds:
//   - a multi-line read-only wxTextCtrl that displays everything
//     emu.print() / print() / luaengine errors emit;
//   - a single-line wxTextCtrl REPL at the bottom that, on Enter,
//     calls LuaEngine::EvalRepl() and appends the result.
//
// Created lazily — first time the user picks Tools → Lua → Show
// console it's instantiated; subsequent toggles show/hide it.

#ifndef VBAM_WX_LUA_LUA_CONSOLE_H_
#define VBAM_WX_LUA_LUA_CONSOLE_H_

#include <wx/frame.h>

class wxTextCtrl;

namespace vbam {
namespace wx {

class LuaConsoleFrame : public wxFrame {
public:
    explicit LuaConsoleFrame(wxWindow* parent);
    ~LuaConsoleFrame() override;

    // Public so the LuaEngine's log sink can append output from any
    // thread / context. Always thread-safe (CallAfter under the hood).
    void Append(const wxString& line);

private:
    void OnReplEnter(wxCommandEvent& evt);
    void OnClose    (wxCloseEvent&  evt);

    wxTextCtrl* log_  = nullptr;
    wxTextCtrl* repl_ = nullptr;
};

// Returns the singleton console (creating it the first time). The
// engine's log sink points here so script output lands in the
// window. Safe to call before the wx UI is fully up — returns
// nullptr in that edge case.
LuaConsoleFrame* LuaConsoleEnsure(wxWindow* parent_for_first_create);
LuaConsoleFrame* LuaConsoleIfAny();
void             LuaConsoleHookLog();

}  // namespace wx
}  // namespace vbam

#endif  // VBAM_WX_LUA_LUA_CONSOLE_H_
