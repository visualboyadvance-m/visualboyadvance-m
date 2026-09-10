#ifndef VBAM_QT_WIDGETS_INPUT_DISPATCHER_H_
#define VBAM_QT_WIDGETS_INPUT_DISPATCHER_H_

#include <vector>

#include <QObject>

#include "optional.hpp"

#include "qt/config/user-input.h"

namespace widgets {

// A batch of user inputs that were pressed or released together. The order in
// the vector matters: this is the order in which the inputs were pressed or
// released, and consumers must process them in that order. A single batch can
// contain both presses and releases (e.g. a modifier combination changing).
struct UserInputBatch {
    struct Data {
        config::UserInput input;
        bool pressed;

        Data(config::UserInput input, bool pressed) : input(input), pressed(pressed) {}

        bool operator==(const Data& other) const {
            return input == other.input && pressed == other.pressed;
        }
        bool operator!=(const Data& other) const { return !(*this == other); }
    };

    std::vector<Data> data;

    // Returns the first released input, if any.
    nonstd::optional<config::UserInput> FirstReleasedInput() const {
        for (const auto& d : data) {
            if (!d.pressed)
                return d.input;
        }
        return nonstd::nullopt;
    }
};

// Central hub through which every user input (keyboard via
// KeyboardInputHandler, joystick via SdlPoller) is delivered. Replaces the wx
// port's VBAM_EVT_USER_INPUT event and EventHandlerProvider: instead of
// routing an event to "the current handler", consumers connect to the
// signal, and a consumer that wants exclusive input (a key-capture control in
// a dialog) pushes itself as the active receiver with SetExclusiveReceiver().
//
// Owned by VbamApp; a singleton for the process.
class InputDispatcher final : public QObject {
    Q_OBJECT

public:
    explicit InputDispatcher(QObject* parent = nullptr) : QObject(parent) {}
    ~InputDispatcher() override = default;

    InputDispatcher(const InputDispatcher&) = delete;
    InputDispatcher& operator=(const InputDispatcher&) = delete;

    // Emits `inputBatch` (or routes the batch to the exclusive receiver, if
    // one is set). Called by the input producers on the GUI thread.
    void Dispatch(const UserInputBatch& batch) {
        if (exclusive_receiver_) {
            Q_EMIT exclusiveInputBatch(batch);
            return;
        }
        Q_EMIT inputBatch(batch);
    }

    // While `receiver` is set, batches are delivered through
    // exclusiveInputBatch() only (the emulator and the shortcut handler do not
    // see them). Used by UserInputCtrl while it is capturing a binding. Pass
    // nullptr to clear. Only the current exclusive receiver may clear it.
    void SetExclusiveReceiver(QObject* receiver) { exclusive_receiver_ = receiver; }
    QObject* exclusive_receiver() const { return exclusive_receiver_; }

Q_SIGNALS:
    // Normal delivery. Connected by GameArea (game keys), MainWindow (shortcut
    // commands) and any viewer that reacts to keys.
    void inputBatch(const widgets::UserInputBatch& batch);
    // Exclusive delivery while a capture control owns the input.
    void exclusiveInputBatch(const widgets::UserInputBatch& batch);

private:
    QObject* exclusive_receiver_ = nullptr;
};

}  // namespace widgets

Q_DECLARE_METATYPE(widgets::UserInputBatch)

#endif  // VBAM_QT_WIDGETS_INPUT_DISPATCHER_H_
