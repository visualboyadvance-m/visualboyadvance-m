// Lua menu command handlers (Tools -> Lua). Ported from the ENABLE_LUA block
// of src/wx/cmdevents.cpp. None of these commands has an enable mask, so
// there are no Do<Name>() counterparts.
//
// When Lua is compiled out (no VBAM_ENABLE_LUA), the handlers still exist so
// the command table and menus link; they just report that the feature is not
// available in this build.

#include "qt/main-window.h"

#include <QCoreApplication>

#include "qt/log.h"

#if defined(VBAM_ENABLE_LUA)

#include <QFileDialog>

#include "qt/lua/lua_console.h"
#include "qt/lua/lua_editor.h"
#include "qt/lua/lua_engine.h"

void MainWindow::OnLuaRunScript() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Run Lua script"), QString(), tr("Lua scripts (*.lua);;All files (*)"));
    if (path.isEmpty())
        return;
    auto* console = vbam::lua::LuaConsoleEnsure(this);
    console->show();
    vbam::lua::LuaConsoleHookLog();
    if (!vbam::lua::LuaInstance().LoadFile(vbam::ToPath(path)))
        vbam::LogError(tr("Lua script failed to load — see console"));
}

void MainWindow::OnLuaStopScript() {
    vbam::lua::LuaInstance().Stop();
    if (auto* c = vbam::lua::LuaConsoleIfAny())
        c->Append("[lua] script stopped");
}

void MainWindow::OnLuaConsole() {
    auto* console = vbam::lua::LuaConsoleEnsure(this);
    vbam::lua::LuaConsoleHookLog();
    if (console->isVisible()) {
        console->hide();
    } else {
        console->show();
        console->raise();
        console->activateWindow();
    }
}

void MainWindow::OnLuaEditor() {
    auto* editor = vbam::lua::LuaEditorEnsure(this);
    if (editor->isVisible()) {
        editor->hide();
    } else {
        editor->show();
        editor->raise();
        editor->activateWindow();
    }
}

#else  // !VBAM_ENABLE_LUA

namespace {
void LuaNotEnabled() {
    vbam::LogError(QCoreApplication::translate("vbam", "Lua scripting is not enabled in this build"));
}
}  // namespace

void MainWindow::OnLuaRunScript() { LuaNotEnabled(); }
void MainWindow::OnLuaStopScript() { LuaNotEnabled(); }
void MainWindow::OnLuaConsole() { LuaNotEnabled(); }
void MainWindow::OnLuaEditor() { LuaNotEnabled(); }

#endif  // VBAM_ENABLE_LUA
