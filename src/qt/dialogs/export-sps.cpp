#include "qt/dialogs/export-sps.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

#include "core/base/check.h"

namespace dialogs {

// static
ExportSps* ExportSps::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new ExportSps(parent);
}

ExportSps::ExportSps(QWidget* parent) : BaseDialog(parent, "ExportSPS") {
    setWindowTitle(tr("Export Game Shark Snapshot"));

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    title_ = new QLineEdit(this);
    form->addRow(tr("Title:"), title_);
    description_ = new QLineEdit(this);
    form->addRow(tr("Description:"), description_);
    notes_ = new QPlainTextEdit(this);
    notes_->setMinimumSize(200, 100);
    form->addRow(tr("Notes:"), notes_);

    layout->addLayout(form, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

}  // namespace dialogs
