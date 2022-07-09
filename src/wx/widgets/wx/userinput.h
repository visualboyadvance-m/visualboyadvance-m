#ifndef _WX_USER_INPUT_H_
#define _WX_USER_INPUT_H_

#include <optional>

#include "wx/joyedit.h"
#include "wx/sdljoy.h"

// Abstraction for a user input, which can come from a keyboard or a joystick.
// This class implements comparison operators so it can be used in sets and as
// a key in maps.
//
// TODO: Right now, this class is implemented as a thin wrapper around the key,
// mod and joy user input representation used in many places in the code base.
// This is to ease a transition away from the wxJoyKeyBinding type, which
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

    // Constructor from a configuration string. Returns wxUserInput::Invalid()
    // on parsing failure.
    static wxUserInput FromString(const wxString& string);

    // TODO: Remove this once all uses of wxJoyKeyBinding have been removed.
    static wxUserInput FromLegacyJoyKeyBinding(const wxJoyKeyBinding& binding);

    // Converts to a configuration string. Computed on first call, and cached
    // for further calls.
    wxString ToString();

    // TODO: Remove these accessors once all callers have been removed.
    int mod() const { return mod_; }
    int key() const { return key_; }
    int joy() const { return joy_; }

    bool operator==(const wxUserInput& other) const;
    bool operator!=(const wxUserInput& other) const;
    bool operator<(const wxUserInput& other) const;
    bool operator<=(const wxUserInput& other) const;
    bool operator>(const wxUserInput& other) const;
    bool operator>=(const wxUserInput& other) const;

private:
    wxUserInput(Device device, int mod, uint8_t key, unsigned joy);

    Device device_;
    int mod_;
    uint8_t key_;
    unsigned joy_;

    wxString config_string_;
};


#endif  /* _WX_USER_INPUT_H */