#include "wx/userinput.h"

#include "wx/joyedit.h"
#include "wx/sdljoy.h"
#include "wx/string.h"
#include "wxutil.h"
#include "../../wx/strutils.h"

// static
wxUserInput wxUserInput::Invalid() {
    return wxUserInput(Device::Invalid, 0, 0, 0);
}

// static
wxUserInput wxUserInput::FromKeyEvent(const wxKeyEvent& event) {
    return wxUserInput(Device::Keyboard,
                       event.GetModifiers(),
                       getKeyboardKeyCode(event),
                       0);
}

// static
wxUserInput wxUserInput::FromJoyEvent(const wxJoyEvent& event) {
    return wxUserInput(Device::Joystick,
                       wxJoyKeyTextCtrl::DigitalButton(event),
                       event.control_index(),
                       event.joystick().player_index());
}

// static
wxUserInput wxUserInput::FromLegacyKeyModJoy(int key, int mod, int joy) {
    return wxUserInput(joy == 0 ? Device::Keyboard : Device::Joystick,
                       mod,
                       key,
                       joy);
}

// static
std::set<wxUserInput> wxUserInput::FromString(const wxString& string) {
    std::set<wxUserInput> user_inputs;

    if (string.empty()) {
        return user_inputs;
    }

    for (const auto& token : str_split_with_sep(string, wxT(","))) {
        int mod, key, joy;
        if (!wxJoyKeyTextCtrl::ParseString(token, token.size(), mod, key, joy)) {
            user_inputs.clear();
            return user_inputs;
        }
        user_inputs.emplace(FromLegacyKeyModJoy(key, mod, joy));
    }
    return user_inputs;
}

// static
wxString wxUserInput::SpanToString(const std::set<wxUserInput>& user_inputs, bool is_config) {
    wxString config_string;
    if (user_inputs.empty()) {
        return config_string;
    }
    for (const wxUserInput& user_input : user_inputs) {
        config_string += user_input.ToString(is_config) + wxT(',');
    }
    return config_string.SubString(0, config_string.size() - 2);
}

wxString wxUserInput::ToString(bool is_config) const {
    // TODO: Move the implementation here once all callers have been updated.
    return wxJoyKeyTextCtrl::ToString(mod_, key_, joy_, is_config);
}

bool wxUserInput::operator==(const wxUserInput& other) const {
    return device_ == other.device_ && mod_ == other.mod_ &&
           key_ == other.key_ && joy_ == other.joy_;
}
bool wxUserInput::operator!=(const wxUserInput& other) const {
    return !(*this == other);
}
bool wxUserInput::operator<(const wxUserInput& other) const {
    if (device_ < other.device_) {
        return device_ < other.device_;
    }
    if (joy_ != other.joy_) {
        return joy_ < other.joy_;
    }
    if (key_ != other.key_) {
        return key_ < other.key_;
    }
    if (mod_ != other.mod_) {
        return mod_ < other.mod_;
    }
    return false;
}
bool wxUserInput::operator<=(const wxUserInput& other) const {
    return !(*this > other);
}
bool wxUserInput::operator>(const wxUserInput& other) const {
    return other < *this;
}
bool wxUserInput::operator>=(const wxUserInput& other) const {
    return !(*this < other);
}

// Actual underlying constructor.
wxUserInput::wxUserInput(Device device, int mod, uint8_t key, unsigned joy) :
    device_(device),
    joystick_(joy == 0 ? wxJoystick::Invalid() : wxJoystick::FromLegacyPlayerIndex(joy)),
    mod_(mod),
    key_(key),
    joy_(joy) {}
