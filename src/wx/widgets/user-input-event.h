#ifndef VBAM_WX_WIDGETS_USER_INPUT_EVENT_H_
#define VBAM_WX_WIDGETS_USER_INPUT_EVENT_H_

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
// Note that a single event can contain multiple inputs pressed and/or released.
// These should be processed in the same order as they are in the vector.
class UserInputEvent final : public wxEvent {
public:
    // Data for the event. Contains the user input and whether it was pressed or
    // released.
    struct Data {
        const config::UserInput input;
        const bool pressed;

        Data(config::UserInput input, bool pressed) : input(input), pressed(pressed) {};

        // Equality operators.
        bool operator==(const Data& other) const {
            return input == other.input && pressed == other.pressed;
        }
        bool operator!=(const Data& other) const { return !(*this == other); }
    };

    UserInputEvent(std::vector<Data> event_data);
    virtual ~UserInputEvent() override = default;

    // Disable copy and copy assignment.
    UserInputEvent(const UserInputEvent&) = delete;
    UserInputEvent& operator=(const UserInputEvent&) = delete;

    // Returns the first released input, if any.
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

}  // namespace widgets

// Fired when a set of user inputs are pressed or released.
wxDECLARE_EVENT(VBAM_EVT_USER_INPUT, ::widgets::UserInputEvent);

#endif  // VBAM_WX_WIDGETS_USER_INPUT_EVENT_H_
