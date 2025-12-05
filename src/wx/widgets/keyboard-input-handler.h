#ifndef VBAM_WX_WIDGETS_KEYBOARD_INPUT_HANDLER_H_
#define VBAM_WX_WIDGETS_KEYBOARD_INPUT_HANDLER_H_

#include <unordered_map>
#include <unordered_set>

#include <wx/event.h>

#include "wx/config/user-input.h"
#include "wx/widgets/event-handler-provider.h"

namespace widgets {

// Object that is used to fire user input events when a keyboard key is pressed
// or released. This class should be kept as a singleton owned by the
// application object. It is meant to be used in the FilterEvent() method of the
// app to create user input events globally whenever the keyboard is used.
class KeyboardInputHandler final {
public:
    explicit KeyboardInputHandler(EventHandlerProvider* const handler_provider);
    ~KeyboardInputHandler();

    // Disable copy and copy assignment.
    KeyboardInputHandler(const KeyboardInputHandler&) = delete;
    KeyboardInputHandler& operator=(const KeyboardInputHandler&) = delete;

    // Processes the provided key event and sends the appropriate user input
    // event to the current event handler.
    void ProcessKeyEvent(wxKeyEvent& event);

    // Resets the state of the sender. This should be called when the main frame
    // loses focus to prevent stuck keys.
    void Reset();

private:
    // Keyboard event handlers.
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);

    std::unordered_set<wxKeyCode> active_keys_;
    std::unordered_set<wxKeyModifier> active_mods_;
    std::unordered_set<config::KeyboardInput> active_mod_inputs_;
    // Track extended modifiers (with L/R distinction) for each key press
    std::unordered_map<wxKeyCode, uint32_t> key_extended_mods_;

    // The provider of event handlers to send the events to.
    EventHandlerProvider* const handler_provider_;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_KEYBOARD_INPUT_HANDLER_H_
