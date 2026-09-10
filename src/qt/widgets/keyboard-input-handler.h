#ifndef VBAM_QT_WIDGETS_KEYBOARD_INPUT_HANDLER_H_
#define VBAM_QT_WIDGETS_KEYBOARD_INPUT_HANDLER_H_

#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "qt/config/user-input.h"

class QKeyEvent;

namespace widgets {

class InputDispatcher;

// Converts QKeyEvents into UserInputBatch deliveries on the InputDispatcher.
// This class should be kept as a singleton owned by the application object and
// fed from VbamApp::notify() so keyboard input is seen globally, whichever
// widget has focus.
//
// Handles the key/modifier bookkeeping of the wx port: a modifier press is
// itself an input ("LCTRL"), a key pressed with modifiers produces the combo
// input ("CTRL+A") and releases are matched to the inputs that were pressed so
// no key ever stays stuck.
class KeyboardInputHandler final {
public:
    // Synchronous sink for input events. Called for each (input, pressed)
    // entry produced by a key event, BEFORE the batch is dispatched. Intended
    // to update joypad state synchronously (see EmulatedGamepad) so key
    // events that arrive while the emulator main loop is running still
    // update joypad state in time for the next frame.
    using InputSink = std::function<void(const config::UserInput& input, bool pressed)>;

    KeyboardInputHandler(InputDispatcher* dispatcher, InputSink sync_sink);
    ~KeyboardInputHandler();

    KeyboardInputHandler(const KeyboardInputHandler&) = delete;
    KeyboardInputHandler& operator=(const KeyboardInputHandler&) = delete;

    // Processes the provided key event. Returns true if the event produced at
    // least one input (the caller may then stop further propagation of the
    // QKeyEvent when a game key or shortcut consumed it).
    bool ProcessKeyEvent(QKeyEvent* event);

    // Resets the state. Called when the main window loses focus to prevent
    // stuck keys; every tracked input is released through the sink first.
    void Reset();

    // Converts Qt modifiers plus the current physical L/R modifier state (from
    // SDL when available) to KeyModFlag bits.
    static uint32_t CurrentModFlags(const QKeyEvent* event);

    // Maps a Qt key code to the modifier flag it represents, or kKeyModNone.
    static uint32_t ModFlagForKey(int qt_key, uint32_t extended_hint);

private:
    void OnKeyDown(QKeyEvent* event);
    void OnKeyUp(QKeyEvent* event);
    void ReleaseAllTracked();

    std::unordered_set<int> active_keys_;
    std::unordered_set<uint32_t> active_mods_;
    std::unordered_set<config::KeyboardInput> active_mod_inputs_;
    // Extended modifiers (with L/R distinction) captured for each key press.
    std::unordered_map<int, uint32_t> key_extended_mods_;

    InputDispatcher* const dispatcher_;
    const InputSink sync_sink_;

    // Set by OnKeyDown/OnKeyUp when the event produced at least one input.
    bool last_event_produced_inputs_ = false;
};

}  // namespace widgets

#endif  // VBAM_QT_WIDGETS_KEYBOARD_INPUT_HANDLER_H_
