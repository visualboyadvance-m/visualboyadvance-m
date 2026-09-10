#ifndef VBAM_QT_VIEWERS_LOG_DIALOG_H_
#define VBAM_QT_VIEWERS_LOG_DIALOG_H_

#include <QString>

#include "qt/dialogs/base-dialog.h"

class QPlainTextEdit;

// The "Logging" dialog: verbose-logging flags (systemVerbose bits) and the
// application log text (vbam::LogText()) with Save and Clear buttons. One
// instance is kept for the whole session by MainWindow (see GetLogDialog());
// Update() is pinged whenever new log text arrives.
class LogDialog final : public dialogs::BaseDialog {
    Q_OBJECT

public:
    explicit LogDialog(QWidget* parent);
    ~LogDialog() override;

    // Appends the log text added since the last call (or resyncs from scratch
    // if the buffer was cleared or truncated).
    void Update();

protected:
    void OnDialogShown() override;

private:
    void Save();
    void Clear();

    QPlainTextEdit* log_ = nullptr;
    int shown_len_ = 0;
};

#endif  // VBAM_QT_VIEWERS_LOG_DIALOG_H_
