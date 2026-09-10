#include "qt/lua/lua_console.h"

#if defined(VBAM_ENABLE_LUA)

#include <cstdio>

#include <QCloseEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "qt/log.h"
#include "qt/lua/lua_engine.h"

namespace vbam {
namespace lua {

namespace {
LuaConsole* g_console = nullptr;
}

LuaConsole::LuaConsole(QWidget* parent) : dialogs::BaseDialog(parent, "LuaConsole") {
    setWindowTitle(tr("Lua Console"));
    setModal(false);
    resize(640, 380);

    auto* layout = new QVBoxLayout(this);

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setLineWrapMode(QPlainTextEdit::NoWrap);
    log_->setMaximumBlockCount(10000);

    repl_ = new QLineEdit(this);
    repl_->setPlaceholderText(tr("Lua statement or expression, Enter to evaluate"));

    // Use a monospaced font for both controls — script output is
    // mostly hex addresses / aligned tables.
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    log_->setFont(mono);
    repl_->setFont(mono);

    auto* buttons = new QHBoxLayout();
    auto* run = new QPushButton(tr("&Run script..."), this);
    auto* stop = new QPushButton(tr("&Stop"), this);
    auto* clear = new QPushButton(tr("&Clear"), this);
    auto* close = new QPushButton(tr("Close"), this);
    buttons->addWidget(run);
    buttons->addWidget(stop);
    buttons->addWidget(clear);
    buttons->addStretch(1);
    buttons->addWidget(close);

    layout->addWidget(log_, 1);
    layout->addWidget(repl_, 0);
    layout->addLayout(buttons);

    connect(repl_, &QLineEdit::returnPressed, this, &LuaConsole::OnReplEnter);
    connect(run, &QPushButton::clicked, this, &LuaConsole::OnRun);
    connect(stop, &QPushButton::clicked, this, &LuaConsole::OnStop);
    connect(clear, &QPushButton::clicked, this, &LuaConsole::OnClear);
    connect(close, &QPushButton::clicked, this, &QWidget::hide);
}

LuaConsole::~LuaConsole() {
    if (g_console == this) g_console = nullptr;
}

void LuaConsole::Append(const QString& line) {
    // Free-thread safe: a queued invocation marshals onto the GUI thread.
    QMetaObject::invokeMethod(this, "AppendOnGuiThread", Qt::QueuedConnection,
                              Q_ARG(QString, line));
}

void LuaConsole::AppendOnGuiThread(const QString& line) {
    if (!log_) return;
    QString text = line;
    while (text.endsWith('\n')) text.chop(1);
    log_->appendPlainText(text);
}

void LuaConsole::OnReplEnter() {
    const QString line = repl_->text();
    repl_->clear();
    if (line.isEmpty()) return;

    Append("> " + line);
    const std::string result = LuaInstance().EvalRepl(line.toStdString());
    if (!result.empty()) Append(QString::fromStdString(result));
}

void LuaConsole::OnRun() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Run Lua script"), QString(),
        tr("Lua scripts (*.lua);;All files (*)"));
    if (path.isEmpty()) return;
    LuaConsoleHookLog();
    if (!LuaInstance().LoadFile(vbam::ToPath(path)))
        vbam::LogError(tr("Lua script failed to load — see console"));
}

void LuaConsole::OnStop() {
    LuaInstance().Stop();
    Append("[lua] script stopped");
}

void LuaConsole::OnClear() {
    log_->clear();
}

void LuaConsole::closeEvent(QCloseEvent* event) {
    // Hide instead of destroying so the same window can be reopened
    // with its log history intact.
    event->ignore();
    hide();
}

LuaConsole* LuaConsoleEnsure(QWidget* parent_for_first_create) {
    if (!g_console)
        g_console = new LuaConsole(parent_for_first_create);
    return g_console;
}

LuaConsole* LuaConsoleIfAny() { return g_console; }

void LuaConsoleHookLog() {
    LuaInstance().SetLogSink([](const std::string& s) {
        if (auto* c = LuaConsoleIfAny())
            c->Append(QString::fromStdString(s));
        else
            std::fprintf(stderr, "[lua] %s\n", s.c_str());
    });
}

}  // namespace lua
}  // namespace vbam

#endif  // VBAM_ENABLE_LUA
