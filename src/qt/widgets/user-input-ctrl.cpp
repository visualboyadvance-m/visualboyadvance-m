#include "qt/widgets/user-input-ctrl.h"

#include <QCoreApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QStringList>

#include "core/base/check.h"
#include "qt/app.h"

namespace widgets {

namespace {

InputDispatcher* Dispatcher() {
    if (!QCoreApplication::instance()) {
        return nullptr;
    }
    VbamApp* app = dynamic_cast<VbamApp*>(QCoreApplication::instance());
    return app ? app->input_dispatcher() : nullptr;
}

}  // namespace

UserInputCtrl::UserInputCtrl(QWidget* parent) : QLineEdit(parent) {
    setReadOnly(true);
    setFocusPolicy(Qt::StrongFocus);
    setPlaceholderText(tr("Press a key or button..."));
}

UserInputCtrl::~UserInputCtrl() {
    ReleaseExclusive();
}

void UserInputCtrl::SetMultiKey(bool multikey) {
    is_multikey_ = multikey;
    Clear();
}

void UserInputCtrl::SetInputs(const std::unordered_set<config::UserInput>& inputs) {
    inputs_.clear();
    inputs_.insert(inputs.begin(), inputs.end());
    UpdateText();
    Q_EMIT inputsChanged();
}

config::UserInput UserInputCtrl::SingleInput() const {
    VBAM_CHECK(!is_multikey_);
    if (inputs_.empty()) {
        return config::UserInput();
    }
    return *inputs_.begin();
}

void UserInputCtrl::Clear() {
    inputs_.clear();
    UpdateText();
    Q_EMIT inputsChanged();
}

void UserInputCtrl::OnInputBatch(const widgets::UserInputBatch& batch) {
    // Find the first released input.
    nonstd::optional<config::UserInput> input = batch.FirstReleasedInput();

    if (input == nonstd::nullopt) {
        // No released inputs.
        return;
    }

    static const qint64 kInterval = 100;
    if (focus_timer_.isValid() && focus_timer_.elapsed() < kInterval) {
        // Ignore events sent very shortly after focus. This is used to ignore
        // some spurious joystick events like an accidental axis motion.
        return;
    }

    if (!is_multikey_) {
        inputs_.clear();
    }

    inputs_.insert(std::move(input.value()));
    UpdateText();
    Q_EMIT inputsChanged();

    // Like the wx port, move on to the next control once an input was
    // captured.
    focusNextChild();
}

void UserInputCtrl::UpdateText() {
    QStringList parts;
    for (const auto& input : inputs_) {
        parts << input.ToLocalizedString();
    }
    setText(parts.join(QStringLiteral(", ")));
}

void UserInputCtrl::AcquireExclusive() {
    InputDispatcher* dispatcher = Dispatcher();
    if (!dispatcher || exclusive_) {
        return;
    }
    exclusive_ = true;
    dispatcher->SetExclusiveReceiver(this);
    connect(dispatcher, &InputDispatcher::exclusiveInputBatch, this,
            &UserInputCtrl::OnInputBatch);
}

void UserInputCtrl::ReleaseExclusive() {
    if (!exclusive_) {
        return;
    }
    exclusive_ = false;
    InputDispatcher* dispatcher = Dispatcher();
    if (!dispatcher) {
        return;
    }
    disconnect(dispatcher, &InputDispatcher::exclusiveInputBatch, this,
               &UserInputCtrl::OnInputBatch);
    if (dispatcher->exclusive_receiver() == this) {
        dispatcher->SetExclusiveReceiver(nullptr);
    }
}

void UserInputCtrl::focusInEvent(QFocusEvent* event) {
    focus_timer_.start();
    AcquireExclusive();
    QLineEdit::focusInEvent(event);
}

void UserInputCtrl::focusOutEvent(QFocusEvent* event) {
    ReleaseExclusive();
    QLineEdit::focusOutEvent(event);
}

void UserInputCtrl::keyPressEvent(QKeyEvent* event) {
    // Keyboard input reaches this control through the InputDispatcher; the
    // line edit itself must not react to keys (no text editing, no Tab
    // navigation while capturing).
    event->accept();
}

void UserInputCtrl::keyReleaseEvent(QKeyEvent* event) {
    event->accept();
}

void UserInputCtrl::contextMenuEvent(QContextMenuEvent* event) {
    // No copy/paste menu on a binding field.
    event->accept();
}

}  // namespace widgets
