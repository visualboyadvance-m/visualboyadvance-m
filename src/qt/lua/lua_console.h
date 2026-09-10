// Lua console / log window for the Qt frontend.
//
// One non-modal dialog holds:
//   - a read-only QPlainTextEdit that displays everything emu.print() /
//     print() / engine errors emit;
//   - a single-line REPL at the bottom that, on Enter, calls
//     LuaEngine::EvalRepl() and appends the result;
//   - Run... (pick a script file), Stop and Clear buttons.
//
// Created lazily — first time the user picks Tools → Lua → Show
// console it's instantiated; subsequent toggles show/hide it.

#ifndef VBAM_QT_LUA_LUA_CONSOLE_H_
#define VBAM_QT_LUA_LUA_CONSOLE_H_

#if defined(VBAM_ENABLE_LUA)

#include <QString>

#include "qt/dialogs/base-dialog.h"

class QLineEdit;
class QPlainTextEdit;

namespace vbam {
namespace lua {

class LuaConsole final : public dialogs::BaseDialog {
    Q_OBJECT

public:
    explicit LuaConsole(QWidget* parent);
    ~LuaConsole() override;

    // Public so the LuaEngine's log sink can append output from any
    // thread / context. Always thread-safe (queued onto the GUI thread).
    void Append(const QString& line);

protected:
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void OnReplEnter();
    void OnRun();
    void OnStop();
    void OnClear();
    void AppendOnGuiThread(const QString& line);

private:
    QPlainTextEdit* log_ = nullptr;
    QLineEdit* repl_ = nullptr;
};

// Returns the singleton console (creating it the first time). The
// engine's log sink points here so script output lands in the window.
LuaConsole* LuaConsoleEnsure(QWidget* parent_for_first_create);
LuaConsole* LuaConsoleIfAny();
void        LuaConsoleHookLog();

}  // namespace lua
}  // namespace vbam

#endif  // VBAM_ENABLE_LUA

#endif  // VBAM_QT_LUA_LUA_CONSOLE_H_
