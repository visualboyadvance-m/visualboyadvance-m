#include "config/user-input.h"

#include <map>

#include <wx/accel.h>
#include <wx/log.h>
#include <wx/regex.h>
#include <wx/translation.h>

#include "strutils.h"
#include "wxutil.h"

namespace config {

namespace {

struct KeyOverride {
    wxString config_name;
    wxString display_name;
};

static const std::map<wxKeyCode, KeyOverride> kKeyCodeOverrides = {
    {WXK_BACK, {"BACK", _("Backspace")}},
    {WXK_DELETE, {"DELETE", _("Delete")}},
    {WXK_PAGEUP, {"PAGEUP", _("Page Up")}},
    {WXK_PAGEDOWN, {"PAGEDOWN", _("Page Down")}},
    {WXK_NUMLOCK, {"NUM_LOCK", _("Num Lock")}},
    {WXK_SCROLL, {"SCROLL_LOCK", _("Scroll Lock")}},

    // Numpad.
    {WXK_NUMPAD_SPACE, {"KP_SPACE", _("Num Space")}},
    {WXK_NUMPAD_TAB, {"KP_TAB", _("Num Tab")}},
    {WXK_NUMPAD_ENTER, {"KP_ENTER", _("Num Enter")}},
    {WXK_NUMPAD_HOME, {"KP_HOME", _("Num Home")}},
    {WXK_NUMPAD_LEFT, {"KP_LEFT", _("Num left")}},
    {WXK_NUMPAD_UP, {"KP_UP", _("Num Up")}},
    {WXK_NUMPAD_RIGHT, {"KP_RIGHT", _("Num Right")}},
    {WXK_NUMPAD_DOWN, {"KP_DOWN", _("Num Down")}},
    {WXK_NUMPAD_PAGEUP, {"KP_PAGEUP", _("Num PageUp")}},
    {WXK_NUMPAD_PAGEDOWN, {"KP_PAGEDOWN", _("Num PageDown")}},
    {WXK_NUMPAD_END, {"KP_END", _("Num End")}},
    {WXK_NUMPAD_BEGIN, {"KP_BEGIN", _("Num Begin")}},
    {WXK_NUMPAD_INSERT, {"KP_INSERT", _("Num Insert")}},
    {WXK_NUMPAD_DELETE, {"KP_DELETE", _("Num Delete")}},
    {WXK_NUMPAD_EQUAL, {"KP_EQUAL", _("Num =")}},
    {WXK_NUMPAD_MULTIPLY, {"KP_MULTIPLY", _("Num *")}},
    {WXK_NUMPAD_ADD, {"KP_ADD", _("Num +")}},
    {WXK_NUMPAD_SEPARATOR, {"KP_SEPARATOR", _("Num ,")}},
    {WXK_NUMPAD_SUBTRACT, {"KP_SUBTRACT", _("Num -")}},
    {WXK_NUMPAD_DECIMAL, {"KP_DECIMAL", _("Num .")}},
    {WXK_NUMPAD_DIVIDE, {"KP_DIVIDE", _("Num /")}},

#if wxCHECK_VERSION(3, 1, 0)
    // Media keys.
    {WXK_VOLUME_MUTE, {"VOL_MUTE", _("Volume Mute")}},
    {WXK_VOLUME_DOWN, {"VOL_DOWN", _("Volume Down")}},
    {WXK_VOLUME_UP, {"VOL_UP", _("Volume Up")}},
    {WXK_MEDIA_NEXT_TRACK, {"MEDIA_NEXT_TRACK", _("Next Track")}},
    {WXK_MEDIA_PREV_TRACK, {"MEDIA_PREV_TRACK", _("Previous Track")}},
    {WXK_MEDIA_STOP, {"MEDIA_STOP", _("Stop")}},
    {WXK_MEDIA_PLAY_PAUSE, {"MEDIA_PLAY_PAUSE", _("Play/Pause")}},
#endif  // wxCHECK_VERSION(3, 1, 0)
};

bool KeyIsModifier(int key) {
    return key == WXK_CONTROL || key == WXK_SHIFT || key == WXK_ALT || key == WXK_RAW_CONTROL;
}

wxString ModToConfigString(int mod) {
    wxString config_string;
    if (mod & wxMOD_ALT) {
        config_string += "ALT+";
    }
    if (mod & wxMOD_CONTROL) {
        config_string += "CTRL+";
    }
#ifdef __WXMAC__
    if (mod & wxMOD_RAW_CONTROL) {
        config_string += "RAWCTRL+";
    }
#endif
    if (mod & wxMOD_SHIFT) {
        config_string += "SHIFT+";
    }
    if (mod & wxMOD_META) {
        config_string += "META+";
    }
    return config_string;
}

wxString ModToLocalizedString(int mod) {
    wxString config_string;
    if (mod & wxMOD_ALT) {
        config_string += _("Alt+");
    }
    if (mod & wxMOD_CONTROL) {
        config_string += _("Ctrl+");
    }
#ifdef __WXMAC__
    if (mod & wxMOD_RAW_CONTROL) {
        config_string += _("Rawctrl+");
    }
#endif
    if (mod & wxMOD_SHIFT) {
        config_string += _("Shift+");
    }
    if (mod & wxMOD_META) {
        config_string += _("Meta+");
    }
    return config_string;
}

wxString KeyboardInputToConfigString(int mod, int key) {
    // Handle the modifier case separately.
    if (KeyIsModifier(key)) {
        return wxString::Format("%d:%d", key, mod);
    }

    // Custom overrides.
    const auto iter = kKeyCodeOverrides.find(static_cast<wxKeyCode>(key));
    if (iter != kKeyCodeOverrides.end()) {
        return ModToConfigString(mod) + iter->second.config_name;
    }

    const wxString accel_string = wxAcceleratorEntry(mod, key).ToRawString().MakeUpper();
    if (!accel_string.IsAscii()) {
        // Unicode handling.
        return wxString::Format("%d:%d", key, mod);
    }
    return accel_string;
}

wxString KeyboardInputToLocalizedString(int mod, int key) {
    static const std::map<int, wxString> kStandaloneModifiers = {
        {WXK_ALT, _("Alt")},
        {WXK_SHIFT, _("Shift")},
#ifdef __WXMAC__
        {WXK_RAW_CONTROL, _("Ctrl")},
        {WXK_COMMAND, _("Cmd")},
#else
        {WXK_CONTROL, _("Ctrl")},
#endif
    };

    const auto iter = kStandaloneModifiers.find(key);
    if (iter != kStandaloneModifiers.end()) {
        return iter->second;
    }

    // Custom overrides.
    const auto iter2 = kKeyCodeOverrides.find(static_cast<wxKeyCode>(key));
    if (iter2 != kKeyCodeOverrides.end()) {
        return ModToLocalizedString(mod) + iter2->second.display_name;
    }

    return wxAcceleratorEntry(mod, key).ToString();
}

int StringToInt(const wxString& string) {
    int ret = 0;
    for (const auto& c : string) {
        ret = ret * 10 + (c - '0');
    }
    return ret;
}

UserInput StringToUserInput(const wxString& string) {
    // Regex used to parse joystick input.
    static const wxRegEx kJoyRegex("^Joy([0-9]+)-", wxRE_EXTENDED | wxRE_ICASE);
    static const wxRegEx kAxisRegex("^Axis([0-9]+)[-+]$", wxRE_EXTENDED | wxRE_ICASE);
    static const wxRegEx kButtonRegex("^Button([0-9]+)$", wxRE_EXTENDED | wxRE_ICASE);
    static const wxRegEx kHatRegex(
        "^Hat([0-9]+)"
        "((N|North|U|Up|NE|NorthEast|UR|UpRight)|"
        "(S|South|D|Down|SW|SouthWest|DL|DownLeft)|"
        "(E|East|R|Right|SE|SouthEast|DR|DownRight)|"
        "(W|West|L|Left|NW|NorthWest|UL|UpLeft))$",
        wxRE_EXTENDED | wxRE_ICASE);

    // Standalone modifiers do not get parsed properly by wxWidgets.
    static const std::map<wxString, UserInput> kStandaloneModifiers = {
        {"ALT", UserInput(WXK_ALT, wxMOD_ALT)},
        {"SHIFT", UserInput(WXK_SHIFT, wxMOD_SHIFT)},
        {"RAWCTRL", UserInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAW_CTRL", UserInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAWCONTROL", UserInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAW_CONTROL", UserInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"CTRL", UserInput(WXK_CONTROL, wxMOD_CONTROL)},
        {"CONTROL", UserInput(WXK_CONTROL, wxMOD_CONTROL)},
    };

    if (string.empty()) {
        return UserInput();
    }

    if (kJoyRegex.Matches(string)) {
        // Joystick.
        size_t start, length;
        kJoyRegex.GetMatch(&start, &length, 1);
        const int joy = StringToInt(string.Mid(start, length));
        const wxString remainder = string.Mid(start + length + 1);
        if (kAxisRegex.Matches(remainder)) {
            kAxisRegex.GetMatch(&start, &length, 1);
            const int key = StringToInt(remainder.Mid(start, length));
            const int mod =
                remainder[start + length] == '+' ? wxJoyControl::AxisPlus : wxJoyControl::AxisMinus;
            return UserInput(key, mod, joy);
        }
        if (kButtonRegex.Matches(remainder)) {
            kButtonRegex.GetMatch(&start, &length, 1);
            const int key = StringToInt(remainder.Mid(start, length));
            return UserInput(key, wxJoyControl::Button, joy);
        }
        if (kHatRegex.Matches(remainder)) {
            kHatRegex.GetMatch(&start, &length, 1);
            const int key = StringToInt(remainder.Mid(start, length));
            if (kHatRegex.GetMatch(&start, &length, 3)) {
                return UserInput(key, wxJoyControl::HatNorth, joy);
            } else if (kHatRegex.GetMatch(&start, &length, 4)) {
                return UserInput(key, wxJoyControl::HatSouth, joy);
            } else if (kHatRegex.GetMatch(&start, &length, 5)) {
                return UserInput(key, wxJoyControl::HatEast, joy);
            } else if (kHatRegex.GetMatch(&start, &length, 6)) {
                return UserInput(key, wxJoyControl::HatWest, joy);
            }
        }

        // Invalid.
        return UserInput();
    }

    // Not a joystick.

    const auto pair = strutils::split(string, ":");
    long mod, key;
    if (pair.size() == 2 && pair[0].ToLong(&key) && pair[1].ToLong(&mod)) {
        // Pair of integers, likely a unicode input.
        return UserInput(key, mod);
    }

    wxString upper(string);
    upper.MakeUpper();

    const auto iter = kStandaloneModifiers.find(upper);
    if (iter != kStandaloneModifiers.end()) {
        // Stand-alone modifier key.
        return iter->second;
    }

    // Need to disable logging to parse the string.
    const wxLogNull disable_logging;
    wxAcceleratorEntry accel;
    if (accel.FromString(string)) {
        // This is weird, but keycode events are sent as "uppercase", but the
        // accelerator parsing always considers them "lowercase", so we force
        // an uppercase conversion here.
        int key = accel.GetKeyCode();
        if (key < WXK_START && wxIslower(key)) {
            key = wxToupper(key);
        }
        return UserInput(key, accel.GetFlags());
    }

    // Invalid.
    return UserInput();
}

}  // namespace

UserInput::UserInput(const wxKeyEvent& event)
    : UserInput(Device::Keyboard, event.GetModifiers(), getKeyboardKeyCode(event), 0) {}

UserInput::UserInput(const wxJoyEvent& event)
    : UserInput(Device::Joystick,
                event.control(),
                event.control_index(),
                event.joystick().player_index()) {}

// static
std::set<UserInput> UserInput::FromConfigString(const wxString& string) {
    std::set<UserInput> user_inputs;

    if (string.empty()) {
        return user_inputs;
    }

    for (const auto& token : strutils::split_with_sep(string, ",")) {
        UserInput user_input = StringToUserInput(token);
        if (!user_input) {
            user_inputs.clear();
            return user_inputs;
        }
        user_inputs.emplace(std::move(user_input));
    }
    return user_inputs;
}

// static
wxString UserInput::SpanToConfigString(const std::set<UserInput>& user_inputs) {
    wxString config_string;
    if (user_inputs.empty()) {
        return config_string;
    }
    for (const UserInput& user_input : user_inputs) {
        config_string += user_input.ToConfigString() + ',';
    }
    return config_string.SubString(0, config_string.size() - 2);
}

wxString UserInput::ToConfigString() const {
    switch (device_) {
        case Device::Invalid:
            return wxEmptyString;
        case Device::Keyboard:
            return KeyboardInputToConfigString(mod_, key_);
        case Device::Joystick:
            wxString key;
            switch (mod_) {
                case wxJoyControl::AxisPlus:
                    key = wxString::Format(("Axis%d+"), key_);
                    break;
                case wxJoyControl::AxisMinus:
                    key = wxString::Format(("Axis%d-"), key_);
                    break;
                case wxJoyControl::Button:
                    key = wxString::Format(("Button%d"), key_);
                    break;
                case wxJoyControl::HatNorth:
                    key = wxString::Format(("Hat%dN"), key_);
                    break;
                case wxJoyControl::HatSouth:
                    key = wxString::Format(("Hat%dS"), key_);
                    break;
                case wxJoyControl::HatWest:
                    key = wxString::Format(("Hat%dW"), key_);
                    break;
                case wxJoyControl::HatEast:
                    key = wxString::Format(("Hat%dE"), key_);
                    break;
            }

            return wxString::Format("Joy%d-%s", joy_, key);
    }

    // Unreachable.
    assert(false);
    return wxEmptyString;
}

wxString UserInput::ToLocalizedString() const {
    switch (device_) {
        case Device::Invalid:
            return wxEmptyString;
        case Device::Keyboard:
            return KeyboardInputToLocalizedString(mod_, key_);
        case Device::Joystick:
            // Same as Config string.
            return ToConfigString();
    }

    // Unreachable.
    assert(false);
    return wxEmptyString;
}

}  // namespace config
