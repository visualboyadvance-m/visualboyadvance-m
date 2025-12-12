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

wxString ModToConfigString(uint32_t mod) {
    wxString config_string;
    // Check for extended (L/R specific) modifiers first
    if (mod & kKeyModLeftAlt) {
        config_string += "LALT+";
    } else if (mod & kKeyModRightAlt) {
        config_string += "RALT+";
    } else if (mod & (kKeyModAlt | wxMOD_ALT)) {
        config_string += "ALT+";
    }
    // For control modifiers, extended L/R flags take precedence.
    // On macOS, both wxMOD_RAW_CONTROL (base) and kKeyModL/RControl (extended)
    // may be set when pressing actual Ctrl key. Extended takes priority.
    if (mod & kKeyModLeftControl) {
        config_string += "LCTRL+";
    } else if (mod & kKeyModRightControl) {
        config_string += "RCTRL+";
    } else if (mod & (kKeyModControl | wxMOD_CONTROL)) {
        config_string += "CTRL+";
#ifdef __WXMAC__
    } else if (mod & wxMOD_RAW_CONTROL) {
        // Only output RAWCTRL if no extended control modifier was set
        config_string += "RAWCTRL+";
#endif
    }
    if (mod & kKeyModLeftShift) {
        config_string += "LSHIFT+";
    } else if (mod & kKeyModRightShift) {
        config_string += "RSHIFT+";
    } else if (mod & (kKeyModShift | wxMOD_SHIFT)) {
        config_string += "SHIFT+";
    }
    if (mod & kKeyModLeftMeta) {
        config_string += "LMETA+";
    } else if (mod & kKeyModRightMeta) {
        config_string += "RMETA+";
    } else if (mod & (kKeyModMeta | wxMOD_META)) {
        config_string += "META+";
    }
    return config_string;
}

wxString ModToLocalizedString(uint32_t mod) {
    wxString config_string;
    // Check for extended (L/R specific) modifiers first
    if (mod & kKeyModLeftAlt) {
        config_string += _("LAlt+");
    } else if (mod & kKeyModRightAlt) {
        config_string += _("RAlt+");
    } else if (mod & (kKeyModAlt | wxMOD_ALT)) {
        config_string += _("Alt+");
    }
    // For control modifiers, extended L/R flags take precedence.
    // On macOS, both wxMOD_RAW_CONTROL (base) and kKeyModL/RControl (extended)
    // may be set when pressing actual Ctrl key. Extended takes priority.
#ifdef __WXMAC__
    // On macOS: wxMOD_CONTROL is Command key, wxMOD_RAW_CONTROL is actual Control key
    if (mod & kKeyModLeftControl) {
        // Left Control key (actual Control, not Command)
        config_string += _("LCtrl+");
    } else if (mod & kKeyModRightControl) {
        // Right Control key (actual Control, not Command)
        config_string += _("RCtrl+");
    } else if (mod & (kKeyModControl | wxMOD_CONTROL)) {
        // Command key on macOS
        config_string += _("Cmd+");
    } else if (mod & wxMOD_RAW_CONTROL) {
        // Actual Control key (only if no extended control modifier was set)
        config_string += _("Ctrl+");
    }
#else
    // On other platforms: wxMOD_CONTROL is the standard Control key
    if (mod & kKeyModLeftControl) {
        config_string += _("LCtrl+");
    } else if (mod & kKeyModRightControl) {
        config_string += _("RCtrl+");
    } else if (mod & (kKeyModControl | wxMOD_CONTROL)) {
        config_string += _("Ctrl+");
    }
#endif
    if (mod & kKeyModLeftShift) {
        config_string += _("LShift+");
    } else if (mod & kKeyModRightShift) {
        config_string += _("RShift+");
    } else if (mod & (kKeyModShift | wxMOD_SHIFT)) {
        config_string += _("Shift+");
    }
    if (mod & (kKeyModLeftMeta | kKeyModRightMeta | kKeyModMeta | wxMOD_META)) {
#ifdef __WXMAC__
        // On macOS, Meta/GUI is the Command key (no left/right distinction needed)
        config_string += _("Cmd+");
#else
        // On other platforms, show left/right distinction for Meta keys
        if (mod & kKeyModLeftMeta) {
            config_string += _("LMeta+");
        } else if (mod & kKeyModRightMeta) {
            config_string += _("RMeta+");
        } else {
            config_string += _("Meta+");
        }
#endif
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
        {"LALT", KeyboardInput(WXK_ALT, kKeyModLeftAlt)},
        {"RALT", KeyboardInput(WXK_ALT, kKeyModRightAlt)},
        {"SHIFT", KeyboardInput(WXK_SHIFT, wxMOD_SHIFT)},
        {"LSHIFT", KeyboardInput(WXK_SHIFT, kKeyModLeftShift)},
        {"RSHIFT", KeyboardInput(WXK_SHIFT, kKeyModRightShift)},
        {"RAWCTRL", KeyboardInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAW_CTRL", KeyboardInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAWCONTROL", KeyboardInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
        {"RAW_CONTROL", KeyboardInput(WXK_RAW_CONTROL, wxMOD_RAW_CONTROL)},
#ifdef __WXMAC__
        // On macOS, Command key is WXK_CONTROL and actual Ctrl is WXK_RAW_CONTROL.
        // LCTRL/RCTRL refer to the actual Ctrl key (WXK_RAW_CONTROL) for consistency
        // with what the keyboard handler generates when these keys are pressed.
        {"CTRL", KeyboardInput(WXK_CONTROL, wxMOD_CONTROL)},
        {"CONTROL", KeyboardInput(WXK_CONTROL, wxMOD_CONTROL)},
        {"LCTRL", KeyboardInput(WXK_RAW_CONTROL, kKeyModLeftControl)},
        {"LCONTROL", KeyboardInput(WXK_RAW_CONTROL, kKeyModLeftControl)},
        {"RCTRL", KeyboardInput(WXK_RAW_CONTROL, kKeyModRightControl)},
        {"RCONTROL", KeyboardInput(WXK_RAW_CONTROL, kKeyModRightControl)},
#else
        {"CTRL", KeyboardInput(WXK_CONTROL, wxMOD_CONTROL)},
        {"CONTROL", KeyboardInput(WXK_CONTROL, wxMOD_CONTROL)},
        {"LCTRL", KeyboardInput(WXK_CONTROL, kKeyModLeftControl)},
        {"LCONTROL", KeyboardInput(WXK_CONTROL, kKeyModLeftControl)},
        {"RCTRL", KeyboardInput(WXK_CONTROL, kKeyModRightControl)},
        {"RCONTROL", KeyboardInput(WXK_CONTROL, kKeyModRightControl)},
#endif
        {"META", KeyboardInput(WXK_WINDOWS_LEFT, wxMOD_META)},
        {"LMETA", KeyboardInput(WXK_WINDOWS_LEFT, kKeyModLeftMeta)},
        {"RMETA", KeyboardInput(WXK_WINDOWS_RIGHT, kKeyModRightMeta)},
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

    // Parse extended modifiers (LCTRL+, RCTRL+, LALT+, RALT+, LSHIFT+, RSHIFT+, LMETA+, RMETA+)
    // that wxAcceleratorEntry doesn't understand.
    // Map of extended modifier prefixes to their flags
    static const std::vector<std::pair<wxString, uint32_t>> kExtendedModPrefixes = {
        {"LCTRL+", kKeyModLeftControl},
        {"LCONTROL+", kKeyModLeftControl},
        {"RCTRL+", kKeyModRightControl},
        {"RCONTROL+", kKeyModRightControl},
        {"LALT+", kKeyModLeftAlt},
        {"RALT+", kKeyModRightAlt},
        {"LSHIFT+", kKeyModLeftShift},
        {"RSHIFT+", kKeyModRightShift},
        {"LMETA+", kKeyModLeftMeta},
        {"RMETA+", kKeyModRightMeta},
    };

    uint32_t extended_mod = kKeyModNone;
    wxString remaining = upper;
    bool found_extended = true;
    while (found_extended) {
        found_extended = false;
        for (const auto& prefix : kExtendedModPrefixes) {
            if (remaining.StartsWith(prefix.first)) {
                extended_mod |= prefix.second;
                remaining = remaining.Mid(prefix.first.length());
                found_extended = true;
                break;
            }
        }
    }

    if (extended_mod != kKeyModNone) {
        // We found extended modifiers, parse the rest with wxAcceleratorEntry
        // but we also need to handle the remaining part which may have standard modifiers
        const wxLogNull disable_logging;
        wxAcceleratorEntry accel;
        // If remaining is just a key (no +), we need to format it properly
        wxString accel_string = remaining;
        if (!remaining.Contains("+")) {
            // Just a key, no standard modifiers
            accel_string = remaining;
        }
        // Try to parse what's left
        if (accel.FromString(accel_string)) {
            key = accel.GetKeyCode();
            if (key < WXK_START && wxIslower(key)) {
                key = wxToupper(key);
            }
            // Combine extended modifiers with any standard modifiers from accel
            extended_mod |= static_cast<uint32_t>(accel.GetFlags());
            return KeyboardInput(static_cast<wxKeyCode>(key), extended_mod);
        }
        // If FromString failed, try to parse as a single character or special key
        if (remaining.length() == 1) {
            key = static_cast<long>(remaining[0]);
            if (key >= 'a' && key <= 'z') {
                key = wxToupper(key);
            }
            return KeyboardInput(static_cast<wxKeyCode>(key), extended_mod);
        }
        // Check for special keys in our map
        for (const auto& kv : kKeyCodeOverrides) {
            if (remaining == kv.second.config_name) {
                return KeyboardInput(kv.first, extended_mod);
            }
        }
        // Invalid
        return UserInput();
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

    VBAM_NOTREACHED_RETURN(wxEmptyString);
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

    VBAM_NOTREACHED_RETURN(wxEmptyString);
}

wxString KeyboardInput::ToConfigString() const {
    // Handle the modifier case separately.
    if (KeyIsModifier(key_)) {
        wxString mod_str = ModToConfigString(mod_);
        if (!mod_str.empty()) {
            return mod_str.RemoveLast();
        }
        // Fallback for modifier keys without extended info
        switch (key_) {
            case WXK_SHIFT:
                return "SHIFT";
            case WXK_CONTROL:
                return "CTRL";
            case WXK_ALT:
                return "ALT";
#ifdef __WXMAC__
            case WXK_RAW_CONTROL:
                return "RAWCTRL";
#endif
            default:
                return wxEmptyString;
        }
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

    // For keys with extended modifiers, build the string manually
    if (HasExtendedModifiers(mod_)) {
        wxString result = ModToConfigString(mod_);
        // Get the key name using wxAcceleratorEntry but without modifiers
        wxString key_str = wxAcceleratorEntry(wxMOD_NONE, key_).ToRawString().MakeUpper();
        if (!key_str.IsAscii()) {
            return wxString::Format("%d:%d", key_, mod_);
        }
        return result + key_str;
    }

    const wxString accel_string = wxAcceleratorEntry(mod(), key_).ToRawString().MakeUpper();
    if (!accel_string.IsAscii()) {
        // Unicode handling.
        return wxString::Format("%d:%d", key_, mod_);
    }
    return accel_string;
}

wxString KeyboardInput::ToLocalizedString() const {
    // Handle the modifier case separately.
    if (KeyIsModifier(key_)) {
        // For standalone modifier keys, show the L/R distinction if available
        wxString mod_str = ModToLocalizedString(mod_);
        if (!mod_str.empty()) {
            // Remove trailing '+' and return just the modifier name
            return mod_str.RemoveLast();
        }
        // Fallback for modifier keys without extended info
        switch (key_) {
            case WXK_SHIFT:
                return _("Shift");
            case WXK_CONTROL:
#ifdef __WXMAC__
                // On macOS, WXK_CONTROL is the Command key
                return _("Cmd");
#else
                return _("Ctrl");
#endif
            case WXK_ALT:
                return _("Alt");
#ifdef __WXMAC__
            case WXK_RAW_CONTROL:
                // On macOS, WXK_RAW_CONTROL is the actual Control key
                return _("Ctrl");
#endif
            default:
                return _("Key");
        }
    }

    // Custom overrides.
    const auto iter = kKeyCodeOverrides.find(key_);
    if (iter != kKeyCodeOverrides.end()) {
        return ModToLocalizedString(mod_) + iter->second.display_name;
    }

    // For keys with extended modifiers, build the string manually
    if (HasExtendedModifiers(mod_)) {
        wxString result = ModToLocalizedString(mod_);
        // Get the key name
        wxString key_str;
        if (key_ >= 32 && key_ < 127) {
            // Printable ASCII character - use it directly
            key_str = wxString(static_cast<wxChar>(wxToupper(key_)));
        } else {
            // Special key - use wxAcceleratorEntry
            key_str = wxAcceleratorEntry(wxMOD_NONE, key_).ToRawString().MakeUpper();
        }
        return result + key_str;
    }

#ifdef __WXMAC__
    // On macOS, wxAcceleratorEntry::ToRawString() outputs "CTRL+" for wxMOD_CONTROL
    // even though wxMOD_CONTROL represents Command key. Use ModToLocalizedString instead.
    wxString result = ModToLocalizedString(mod_);
    wxString key_str;
    if (key_ >= 32 && key_ < 127) {
        // Printable ASCII character
        key_str = wxString(static_cast<wxChar>(wxToupper(key_)));
    } else {
        // Special key - use wxAcceleratorEntry for key name only
        key_str = wxAcceleratorEntry(wxMOD_NONE, key_).ToRawString().MakeUpper();
    }
    return result + key_str;
#else
    const wxString accel_string = wxAcceleratorEntry(mod(), key_).ToRawString().MakeUpper();
    return accel_string;
#endif
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

    VBAM_NOTREACHED_RETURN(wxEmptyString);
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

    VBAM_NOTREACHED_RETURN(wxEmptyString);
}

}  // namespace config
