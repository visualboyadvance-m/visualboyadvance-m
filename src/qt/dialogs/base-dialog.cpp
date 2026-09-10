#include "qt/dialogs/base-dialog.h"

#include <QDialogButtonBox>
#include <QHideEvent>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QWindow>

#include "core/base/check.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"

namespace dialogs {

namespace {

constexpr int kGeometryVersion = 1;

QString GeometryKey(const QString& name, const char* suffix) {
    return QStringLiteral("Geometry/") + name + QLatin1String(suffix);
}

}  // namespace

BaseDialog::BaseDialog(QWidget* parent, const QString& name)
    : QDialog(parent), name_(name) {
    VBAM_CHECK(parent);
    setObjectName(name);
    setModal(true);
    setSizeGripEnabled(true);
}

BaseDialog::~BaseDialog() = default;

QDialogButtonBox* BaseDialog::CreateOkCancel() {
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &BaseDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &BaseDialog::reject);
    return box;
}

void BaseDialog::showEvent(QShowEvent* event) {
    // Reload the widgets from the options: the dialog instance is cached and
    // reused, and the options may have changed since it was last shown.
    bindings_.Load();

    if (!dialog_shown_) {
        dialog_shown_ = true;
        RepositionDialog();
    }

    ApplyKeepOnTop();
    keep_on_top_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kDispKeepOnTop, [this](config::Option*) { ApplyKeepOnTop(); });

    OnDialogShown();
    QDialog::showEvent(event);
}

void BaseDialog::hideEvent(QHideEvent* event) {
    SaveGeometry();
    keep_on_top_observer_.reset();
    OnDialogHidden();
    QDialog::hideEvent(event);
}

void BaseDialog::accept() {
    if (!bindings_.Save()) {
        // An invalid value was reported by the binding; keep the dialog open.
        return;
    }
    if (!OnAccept()) {
        return;
    }
    QDialog::accept();
}

void BaseDialog::ApplyKeepOnTop() {
    const bool on_top = OPTION(kDispKeepOnTop);
    if (bool(windowFlags() & Qt::WindowStaysOnTopHint) == on_top) {
        return;
    }
    const bool was_visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, on_top);
    // Changing the flags hides the window on most platforms.
    if (was_visible) {
        show();
    }
}

void BaseDialog::RepositionDialog() {
    QSettings* cfg = vbamApp().config();

    // Restore a saved geometry, if it was saved at the current DPI.
    if (cfg->value(GeometryKey(name_, "Version"), 0).toInt() == kGeometryVersion &&
        cfg->contains(GeometryKey(name_, "X"))) {
        const qreal saved_dpr = cfg->value(GeometryKey(name_, "DPR"), 1.0).toReal();
        const qreal current_dpr = windowHandle() ? windowHandle()->devicePixelRatio()
                                                 : devicePixelRatioF();
        if (qFuzzyCompare(saved_dpr, current_dpr)) {
            QRect rect(cfg->value(GeometryKey(name_, "X")).toInt(),
                       cfg->value(GeometryKey(name_, "Y")).toInt(),
                       cfg->value(GeometryKey(name_, "W")).toInt(),
                       cfg->value(GeometryKey(name_, "H")).toInt());
            // Make sure we are not drawing out of bounds.
            QScreen* screen = QGuiApplication::screenAt(rect.center());
            if (!screen && parentWidget()) {
                screen = parentWidget()->screen();
            }
            if (screen) {
                const QRect avail = screen->availableGeometry();
                if (rect.width() > avail.width()) rect.setWidth(avail.width());
                if (rect.height() > avail.height()) rect.setHeight(avail.height());
                if (rect.right() > avail.right()) rect.moveRight(avail.right());
                if (rect.bottom() > avail.bottom()) rect.moveBottom(avail.bottom());
                if (rect.left() < avail.left()) rect.moveLeft(avail.left());
                if (rect.top() < avail.top()) rect.moveTop(avail.top());
                if (rect.width() >= minimumSizeHint().width() / 2 &&
                    rect.height() >= minimumSizeHint().height() / 2) {
                    setGeometry(rect);
                    geometry_restored_ = true;
                    return;
                }
            }
        }
    }

    // No usable saved geometry: place the dialog at the bottom-right of the
    // parent, clamped to the screen, like the wx port.
    QWidget* parent = parentWidget();
    if (!parent) {
        return;
    }
    adjustSize();
    const QRect parent_rect = parent->frameGeometry();
    QPoint pos(parent_rect.right() - width(), parent_rect.bottom() - height());
    if (QScreen* screen = parent->screen()) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + width() > avail.right()) pos.setX(avail.right() - width());
        if (pos.y() + height() > avail.bottom()) pos.setY(avail.bottom() - height());
        if (pos.x() < avail.left()) pos.setX(avail.left());
        if (pos.y() < avail.top()) pos.setY(avail.top());
    }
    move(pos);
}

void BaseDialog::SaveGeometry() {
    if (!dialog_shown_) {
        return;
    }
    QSettings* cfg = vbamApp().config();
    const QRect rect = geometry();
    cfg->setValue(GeometryKey(name_, "Version"), kGeometryVersion);
    cfg->setValue(GeometryKey(name_, "X"), rect.x());
    cfg->setValue(GeometryKey(name_, "Y"), rect.y());
    cfg->setValue(GeometryKey(name_, "W"), rect.width());
    cfg->setValue(GeometryKey(name_, "H"), rect.height());
    cfg->setValue(GeometryKey(name_, "DPR"),
                  windowHandle() ? windowHandle()->devicePixelRatio() : devicePixelRatioF());
}

}  // namespace dialogs
