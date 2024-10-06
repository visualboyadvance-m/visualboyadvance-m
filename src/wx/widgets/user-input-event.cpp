#include "wx/widgets/user-input-event.h"

#include <utility>
#include <vector>

#include <wx/event.h>
#include <wx/eventfilter.h>

#include "wx/config/user-input.h"

namespace widgets {

UserInputEvent::UserInputEvent(std::vector<Data> event_data)
    : wxEvent(0, VBAM_EVT_USER_INPUT), data_(std::move(event_data)) {}

nonstd::optional<config::UserInput> UserInputEvent::FirstReleasedInput() const {
    const auto iter =
        std::find_if(data_.begin(), data_.end(), [](const auto& data) { return !data.pressed; });

    if (iter == data_.end()) {
        // No released inputs.
        return nonstd::nullopt;
    }

    return iter->input;
}

int UserInputEvent::FilterProcessedInput(const config::UserInput& user_input) {
    // Keep all data not using `user_input`.
    std::vector<Data> new_data;
    for (const auto& data : data_) {
        if (data.input != user_input) {
            new_data.push_back(data);
        }
    }

    // Update the internal data.
    data_ = std::move(new_data);

    if (data_.empty()) {
        // All data was removed, the event was fully processed.
        return wxEventFilter::Event_Processed;
    } else {
        // Some data remains, let the event propagate.
        return wxEventFilter::Event_Skip;
    }
}

wxEvent* UserInputEvent::Clone() const {
    return new UserInputEvent(this->data_);
}

}  // namespace widgets

wxDEFINE_EVENT(VBAM_EVT_USER_INPUT, ::widgets::UserInputEvent);
