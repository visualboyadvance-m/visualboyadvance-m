#include "qt/dialogs/cheat-add.h"

#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
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
CheatAdd* CheatAdd::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new CheatAdd(parent);
}

CheatAdd::CheatAdd(QWidget* parent) : BaseDialog(parent, "CheatAdd") {
    setWindowTitle(tr("Add Cheat"));

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(BoldLabel(tr("&Description"), this));
    desc_ = new QLineEdit(this);
    desc_->setMaxLength(sizeof(cheatsList[0].desc) - 1);
    layout->addWidget(desc_);

    layout->addWidget(BoldLabel(tr("Address"), this));
    address_ = new QLabel(this);
    layout->addWidget(address_);

    layout->addWidget(BoldLabel(tr("&Value"), this));
    value_ = new QLineEdit(this);
    layout->addWidget(value_);

    layout->addWidget(BoldLabel(tr("Format"), this));
    format_ = new QLabel(this);
    layout->addWidget(format_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void CheatAdd::SetValueCharset(const QString& allowed) {
    const QString pattern =
        QStringLiteral("[%1]*").arg(QRegularExpression::escape(allowed));
    value_->setValidator(new QRegularExpressionValidator(QRegularExpression(pattern), value_));
}

}  // namespace dialogs
