#ifndef VBAM_QT_DIALOGS_CHEAT_ADD_H_
#define VBAM_QT_DIALOGS_CHEAT_ADD_H_

#include <QLabel>
#include <QLineEdit>

#include "qt/dialogs/base-dialog.h"

namespace dialogs {

// Adds a cheat search result as a cheat: description and value entry, with
// the (read-only) address and value format of the selected result.
class CheatAdd : public BaseDialog {
    Q_OBJECT

public:
    static CheatAdd* NewInstance(QWidget* parent);
    ~CheatAdd() override = default;

    void SetDescription(const QString& desc) { desc_->setText(desc); }
    void SetAddress(const QString& addr) { address_->setText(addr); }
    void SetValue(const QString& value) { value_->setText(value); }
    void SetFormat(const QString& fmt) { format_->setText(fmt); }
    // Restricts the value entry to the given characters (like the wx
    // wxFILTER_INCLUDE_CHAR_LIST validator).
    void SetValueCharset(const QString& allowed);

    QString description() const { return desc_->text(); }
    QString value() const { return value_->text(); }

private:
    explicit CheatAdd(QWidget* parent);

    QLineEdit* desc_;
    QLabel* address_;
    QLineEdit* value_;
    QLabel* format_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_CHEAT_ADD_H_
