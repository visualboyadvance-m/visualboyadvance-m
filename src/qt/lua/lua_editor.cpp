// Built-in Lua editor implementation. See lua_editor.h.

#include "qt/lua/lua_editor.h"

#if defined(VBAM_ENABLE_LUA)

#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

#include "qt/log.h"
#include "qt/lua/lua_console.h"
#include "qt/lua/lua_engine.h"

namespace vbam {
namespace lua {

namespace {
LuaEditor* g_editor = nullptr;

const char* const kLuaKeywords[] = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
    "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then",
    "true", "until", "while", nullptr,
};

// FCEUX-flavored VBA-M API names, highlighted as a second keyword group.
const char* const kLuaApiKeywords[] = {
    "emu", "memory", "joypad", "gui", "savestate", "rom", "bit",
    "frameadvance", "pause", "unpause", "poweron", "softreset", "message",
    "print", "framecount", "romname", "getsystem", "registerbefore",
    "registerafter", "registerexit", "readbyte", "readbytesigned", "readword",
    "readwordsigned", "readdword", "readdwordsigned", "writebyte", "writeword",
    "writedword", "registerread", "registerwrite", "registerexec", "get", "set",
    "read", "write", "text", "box", "line", "pixel", "create", "save", "load",
    "registerload", "registersave", "band", "bor", "bxor", "bnot", "lshift",
    "rshift", "arshift", "rol", "ror", "tobit", "tohex", nullptr,
};

const char* const kDefaultText =
    "-- Lua scripting (FCEUX-style API)\n"
    "-- Quick reference:\n"
    "--   memory.readbyte(addr) / memory.writebyte(addr, v)\n"
    "--   joypad.set({A=true, start=true})\n"
    "--   gui.text(10, 10, 'hello')\n"
    "--   emu.registerbefore(function() ... end)\n"
    "\n"
    "emu.print('hello from lua ' .. _VERSION)\n";

}  // namespace

// ----------------------------------------------------------------------------
// LuaHighlighter
// ----------------------------------------------------------------------------

LuaHighlighter::LuaHighlighter(QTextDocument* document) : QSyntaxHighlighter(document) {
    QTextCharFormat keyword;
    keyword.setForeground(QColor(0, 0, 192));
    keyword.setFontWeight(QFont::Bold);
    for (const char* const* k = kLuaKeywords; *k; ++k) {
        rules_.push_back({QRegularExpression(QStringLiteral("\\b%1\\b").arg(*k)), keyword});
    }

    QTextCharFormat api;
    api.setForeground(QColor(0, 128, 128));
    api.setFontWeight(QFont::Bold);
    for (const char* const* k = kLuaApiKeywords; *k; ++k) {
        rules_.push_back({QRegularExpression(QStringLiteral("\\b%1\\b").arg(*k)), api});
    }

    QTextCharFormat number;
    number.setForeground(QColor(160, 64, 160));
    rules_.push_back(
        {QRegularExpression(QStringLiteral("\\b(0[xX][0-9a-fA-F]+|\\d+(\\.\\d+)?([eE][-+]?\\d+)?)\\b")),
         number});

    QTextCharFormat string;
    string.setForeground(QColor(160, 64, 64));
    rules_.push_back({QRegularExpression(QStringLiteral("\"(\\\\.|[^\"\\\\])*\"")), string});
    rules_.push_back({QRegularExpression(QStringLiteral("'(\\\\.|[^'\\\\])*'")), string});

    comment_format_.setForeground(QColor(96, 128, 96));
    // Line comment, but not the start of a block comment.
    rules_.push_back({QRegularExpression(QStringLiteral("--(?!\\[\\[).*$")), comment_format_});

    block_comment_start_ = QRegularExpression(QStringLiteral("--\\[\\["));
    block_comment_end_ = QRegularExpression(QStringLiteral("\\]\\]"));
}

void LuaHighlighter::highlightBlock(const QString& text) {
    for (const Rule& rule : rules_) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // Multi-line block comments: state 1 = inside a comment.
    setCurrentBlockState(0);
    int start = 0;
    if (previousBlockState() != 1) {
        start = text.indexOf(block_comment_start_);
    }
    while (start >= 0) {
        const QRegularExpressionMatch end = block_comment_end_.match(text, start);
        int length;
        if (!end.hasMatch()) {
            setCurrentBlockState(1);
            length = text.length() - start;
        } else {
            length = end.capturedEnd() - start;
        }
        setFormat(start, length, comment_format_);
        start = text.indexOf(block_comment_start_, start + length);
    }
}

// ----------------------------------------------------------------------------
// LuaEditor
// ----------------------------------------------------------------------------

LuaEditor::LuaEditor(QWidget* parent) : dialogs::BaseDialog(parent, "LuaEditor") {
    setModal(false);
    resize(820, 540);

    auto* layout = new QVBoxLayout(this);

    auto* buttons = new QHBoxLayout();
    auto* new_btn = new QPushButton(tr("&New"), this);
    auto* open = new QPushButton(tr("&Open..."), this);
    auto* save = new QPushButton(tr("&Save"), this);
    auto* save_as = new QPushButton(tr("Save &As..."), this);
    auto* run = new QPushButton(tr("&Run"), this);
    auto* stop = new QPushButton(tr("S&top"), this);
    buttons->addWidget(new_btn);
    buttons->addWidget(open);
    buttons->addWidget(save);
    buttons->addWidget(save_as);
    buttons->addSpacing(12);
    buttons->addWidget(run);
    buttons->addWidget(stop);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    edit_ = new QPlainTextEdit(this);
    edit_->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    edit_->setFont(mono);
    edit_->setTabStopDistance(2 * QFontMetrics(mono).horizontalAdvance(' '));
    highlighter_ = new LuaHighlighter(edit_->document());
    layout->addWidget(edit_, 1);

    status_ = new QLabel(this);
    layout->addWidget(status_, 0);

    connect(new_btn, &QPushButton::clicked, this, &LuaEditor::DoNew);
    connect(open, &QPushButton::clicked, this, &LuaEditor::DoOpen);
    connect(save, &QPushButton::clicked, this, &LuaEditor::DoSave);
    connect(save_as, &QPushButton::clicked, this, &LuaEditor::DoSaveAs);
    connect(run, &QPushButton::clicked, this, &LuaEditor::DoRun);
    connect(stop, &QPushButton::clicked, this, &LuaEditor::DoStop);
    connect(edit_, &QPlainTextEdit::textChanged, this, &LuaEditor::OnTextModified);

    SetText(QString::fromUtf8(kDefaultText));
    dirty_ = false;
    UpdateTitle();
}

LuaEditor::~LuaEditor() {
    if (g_editor == this) g_editor = nullptr;
}

QString LuaEditor::GetText() const {
    return edit_->toPlainText();
}

void LuaEditor::SetText(const QString& s) {
    // setPlainText resets the undo stack and emits textChanged; the caller
    // resets dirty_ afterwards.
    edit_->setPlainText(s);
}

bool LuaEditor::ConfirmDiscardIfDirty() {
    if (!dirty_) return true;
    const int res = QMessageBox::question(this, tr("Lua Editor"),
                                          tr("Discard unsaved changes?"),
                                          QMessageBox::Yes | QMessageBox::No);
    return res == QMessageBox::Yes;
}

bool LuaEditor::SaveTo(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        vbam::LogError(tr("Cannot write %1").arg(path));
        return false;
    }
    const QByteArray utf8 = GetText().toUtf8();
    if (f.write(utf8) != utf8.size()) {
        vbam::LogError(tr("Write failed"));
        return false;
    }
    current_path_ = path;
    dirty_        = false;
    UpdateTitle();
    return true;
}

void LuaEditor::UpdateTitle() {
    const QString name = current_path_.isEmpty() ? tr("(unsaved)") : current_path_;
    setWindowTitle(tr("Lua Editor") + QStringLiteral(" — ") + name +
                   (dirty_ ? QStringLiteral("*") : QString()));
    status_->setText(name);
}

void LuaEditor::OpenFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        vbam::LogError(tr("Cannot open %1").arg(path));
        return;
    }
    const QString text = QString::fromUtf8(f.readAll());
    SetText(text);
    current_path_ = path;
    dirty_        = false;
    UpdateTitle();
}

void LuaEditor::DoNew() {
    if (!ConfirmDiscardIfDirty()) return;
    SetText(QString::fromUtf8(kDefaultText));
    current_path_.clear();
    dirty_ = false;
    UpdateTitle();
}

void LuaEditor::DoOpen() {
    if (!ConfirmDiscardIfDirty()) return;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Lua script"), QString(), tr("Lua scripts (*.lua);;All files (*)"));
    if (path.isEmpty()) return;
    OpenFile(path);
}

void LuaEditor::DoSave() {
    if (current_path_.isEmpty()) { DoSaveAs(); return; }
    SaveTo(current_path_);
}

void LuaEditor::DoSaveAs() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Lua script"), current_path_, tr("Lua scripts (*.lua);;All files (*)"));
    if (path.isEmpty()) return;
    SaveTo(path);
}

void LuaEditor::DoRun() {
    // Make sure the console is up so script output is visible.
    auto* console = LuaConsoleEnsure(parentWidget());
    console->show();
    LuaConsoleHookLog();

    const std::string name = current_path_.isEmpty() ? std::string("<editor>")
                                                     : vbam::ToPath(current_path_);
    if (!LuaInstance().LoadString(GetText().toStdString(), name)) {
        vbam::LogError(tr("Lua script failed to load — see console"));
    }
}

void LuaEditor::DoStop() {
    LuaInstance().Stop();
    if (auto* c = LuaConsoleIfAny())
        c->Append("[lua] script stopped");
}

void LuaEditor::OnTextModified() {
    if (!dirty_) {
        dirty_ = true;
        UpdateTitle();
    }
}

void LuaEditor::closeEvent(QCloseEvent* event) {
    if (dirty_ && !ConfirmDiscardIfDirty()) {
        event->ignore();
        return;
    }
    // Hide instead of destroying so the buffer survives reopening.
    event->ignore();
    hide();
}

LuaEditor* LuaEditorEnsure(QWidget* parent_for_first_create) {
    if (!g_editor)
        g_editor = new LuaEditor(parent_for_first_create);
    return g_editor;
}

LuaEditor* LuaEditorIfAny() { return g_editor; }

}  // namespace lua
}  // namespace vbam

#endif  // VBAM_ENABLE_LUA
