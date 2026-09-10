#ifndef VBAM_QT_DIALOGS_EXPORT_SPS_H_
#define VBAM_QT_DIALOGS_EXPORT_SPS_H_

#include <QLineEdit>
#include <QPlainTextEdit>

#include "qt/dialogs/base-dialog.h"

namespace dialogs {

// Title / description / notes entry for a Game Shark snapshot export.
class ExportSps : public BaseDialog {
    Q_OBJECT

public:
    static ExportSps* NewInstance(QWidget* parent);
    ~ExportSps() override = default;

    void SetTitle(const QString& title) { title_->setText(title); }
    void SetDescription(const QString& desc) { description_->setText(desc); }
    void SetNotes(const QString& notes) { notes_->setPlainText(notes); }

    QString title() const { return title_->text(); }
    QString description() const { return description_->text(); }
    QString notes() const { return notes_->toPlainText(); }

private:
    explicit ExportSps(QWidget* parent);

    QLineEdit* title_;
    QLineEdit* description_;
    QPlainTextEdit* notes_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_EXPORT_SPS_H_
