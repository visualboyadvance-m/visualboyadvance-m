#ifndef _WX_USER_INPUT_H_
#define _WX_USER_INPUT_H_

#include <set>
#include <wx/event.h>

#include "wx/sdljoy.h"

// Abstraction for a user input, which can come from a keyboard or a joystick.
// This class implements comparison operators so it can be used in sets and as
// a key in maps.
//
// TODO: Right now, this class is implemented as a thin wrapper around the key,
// mod and joy user input representation used in many places in the code base.
// This is to ease a transition away from the key, mod, joy triplet, which
// wxUserInput will eventually replace.
class wxUserInput {
public:
    // The device type for a user control.
    enum class Device {
        Invalid = 0,
        Keyboard,
        Joystick,
        Last = Joystick
    };

    // Invalid wxUserInput, mainly used for comparison.
    static wxUserInput Invalid();

    // Constructor from a wxKeyEvent.
    static wxUserInput FromKeyEvent(const wxKeyEvent& event);

    // Constructor from a wxJoyEvent.
    static wxUserInput FromJoyEvent(const wxJoyEvent& event);

    // Constructor from a configuration string. Returns empty set on failure.
    static std::set<wxUserInput> FromString(const wxString& string);

    // TODO: Remove this once all uses have been removed.
    static wxUserInput FromLegacyKeyModJoy(int key = 0, int mod = 0, int joy = 0);

    // Converts a set of wxUserInput into a configuration string. This
    // recomputes the configuration string every time and should not be used
    // for comparison purposes.
    // TODO: Replace std::set with std::span when the code base uses C++20.
    static wxString SpanToString(
        const std::set<wxUserInput>& user_inputs, bool is_config = false);

    // Converts to a configuration string.
    wxString ToString(bool is_config = false) const;

    wxJoystick joystick() const { return joystick_; }
    bool is_valid() const { return device_ != Device::Invalid; }
    operator bool() const { return is_valid(); }

    bool operator==(const wxUserInput& other) const;
    bool operator!=(const wxUserInput& other) const;
    bool operator<(const wxUserInput& other) const;
    bool operator<=(const wxUserInput& other) const;
    bool operator>(const wxUserInput& other) const;
    bool operator>=(const wxUserInput& other) const;

private:
    wxUserInput(Device device, int mod, uint8_t key, unsigned joy);

    const Device device_;
    const wxJoystick joystick_;
    const int mod_;
    const uint8_t key_;
    const unsigned joy_;
};


#endif  /* _WX_USER_INPUT_H */