#ifndef WX_WIDGETS_USER_INPUT_EVENT_H_
#define WX_WIDGETS_USER_INPUT_EVENT_H_

#include <unordered_set>
#include <vector>

#include <optional.hpp>

#include <wx/clntdata.h>
#include <wx/event.h>

#include "wx/config/user-input.h"
#include "wx/widgets/event-handler-provider.h"

namespace widgets {

// Event fired when a set of user input are pressed or released. The event
// contains the set of user input that were pressed or released. The order
// in the vector matters, this is the order in which the inputs were pressed or
// released.
class UserInputEvent final : public wxEvent {
public:
    // Data for the event. Contains the user input and whether it was pressed or
    // released.
    struct Data {
        const config::UserInput input;
        const bool pressed;

        Data(config::UserInput input, bool pressed) : input(input), pressed(pressed){};
    };

    UserInputEvent(std::vector<Data> event_data);
    virtual ~UserInputEvent() override = default;

    // Disable copy and copy assignment.
    UserInputEvent(const UserInputEvent&) = delete;
    UserInputEvent& operator=(const UserInputEvent&) = delete;

    // Returns the first pressed input, if any.
    nonstd::optional<config::UserInput> FirstReleasedInput() const;

    // Marks `user_input` as processed and returns the new event filter. This is
    // meant to be used with FilterEvent() to process global shortcuts before
    // sending the event to the next handler.
    int FilterProcessedInput(const config::UserInput& user_input);

    // wxEvent implementation.
    wxEvent* Clone() const override;

    const std::vector<Data>& data() const { return data_; }

private:
    std::vector<Data> data_;
};

// Object that is used to fire user input events when a key is pressed or
// released. This class should be kept as a singleton owned by the application
// object. It is meant to be used in the FilterEvent() method of the app.
class KeyboardInputSender final : public wxClientData {
public:
    explicit KeyboardInputSender(EventHandlerProvider* const handler_provider);
    ~KeyboardInputSender() override;

    // Disable copy and copy assignment.
    KeyboardInputSender(const KeyboardInputSender&) = delete;
    KeyboardInputSender& operator=(const KeyboardInputSender&) = delete;

    // Processes the provided key event and sends the appropriate user input
    // event to the current event handler.
    void ProcessKeyEvent(wxKeyEvent& event);

private:
    // Keyboard event handlers.
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);

    std::unordered_set<wxKeyCode> active_keys_;
    std::unordered_set<wxKeyModifier> active_mods_;
    std::unordered_set<config::KeyboardInput> active_mod_inputs_;

    // The provider of event handlers to send the events to.
    EventHandlerProvider* const handler_provider_;
};

}  // namespace widgets

// Fired when a set of user inputs are pressed or released.
wxDECLARE_EVENT(VBAM_EVT_USER_INPUT, widgets::UserInputEvent);

#endif  // WX_WIDGETS_USER_INPUT_EVENT_H_
