#ifndef VBAM_WX_CONFIG_USER_INPUT_H_
#define VBAM_WX_CONFIG_USER_INPUT_H_

#include <wx/event.h>
#include <wx/string.h>
#include <set>

#include "widgets/wx/sdljoy.h"

namespace config {

// Abstraction for a user input, which can come from a keyboard or a joystick.
// This class implements comparison operators so it can be used in sets and as
// a key in maps.
//
// TODO: Right now, this class is implemented as a thin wrapper around the key,
// mod and joy user input representation used in many places in the code base.
// This is to ease a transition away from the key, mod, joy triplet, which
// UserInput will eventually replace.
class UserInput {
public:
    // The device type for a user control.
    enum class Device { Invalid = 0, Keyboard, Joystick, Last = Joystick };

    // Constructor from a configuration string. Returns empty set on failure.
    static std::set<UserInput> FromConfigString(const wxString& string);

    // Converts a set of UserInput into a configuration string. This
    // recomputes the configuration string every time and should not be used
    // for comparison purposes.
    // TODO: Replace std::set with std::span when the code base uses C++20.
    static wxString SpanToConfigString(const std::set<UserInput>& user_inputs);

    // Invalid UserInput, mainly used for comparison.
    UserInput() : UserInput(Device::Invalid, 0, 0, 0) {}

    // Constructor from a wxKeyEvent.
    UserInput(const wxKeyEvent& event);

    // Constructor from a wxJoyEvent.
    UserInput(const wxJoyEvent& event);

    // TODO: Remove this once all uses have been removed.
    UserInput(int key, int mod = 0, int joy = 0)
        : UserInput(joy == 0 ? Device::Keyboard : Device::Joystick,
                    mod,
                    key,
                    joy) {}

    Device device() const { return device_; }

    // Converts to a configuration string for saving.
    wxString ToConfigString() const;

    // Converts to a localized string for display.
    wxString ToLocalizedString() const;

    wxJoystick joystick() const { return joystick_; }
    constexpr bool is_valid() const { return device_ != Device::Invalid; }
    constexpr operator bool() const { return is_valid(); }

    int key() const { return key_; }
    int mod() const { return mod_; }
    unsigned joy() const { return joy_; }

    constexpr bool operator==(const UserInput& other) const {
        return device_ == other.device_ && mod_ == other.mod_ &&
               key_ == other.key_ && joy_ == other.joy_;
    }
    constexpr bool operator!=(const UserInput& other) const {
        return !(*this == other);
    }
    constexpr bool operator<(const UserInput& other) const {
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
    constexpr bool operator<=(const UserInput& other) const {
        return !(*this > other);
    }
    constexpr bool operator>(const UserInput& other) const {
        return other < *this;
    }
    constexpr bool operator>=(const UserInput& other) const {
        return !(*this < other);
    }

private:
    UserInput(Device device, int mod, int key, unsigned joy)
        : device_(device),
          joystick_(joy == 0 ? wxJoystick::Invalid()
                             : wxJoystick::FromLegacyPlayerIndex(joy)),
          mod_(mod),
          key_(key),
          joy_(joy) {}

    Device device_;
    wxJoystick joystick_;
    int mod_;
    int key_;
    unsigned joy_;
};

}  // namespace config

#endif  // VBAM_WX_CONFIG_USER_INPUT_H_
