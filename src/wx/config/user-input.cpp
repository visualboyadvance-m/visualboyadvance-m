#include "wx/config/user-input.h"

#include <map>

#include <wx/accel.h>
#include <wx/log.h>
#include <wx/regex.h>
#include <wx/translation.h>

#include "wx/config/strutils.h"

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
        {"ALT", KeyboardInput(WXK_ALT, wxMOD_ALT)},
        {"SHIFT", KeyboardInput(WXK_SHIFT, wxMOD_SHIFT)},
        {"RAWCTRL", KeyboardInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAW_CTRL", KeyboardInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAWCONTROL", KeyboardInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAW_CONTROL", KeyboardInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"CTRL", KeyboardInput(WXK_CONTROL, wxMOD_CONTROL)},
        {"CONTROL", KeyboardInput(WXK_CONTROL, wxMOD_CONTROL)},
    };

    if (string.empty()) {
        return UserInput();
    }

    if (kJoyRegex.Matches(string)) {
        // Joystick.
        size_t start, length;
        kJoyRegex.GetMatch(&start, &length, 1);
        const int joy = StringToInt(string.Mid(start, length));
        const JoyId joy_id(joy - 1);
        const wxString remainder = string.Mid(start + length + 1);
        if (kAxisRegex.Matches(remainder)) {
            kAxisRegex.GetMatch(&start, &length, 1);
            const int key = StringToInt(remainder.Mid(start, length));
            const JoyControl control =
                remainder[start + length] == '+' ? JoyControl::AxisPlus : JoyControl::AxisMinus;
            return JoyInput(joy_id, control, key);
        }
        if (kButtonRegex.Matches(remainder)) {
            kButtonRegex.GetMatch(&start, &length, 1);
            const int key = StringToInt(remainder.Mid(start, length));
            return JoyInput(joy_id, JoyControl::Button, key);
        }
        if (kHatRegex.Matches(remainder)) {
            kHatRegex.GetMatch(&start, &length, 1);
            const int key = StringToInt(remainder.Mid(start, length));
            if (kHatRegex.GetMatch(remainder, 3).Length()) {
                return JoyInput(joy_id, JoyControl::HatNorth, key);
            } else if (kHatRegex.GetMatch(remainder, 4).Length()) {
                return JoyInput(joy_id, JoyControl::HatSouth, key);
            } else if (kHatRegex.GetMatch(remainder, 5).Length()) {
                return JoyInput(joy_id, JoyControl::HatEast, key);
            } else if (kHatRegex.GetMatch(remainder, 6).Length()) {
                return JoyInput(joy_id, JoyControl::HatWest, key);
            }
        }

        // Invalid.
        return UserInput();
    }

    // Not a joystick.

    // Non-ASCII keyboard input are treated as a pair of integers "key:mod".
    const auto pair = config::str_split(string, ":");
    long mod = 0;
    long key = 0;
    if (pair.size() == 2 && pair[0].ToLong(&key) && pair[1].ToLong(&mod)) {
        // Pair of integers, likely a unicode input.
        return KeyboardInput(static_cast<wxKeyCode>(key), static_cast<wxKeyModifier>(mod));
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
        key = accel.GetKeyCode();
        if (key < WXK_START && wxIslower(key)) {
            key = wxToupper(key);
        }
        return KeyboardInput(static_cast<wxKeyCode>(key),
                             static_cast<wxKeyModifier>(accel.GetFlags()));
    }

    // Invalid.
    return UserInput();
}

}  // namespace

// static
JoyId JoyId::Invalid() {
    static constexpr int kInvalidSdlIndex = -1;
    return JoyId(kInvalidSdlIndex);
}

wxString JoyId::ToConfigString() const {
    return wxString::Format("Joy%d", sdl_index_ + 1);
}

wxString JoyId::ToLocalizedString() const {
    return wxString::Format(_("Joystick %d"), sdl_index_ + 1);
}

wxString JoyInput::ToConfigString() const {
    const wxString joy_string = joy_.ToConfigString();
    switch (control_) {
        case JoyControl::AxisPlus:
            return wxString::Format("%s-Axis%d+", joy_string, control_index_);
        case JoyControl::AxisMinus:
            return wxString::Format("%s-Axis%d-", joy_string, control_index_);
        case JoyControl::Button:
            return wxString::Format("%s-Button%d", joy_string, control_index_);
        case JoyControl::HatNorth:
            return wxString::Format("%s-Hat%dN", joy_string, control_index_);
        case JoyControl::HatSouth:
            return wxString::Format("%s-Hat%dS", joy_string, control_index_);
        case JoyControl::HatWest:
            return wxString::Format("%s-Hat%dW", joy_string, control_index_);
        case JoyControl::HatEast:
            return wxString::Format("%s-Hat%dE", joy_string, control_index_);
    }

    VBAM_NOTREACHED();
    return wxEmptyString;
}

wxString JoyInput::ToLocalizedString() const {
    const wxString joy_string = joy_.ToLocalizedString();
    switch (control_) {
        case JoyControl::AxisPlus:
            return wxString::Format(_("%s: Axis %d+"), joy_string, control_index_);
        case JoyControl::AxisMinus:
            return wxString::Format(_("%s: Axis %d-"), joy_string, control_index_);
        case JoyControl::Button:
            return wxString::Format(_("%s: Button %d"), joy_string, control_index_);
        case JoyControl::HatNorth:
            return wxString::Format(_("%s: Hat %d North"), joy_string, control_index_);
        case JoyControl::HatSouth:
            return wxString::Format(_("%s: Hat %d South"), joy_string, control_index_);
        case JoyControl::HatWest:
            return wxString::Format(_("%s: Hat %d West"), joy_string, control_index_);
        case JoyControl::HatEast:
            return wxString::Format(_("%s: Hat %d East"), joy_string, control_index_);
    }

    VBAM_NOTREACHED();
    return wxEmptyString;
}

wxString KeyboardInput::ToConfigString() const {
    // Handle the modifier case separately.
    if (KeyIsModifier(key_)) {
        return ModToConfigString(mod_).RemoveLast();
    }

    // Custom overrides.
    const auto iter = kKeyCodeOverrides.find(key_);
    if (iter != kKeyCodeOverrides.end()) {
        return ModToConfigString(mod_) + iter->second.config_name;
    }

    if (key_ == ',' || key_ == ':') {
        // Special case for comma and colon to avoid parsing issues.
        return wxString::Format("%d:%d", key_, mod_);
    }

    const wxString accel_string = wxAcceleratorEntry(mod_, key_).ToRawString().MakeUpper();
    if (!accel_string.IsAscii()) {
        // Unicode handling.
        return wxString::Format("%d:%d", key_, mod_);
    }
    return accel_string;
}

wxString KeyboardInput::ToLocalizedString() const {
    // Handle the modifier case separately.
    if (KeyIsModifier(key_)) {
        return ModToLocalizedString(mod_) + _("Key");
    }

    // Custom overrides.
    const auto iter = kKeyCodeOverrides.find(key_);
    if (iter != kKeyCodeOverrides.end()) {
        return ModToLocalizedString(mod_) + iter->second.display_name;
    }

    const wxString accel_string = wxAcceleratorEntry(mod_, key_).ToRawString().MakeUpper();
    return accel_string;
}

// static
std::unordered_set<UserInput> UserInput::FromConfigString(const wxString& string) {
    std::unordered_set<UserInput> user_inputs;

    if (string.empty()) {
        return user_inputs;
    }

    for (const auto& token : config::str_split_with_sep(string, ",")) {
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
wxString UserInput::SpanToConfigString(const std::unordered_set<UserInput>& user_inputs) {
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
            return keyboard_input().ToConfigString();
        case Device::Joystick:
            return joy_input().ToConfigString();
    }

    VBAM_NOTREACHED();
    return wxEmptyString;
}

wxString UserInput::ToLocalizedString() const {
    switch (device_) {
        case Device::Invalid:
            return wxEmptyString;
        case Device::Keyboard:
            return keyboard_input().ToLocalizedString();
        case Device::Joystick:
            return joy_input().ToLocalizedString();
    }

    VBAM_NOTREACHED();
    return wxEmptyString;
}

}  // namespace config
