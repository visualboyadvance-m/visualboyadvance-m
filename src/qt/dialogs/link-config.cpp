#include "qt/dialogs/link-config.h"

#ifndef NO_LINK

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QShowEvent>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "qt/opts.h"

namespace dialogs {

// static
LinkConfig* LinkConfig::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new LinkConfig(parent);
}

LinkConfig::LinkConfig(QWidget* parent) : BaseDialog(parent, "LinkConfig") {
    setWindowTitle(tr("Link configuration"));

    auto* layout = new QVBoxLayout(this);
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Link timeout (in milliseconds)"), this));
    timeout_ = new QSpinBox(this);
    timeout_->setRange(0, 9999999);
    row->addWidget(timeout_, 1);
    layout->addLayout(row);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &LinkConfig::OnAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void LinkConfig::showEvent(QShowEvent* event) {
    timeout_->setValue(gopts.link_timeout);
    BaseDialog::showEvent(event);
}

void LinkConfig::OnAccepted() {
    gopts.link_timeout = timeout_->value();
    accept();
}

}  // namespace dialogs

#endif  // NO_LINK
