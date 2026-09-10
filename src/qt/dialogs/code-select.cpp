#include "qt/dialogs/code-select.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "core/base/check.h"

namespace dialogs {

// static
CodeSelect* CodeSelect::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new CodeSelect(parent);
}

CodeSelect::CodeSelect(QWidget* parent) : BaseDialog(parent, "CodeSelect") {
    setWindowTitle(tr("Select Game"));

    auto* layout = new QVBoxLayout(this);
    list_ = new QListWidget(this);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(list_, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    layout->addWidget(buttons);
}

void CodeSelect::SetCodes(const QStringList& codes) {
    list_->clear();
    int index = 0;
    for (const QString& code : codes) {
        auto* item = new QListWidgetItem(code, list_);
        item->setData(Qt::UserRole, index++);
    }
    // The wx dialog sorted the list (wxLB_SORT); keep the original index in
    // the item data so the caller can still map the choice back.
    list_->sortItems();
    if (list_->count() > 0)
        list_->setCurrentRow(0);
}

int CodeSelect::selection() const {
    QListWidgetItem* item = list_->currentItem();
    if (!item)
        return -1;
    return item->data(Qt::UserRole).toInt();
}

}  // namespace dialogs
