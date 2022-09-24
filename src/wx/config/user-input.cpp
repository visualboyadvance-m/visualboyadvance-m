#include "config/user-input.h"

#include "strutils.h"
#include "wx/joyedit.h"

namespace config {

UserInput::UserInput(const wxKeyEvent& event)
    : UserInput(Device::Keyboard,
                event.GetModifiers(),
                getKeyboardKeyCode(event),
                0) {}

UserInput::UserInput(const wxJoyEvent& event)
    : UserInput(Device::Joystick,
                event.control(),
                event.control_index(),
                event.joystick().player_index()) {}

// static
std::set<UserInput> UserInput::FromString(const wxString& string) {
    std::set<UserInput> user_inputs;

    if (string.empty()) {
        return user_inputs;
    }

    for (const auto& token : strutils::split_with_sep(string, wxT(","))) {
        int mod, key, joy;
        if (!wxJoyKeyTextCtrl::ParseString(token, token.size(), mod, key,
                                           joy)) {
            user_inputs.clear();
            return user_inputs;
        }
        user_inputs.emplace(UserInput(key, mod, joy));
    }
    return user_inputs;
}

// static
wxString UserInput::SpanToString(const std::set<UserInput>& user_inputs,
                                 bool is_config) {
    wxString config_string;
    if (user_inputs.empty()) {
        return config_string;
    }
    for (const UserInput& user_input : user_inputs) {
        config_string += user_input.ToString(is_config) + wxT(',');
    }
    return config_string.SubString(0, config_string.size() - 2);
}

wxString UserInput::ToString(bool is_config) const {
    // TODO: Move the implementation here once all callers have been updated.
    return wxJoyKeyTextCtrl::ToString(mod_, key_, joy_, is_config);
}

}  // namespace config
