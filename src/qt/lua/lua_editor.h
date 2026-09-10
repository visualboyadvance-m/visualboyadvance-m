// Built-in Lua editor for the Qt frontend.
//
// One non-modal dialog holds:
//   - a QPlainTextEdit with a small QSyntaxHighlighter for Lua
//     (keywords, the VBA-M API names, strings, comments, numbers);
//   - a row of New / Open / Save / Save As / Run / Stop buttons;
//   - a status label showing the current file path.
//
// "Run" hands the buffer to LuaEngine::LoadString. "Stop" calls
// LuaEngine::Stop. Output goes to the Lua console window.

#ifndef VBAM_QT_LUA_LUA_EDITOR_H_
#define VBAM_QT_LUA_LUA_EDITOR_H_

#if defined(VBAM_ENABLE_LUA)

#include <QRegularExpression>
#include <QString>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

#include "qt/dialogs/base-dialog.h"

class QLabel;
class QPlainTextEdit;
class QTextDocument;

namespace vbam {
namespace lua {

// Minimal Lua highlighter: keywords, API identifiers, numbers, strings
// (single/double quoted), line comments (--) and block comments (--[[ ]]).
class LuaHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit LuaHighlighter(QTextDocument* document);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<Rule> rules_;
    QRegularExpression block_comment_start_;
    QRegularExpression block_comment_end_;
    QTextCharFormat comment_format_;
};

class LuaEditor final : public dialogs::BaseDialog {
    Q_OBJECT

public:
    explicit LuaEditor(QWidget* parent);
    ~LuaEditor() override;

    // Replace the editor's contents and reset the dirty/path state.
    void OpenFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void DoNew();
    void DoOpen();
    void DoSave();
    void DoSaveAs();
    void DoRun();
    void DoStop();
    void OnTextModified();

private:
    QString GetText() const;
    void    SetText(const QString& s);
    bool    ConfirmDiscardIfDirty();
    bool    SaveTo(const QString& path);
    void    UpdateTitle();

    QPlainTextEdit* edit_ = nullptr;
    LuaHighlighter* highlighter_ = nullptr;
    QLabel* status_ = nullptr;
    QString current_path_;
    bool    dirty_ = false;
};

LuaEditor* LuaEditorEnsure(QWidget* parent_for_first_create);
LuaEditor* LuaEditorIfAny();

}  // namespace lua
}  // namespace vbam

#endif  // VBAM_ENABLE_LUA

#endif  // VBAM_QT_LUA_LUA_EDITOR_H_
