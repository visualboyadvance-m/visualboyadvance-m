#ifndef WX_WIDGETS_USER_INPUT_EVENT_H_
#define WX_WIDGETS_USER_INPUT_EVENT_H_

#include <unordered_set>
#include <vector>

#include <optional.hpp>

#include <wx/clntdata.h>
#include <wx/event.h>

#include "wx/config/user-input.h"

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

    // Mark `event_data` as processed and returns the new event filter. This is
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
// released. To use this object, attach it to a wxWindow and listen for
// the VBAM_EVT_USER_INPUT_DOWN and VBAM_EVT_USER_INPUT_UP events.
class UserInputEventSender final : public wxClientData {
public:
    // `window` must not be nullptr. Will assert otherwise.
    // `window` must outlive this object.
    explicit UserInputEventSender(wxWindow* const window);
    ~UserInputEventSender() override;

    // Disable copy and copy assignment.
    UserInputEventSender(const UserInputEventSender&) = delete;
    UserInputEventSender& operator=(const UserInputEventSender&) = delete;

private:
    // Keyboard event handlers.
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);

    // Resets the internal state. Called on focus in/out.
    void Reset(wxFocusEvent& event);

    std::unordered_set<wxKeyCode> active_keys_;
    std::unordered_set<wxKeyModifier> active_mods_;
    std::unordered_set<config::KeyboardInput> active_mod_inputs_;

    // The wxWindow this object is attached to.
    // Must outlive this object.
    wxWindow* const window_;
};

}  // namespace widgets

// Fired when a set of user inputs are pressed or released.
wxDECLARE_EVENT(VBAM_EVT_USER_INPUT, widgets::UserInputEvent);

#endif  // WX_WIDGETS_USER_INPUT_EVENT_H_
