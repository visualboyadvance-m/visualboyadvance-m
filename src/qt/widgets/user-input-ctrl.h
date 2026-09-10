#ifndef VBAM_QT_WIDGETS_USER_INPUT_CTRL_H_
#define VBAM_QT_WIDGETS_USER_INPUT_CTRL_H_

#include <unordered_set>

#include <QElapsedTimer>
#include <QLineEdit>

#include "qt/config/user-input.h"
#include "qt/widgets/input-dispatcher.h"

namespace widgets {

// A line edit used for input configuration. It can be configured for single
// or multi-key input. In multi-key mode, the user can press multiple keys to
// configure a single input.
//
// While it has focus the control is the InputDispatcher's exclusive receiver:
// every keyboard or joystick input is captured (on release, like the wx port)
// instead of reaching the emulator or the shortcut handler. Internally, this
// control stores a set of UserInput objects, which is how the value for the
// field should be modified.
class UserInputCtrl final : public QLineEdit {
    Q_OBJECT

public:
    explicit UserInputCtrl(QWidget* parent = nullptr);
    ~UserInputCtrl() override;

    // Sets multi-key mode on or off. Clears the inputs.
    void SetMultiKey(bool multikey);
    bool is_multikey() const { return is_multikey_; }

    // Sets this control inputs.
    void SetInputs(const std::unordered_set<config::UserInput>& inputs);

    // Returns the inputs set in this control.
    std::unordered_set<config::UserInput> inputs() const { return inputs_; }

    // Helper method to return the single input for non-multikey controls.
    // Asserts if `is_multikey_` is true. Returns an invalid UserInput if no
    // input is currently set.
    config::UserInput SingleInput() const;

    // Clears the inputs set in this control.
    void Clear();

    bool IsEmpty() const { return inputs_.empty(); }

    // Feeds a batch to the control as if it came from the dispatcher. Used by
    // the exclusive-receiver connection and by tests.
    void OnInputBatch(const widgets::UserInputBatch& batch);

Q_SIGNALS:
    // Emitted whenever the set of inputs changes (capture, SetInputs, Clear).
    void inputsChanged();

protected:
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    // Updates the text in the control to reflect the current inputs.
    void UpdateText();

    void AcquireExclusive();
    void ReleaseExclusive();

    bool is_multikey_ = false;
    bool exclusive_ = false;

    // Time since the control was focused. Used to ignore events sent very
    // shortly after activation (spurious joystick axis motion).
    QElapsedTimer focus_timer_;

    std::unordered_set<config::UserInput> inputs_;
};

}  // namespace widgets

#endif  // VBAM_QT_WIDGETS_USER_INPUT_CTRL_H_
