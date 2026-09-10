#include "qt/dialogs/cheat-edit.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "core/gba/gbaCheats.h"

namespace dialogs {

namespace {

QLabel* BoldLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    QFont f = label->font();
    f.setBold(true);
    label->setFont(f);
    return label;
}

}  // namespace

// static
CheatEdit* CheatEdit::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new CheatEdit(parent);
}

CheatEdit::CheatEdit(QWidget* parent) : BaseDialog(parent, "CheatEdit") {
    setWindowTitle(tr("Edit Cheat"));

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(BoldLabel(tr("&Description"), this));
    desc_ = new QLineEdit(this);
    desc_->setMaxLength(sizeof(cheatsList[0].desc) - 1);
    layout->addWidget(desc_);

    layout->addWidget(BoldLabel(tr("&Type"), this));
    type_ = new QComboBox(this);
    layout->addWidget(type_);

    layout->addWidget(BoldLabel(tr("C&odes"), this));
    codes_ = new QPlainTextEdit(this);
    codes_->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono = codes_->font();
    mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    codes_->setFont(mono);
    layout->addWidget(codes_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    SetIsGb(false);
}

void CheatEdit::SetIsGb(bool isgb) {
    type_->clear();
    if (isgb) {
        // DO NOT TRANSLATE
        type_->addItem("Game Shark");
        type_->addItem("Game Genie");
    } else {
        type_->addItem(tr("Generic Code"));
        // DO NOT TRANSLATE
        type_->addItem("Game Shark Advance");
        type_->addItem("Code Breaker Advance");
        type_->addItem("Flashcart CHT");
    }
    type_->setCurrentIndex(0);
}

}  // namespace dialogs
