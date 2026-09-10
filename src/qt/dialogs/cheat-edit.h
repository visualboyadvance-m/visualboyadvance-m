#ifndef VBAM_QT_DIALOGS_CHEAT_EDIT_H_
#define VBAM_QT_DIALOGS_CHEAT_EDIT_H_

#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>

#include "qt/dialogs/base-dialog.h"

namespace dialogs {

// Add / edit a cheat: description, code type and one or more codes (one per
// whitespace-separated token). The type list depends on the loaded system:
//   GB : 0 = Game Shark, 1 = Game Genie
//   GBA: 0 = Generic Code, 1 = Game Shark Advance, 2 = Code Breaker Advance,
//        3 = Flashcart CHT
// Interpretation of the codes is done by CheatList::AddCheat(), which mirrors
// the wx port (the chosen type is only a hint for ambiguous GBA codes).
class CheatEdit : public BaseDialog {
    Q_OBJECT

public:
    static CheatEdit* NewInstance(QWidget* parent);
    ~CheatEdit() override = default;

    // Repopulates the type list for the given system.
    void SetIsGb(bool isgb);

    void SetDescription(const QString& desc) { desc_->setText(desc); }
    void SetType(int type) { type_->setCurrentIndex(type); }
    void SetCodes(const QString& codes) { codes_->setPlainText(codes); }

    QString description() const { return desc_->text(); }
    int type() const { return type_->currentIndex(); }
    QString codes() const { return codes_->toPlainText(); }

private:
    explicit CheatEdit(QWidget* parent);

    QLineEdit* desc_;
    QComboBox* type_;
    QPlainTextEdit* codes_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_CHEAT_EDIT_H_
