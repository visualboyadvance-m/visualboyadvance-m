#include "wx/userinput.h"

#include "wx/string.h"
#include "wxutil.h"

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
wxUserInput wxUserInput::FromLegacyJoyKeyBinding(const wxJoyKeyBinding& binding) {
    return wxUserInput(binding.joy == 0 ? Device::Keyboard : Device::Joystick,
                       binding.mod,
                       binding.key,
                       binding.joy);
}

// static
wxUserInput wxUserInput::FromString(const wxString& string) {
    // TODO: Move the implementation here once all callers have been updated.
    int mod = 0;
    int key = 0;
    int joy = 0;
    if (!wxJoyKeyTextCtrl::FromString(string, mod, key, joy)) {
        return wxUserInput::Invalid();
    }
    return wxUserInput::FromLegacyJoyKeyBinding({key, mod, joy});
}

wxString wxUserInput::ToString() {
    if (!config_string_.IsNull()) {
        return config_string_;
    }

    // TODO: Move the implementation here once all callers have been updated.
    config_string_ = wxJoyKeyTextCtrl::ToString(mod_, key_, joy_);
    return config_string_;
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
    mod_(mod),
    key_(key),
    joy_(joy) {}
