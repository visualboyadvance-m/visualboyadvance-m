#include "qt/config/user-input.h"

#include <map>
#include <vector>

#include <QCoreApplication>
#include <QKeySequence>
#include <QRegularExpression>

#include "qt/config/strutils.h"

namespace config {

namespace {

QString Tr(const char* s) {
    return QCoreApplication::translate("vbam", s);
}

// Named keys whose config string is not derived from QKeySequence. The names
// match the wx port's INI vocabulary so shortcut sections can be shared.
struct KeyOverride {
    const char* config_name;
    const char* display_name;  // untranslated
};

const std::map<int, KeyOverride>& KeyCodeOverrides() {
    static const std::map<int, KeyOverride> kKeyCodeOverrides = {
        {Qt::Key_Backspace, {"BACK", "Backspace"}},
        {Qt::Key_Delete, {"DELETE", "Delete"}},
        {Qt::Key_Insert, {"INSERT", "Insert"}},
        {Qt::Key_PageUp, {"PAGEUP", "Page Up"}},
        {Qt::Key_PageDown, {"PAGEDOWN", "Page Down"}},
        {Qt::Key_NumLock, {"NUM_LOCK", "Num Lock"}},
        {Qt::Key_ScrollLock, {"SCROLL_LOCK", "Scroll Lock"}},
        {Qt::Key_CapsLock, {"CAPITAL", "Caps Lock"}},
        {Qt::Key_Escape, {"ESCAPE", "Escape"}},
        {Qt::Key_Return, {"RETURN", "Return"}},
        // Qt distinguishes the keypad Enter key by key code.
        {Qt::Key_Enter, {"KP_ENTER", "Num Enter"}},
        {Qt::Key_Space, {"SPACE", "Space"}},
        {Qt::Key_Tab, {"TAB", "Tab"}},
        {Qt::Key_Print, {"PRINT", "Print"}},
        {Qt::Key_Pause, {"PAUSE", "Pause"}},
        {Qt::Key_Menu, {"MENU", "Menu"}},
        {Qt::Key_Help, {"HELP", "Help"}},
        {Qt::Key_Clear, {"CLEAR", "Clear"}},
        {Qt::Key_Home, {"HOME", "Home"}},
        {Qt::Key_End, {"END", "End"}},
        {Qt::Key_Left, {"LEFT", "Left"}},
        {Qt::Key_Up, {"UP", "Up"}},
        {Qt::Key_Right, {"RIGHT", "Right"}},
        {Qt::Key_Down, {"DOWN", "Down"}},

        // Media keys.
        {Qt::Key_VolumeMute, {"VOL_MUTE", "Volume Mute"}},
        {Qt::Key_VolumeDown, {"VOL_DOWN", "Volume Down"}},
        {Qt::Key_VolumeUp, {"VOL_UP", "Volume Up"}},
        {Qt::Key_MediaNext, {"MEDIA_NEXT_TRACK", "Next Track"}},
        {Qt::Key_MediaPrevious, {"MEDIA_PREV_TRACK", "Previous Track"}},
        {Qt::Key_MediaStop, {"MEDIA_STOP", "Stop"}},
        {Qt::Key_MediaTogglePlayPause, {"MEDIA_PLAY_PAUSE", "Play/Pause"}},
    };
    return kKeyCodeOverrides;
}

// Additional accepted spellings when parsing (wx names, Qt names, legacy
// numpad names). Qt does not give keypad keys their own key codes (except
// Enter), so the KP_ names map to the plain key.
const std::map<QString, int>& KeyNameAliases() {
    static const std::map<QString, int> kAliases = {
        {"ESC", Qt::Key_Escape},
        {"BACKSPACE", Qt::Key_Backspace},
        {"DEL", Qt::Key_Delete},
        {"INS", Qt::Key_Insert},
        {"PGUP", Qt::Key_PageUp},
        {"PGDOWN", Qt::Key_PageDown},
        {"PGDN", Qt::Key_PageDown},
        {"PRIOR", Qt::Key_PageUp},
        {"NEXT", Qt::Key_PageDown},
        {"ENTER", Qt::Key_Enter},
        {"CAPSLOCK", Qt::Key_CapsLock},
        {"NUMLOCK", Qt::Key_NumLock},
        {"SCROLLLOCK", Qt::Key_ScrollLock},
        {"PRINTSCREEN", Qt::Key_Print},
        {"SNAPSHOT", Qt::Key_Print},
        {"KP_SPACE", Qt::Key_Space},
        {"KP_TAB", Qt::Key_Tab},
        {"KP_HOME", Qt::Key_Home},
        {"KP_LEFT", Qt::Key_Left},
        {"KP_UP", Qt::Key_Up},
        {"KP_RIGHT", Qt::Key_Right},
        {"KP_DOWN", Qt::Key_Down},
        {"KP_PAGEUP", Qt::Key_PageUp},
        {"KP_PAGEDOWN", Qt::Key_PageDown},
        {"KP_END", Qt::Key_End},
        {"KP_BEGIN", Qt::Key_Clear},
        {"KP_INSERT", Qt::Key_Insert},
        {"KP_DELETE", Qt::Key_Delete},
        {"KP_EQUAL", Qt::Key_Equal},
        {"KP_MULTIPLY", Qt::Key_Asterisk},
        {"KP_ADD", Qt::Key_Plus},
        {"KP_SEPARATOR", Qt::Key_Comma},
        {"KP_SUBTRACT", Qt::Key_Minus},
        {"KP_DECIMAL", Qt::Key_Period},
        {"KP_DIVIDE", Qt::Key_Slash},
        {"KP0", Qt::Key_0},
        {"KP1", Qt::Key_1},
        {"KP2", Qt::Key_2},
        {"KP3", Qt::Key_3},
        {"KP4", Qt::Key_4},
        {"KP5", Qt::Key_5},
        {"KP6", Qt::Key_6},
        {"KP7", Qt::Key_7},
        {"KP8", Qt::Key_8},
        {"KP9", Qt::Key_9},
    };
    return kAliases;
}

bool KeyIsModifier(int key) {
    return key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt ||
           key == Qt::Key_Meta;
}

// Uppercase portable name of a key as QKeySequence spells it, or empty.
QString QtKeyName(int key) {
    if (key <= 0) {
        return QString();
    }
    const QString name = QKeySequence(key).toString(QKeySequence::PortableText);
    if (name.isEmpty()) {
        return QString();
    }
    return name.toUpper();
}

// Config-string name of a key (no modifiers), or empty if the key has none.
QString KeyToConfigName(int key) {
    const auto iter = KeyCodeOverrides().find(key);
    if (iter != KeyCodeOverrides().end()) {
        return QString::fromLatin1(iter->second.config_name);
    }
    if (key >= 0x20 && key < 0x7f) {
        // Printable ASCII: the character itself, uppercased (Qt key codes for
        // Latin-1 keys are the uppercase character).
        return QString(QChar(key)).toUpper();
    }
    const QString name = QtKeyName(key);
    // Names containing '+' or ',' or ':' would break parsing, and non-ASCII
    // names (accented letters, symbols) are written in the numeric form like
    // the wx port does, so the INI stays plain ASCII.
    if (name.contains('+') || name.contains(',') || name.contains(':')) {
        return QString();
    }
    for (const QChar c : name) {
        if (c.unicode() < 0x20 || c.unicode() >= 0x7f) {
            return QString();
        }
    }
    return name;
}

// Map of uppercase key name -> key code, built from the overrides, the
// aliases and the QKeySequence names of every Qt::Key that has one.
const std::map<QString, int>& KeyNameMap() {
    static const std::map<QString, int> kMap([] {
        std::map<QString, int> map;
        // Function keys, arrows, navigation, misc named keys.
        const std::vector<int> named_keys = {
            Qt::Key_Escape,     Qt::Key_Tab,        Qt::Key_Backtab,     Qt::Key_Backspace,
            Qt::Key_Return,     Qt::Key_Enter,      Qt::Key_Insert,      Qt::Key_Delete,
            Qt::Key_Pause,      Qt::Key_Print,      Qt::Key_SysReq,      Qt::Key_Clear,
            Qt::Key_Home,       Qt::Key_End,        Qt::Key_Left,        Qt::Key_Up,
            Qt::Key_Right,      Qt::Key_Down,       Qt::Key_PageUp,      Qt::Key_PageDown,
            Qt::Key_CapsLock,   Qt::Key_NumLock,    Qt::Key_ScrollLock,  Qt::Key_Menu,
            Qt::Key_Help,       Qt::Key_Space,      Qt::Key_Super_L,     Qt::Key_Super_R,
            Qt::Key_Hyper_L,    Qt::Key_Hyper_R,    Qt::Key_Back,        Qt::Key_Forward,
            Qt::Key_Stop,       Qt::Key_Refresh,    Qt::Key_VolumeDown,  Qt::Key_VolumeMute,
            Qt::Key_VolumeUp,   Qt::Key_MediaPlay,  Qt::Key_MediaStop,   Qt::Key_MediaPrevious,
            Qt::Key_MediaNext,  Qt::Key_MediaRecord, Qt::Key_MediaPause, Qt::Key_MediaTogglePlayPause,
            Qt::Key_HomePage,   Qt::Key_Favorites,  Qt::Key_Search,      Qt::Key_Standby,
            Qt::Key_OpenUrl,    Qt::Key_LaunchMail, Qt::Key_LaunchMedia, Qt::Key_Select,
            Qt::Key_Yes,        Qt::Key_No,         Qt::Key_Cancel,      Qt::Key_Printer,
            Qt::Key_Execute,    Qt::Key_Sleep,      Qt::Key_Play,        Qt::Key_Zoom,
            Qt::Key_Exit,
        };
        for (int key : named_keys) {
            const QString name = QtKeyName(key);
            if (!name.isEmpty()) {
                map.emplace(name, key);
            }
        }
        for (int key = Qt::Key_F1; key <= Qt::Key_F35; key++) {
            map.emplace(QtKeyName(key), key);
        }
        for (int key = Qt::Key_Launch0; key <= Qt::Key_LaunchF; key++) {
            const QString name = QtKeyName(key);
            if (!name.isEmpty()) {
                map.emplace(name, key);
            }
        }
        // Overrides and aliases take precedence.
        for (const auto& iter : KeyCodeOverrides()) {
            map[QString::fromLatin1(iter.second.config_name)] = iter.first;
        }
        for (const auto& iter : KeyNameAliases()) {
            map[iter.first] = iter.second;
        }
        return map;
    }());
    return kMap;
}

QString ModToConfigString(uint32_t mod) {
    QString config_string;
    // Ctrl, Alt, Shift, Meta: the order of wxAcceleratorEntry::ToRawString(),
    // so the common cases produce the same strings as the wx port.
    if (mod & kKeyModLeftControl) {
        config_string += "LCTRL+";
    } else if (mod & kKeyModRightControl) {
        config_string += "RCTRL+";
    } else if (mod & kKeyModControl) {
        config_string += "CTRL+";
    }
    if (mod & kKeyModLeftAlt) {
        config_string += "LALT+";
    } else if (mod & kKeyModRightAlt) {
        config_string += "RALT+";
    } else if (mod & kKeyModAlt) {
        config_string += "ALT+";
    }
    if (mod & kKeyModLeftShift) {
        config_string += "LSHIFT+";
    } else if (mod & kKeyModRightShift) {
        config_string += "RSHIFT+";
    } else if (mod & kKeyModShift) {
        config_string += "SHIFT+";
    }
    if (mod & kKeyModLeftMeta) {
        config_string += "LMETA+";
    } else if (mod & kKeyModRightMeta) {
        config_string += "RMETA+";
    } else if (mod & kKeyModMeta) {
        config_string += "META+";
    }
    return config_string;
}

QString ModToLocalizedString(uint32_t mod) {
    QString config_string;
#if defined(__APPLE__)
    // On macOS, Qt's ControlModifier is the Command key and MetaModifier the
    // actual Control key.
    const char* ctrl_name = "Cmd+";
    const char* lctrl_name = "LCmd+";
    const char* rctrl_name = "RCmd+";
    const char* meta_name = "Ctrl+";
    const char* lmeta_name = "LCtrl+";
    const char* rmeta_name = "RCtrl+";
#else
    const char* ctrl_name = "Ctrl+";
    const char* lctrl_name = "LCtrl+";
    const char* rctrl_name = "RCtrl+";
    const char* meta_name = "Meta+";
    const char* lmeta_name = "LMeta+";
    const char* rmeta_name = "RMeta+";
#endif
    if (mod & kKeyModLeftControl) {
        config_string += Tr(lctrl_name);
    } else if (mod & kKeyModRightControl) {
        config_string += Tr(rctrl_name);
    } else if (mod & kKeyModControl) {
        config_string += Tr(ctrl_name);
    }
    if (mod & kKeyModLeftAlt) {
        config_string += Tr("LAlt+");
    } else if (mod & kKeyModRightAlt) {
        config_string += Tr("RAlt+");
    } else if (mod & kKeyModAlt) {
        config_string += Tr("Alt+");
    }
    if (mod & kKeyModLeftShift) {
        config_string += Tr("LShift+");
    } else if (mod & kKeyModRightShift) {
        config_string += Tr("RShift+");
    } else if (mod & kKeyModShift) {
        config_string += Tr("Shift+");
    }
    if (mod & kKeyModLeftMeta) {
        config_string += Tr(lmeta_name);
    } else if (mod & kKeyModRightMeta) {
        config_string += Tr(rmeta_name);
    } else if (mod & kKeyModMeta) {
        config_string += Tr(meta_name);
    }
    return config_string;
}

// Localized (display) name of a key without modifiers.
QString KeyToLocalizedName(int key) {
    const auto iter = KeyCodeOverrides().find(key);
    if (iter != KeyCodeOverrides().end()) {
        return Tr(iter->second.display_name);
    }
    if (key >= 0x20 && key < 0x7f) {
        return QString(QChar(key)).toUpper();
    }
    if (key > 0) {
        const QString name = QKeySequence(key).toString(QKeySequence::NativeText);
        if (!name.isEmpty()) {
            return name;
        }
    }
    return QStringLiteral("%1").arg(key);
}

// Stand-alone modifier tokens.
const std::map<QString, UserInput>& StandaloneModifiers() {
    static const std::map<QString, UserInput> kStandaloneModifiers = {
        {"ALT", KeyboardInput(Qt::Key_Alt, kKeyModAlt)},
        {"LALT", KeyboardInput(Qt::Key_Alt, kKeyModLeftAlt)},
        {"RALT", KeyboardInput(Qt::Key_Alt, kKeyModRightAlt)},
        {"SHIFT", KeyboardInput(Qt::Key_Shift, kKeyModShift)},
        {"LSHIFT", KeyboardInput(Qt::Key_Shift, kKeyModLeftShift)},
        {"RSHIFT", KeyboardInput(Qt::Key_Shift, kKeyModRightShift)},
        {"CTRL", KeyboardInput(Qt::Key_Control, kKeyModControl)},
        {"CONTROL", KeyboardInput(Qt::Key_Control, kKeyModControl)},
        {"LCTRL", KeyboardInput(Qt::Key_Control, kKeyModLeftControl)},
        {"LCONTROL", KeyboardInput(Qt::Key_Control, kKeyModLeftControl)},
        {"RCTRL", KeyboardInput(Qt::Key_Control, kKeyModRightControl)},
        {"RCONTROL", KeyboardInput(Qt::Key_Control, kKeyModRightControl)},
        // wx macOS spelling of the actual Control key; Qt calls it Meta there.
        {"RAWCTRL", KeyboardInput(Qt::Key_Meta, kKeyModMeta)},
        {"RAW_CTRL", KeyboardInput(Qt::Key_Meta, kKeyModMeta)},
        {"RAWCONTROL", KeyboardInput(Qt::Key_Meta, kKeyModMeta)},
        {"RAW_CONTROL", KeyboardInput(Qt::Key_Meta, kKeyModMeta)},
        {"META", KeyboardInput(Qt::Key_Meta, kKeyModMeta)},
        {"LMETA", KeyboardInput(Qt::Key_Meta, kKeyModLeftMeta)},
        {"RMETA", KeyboardInput(Qt::Key_Meta, kKeyModRightMeta)},
    };
    return kStandaloneModifiers;
}

// Modifier prefixes accepted while parsing.
const std::vector<std::pair<QString, uint32_t>>& ModPrefixes() {
    static const std::vector<std::pair<QString, uint32_t>> kModPrefixes = {
        {"LCTRL+", kKeyModLeftControl},   {"LCONTROL+", kKeyModLeftControl},
        {"RCTRL+", kKeyModRightControl},  {"RCONTROL+", kKeyModRightControl},
        {"CTRL+", kKeyModControl},        {"CONTROL+", kKeyModControl},
        {"RAWCTRL+", kKeyModMeta},        {"RAW_CTRL+", kKeyModMeta},
        {"RAWCONTROL+", kKeyModMeta},     {"RAW_CONTROL+", kKeyModMeta},
        {"LALT+", kKeyModLeftAlt},        {"RALT+", kKeyModRightAlt},
        {"ALT+", kKeyModAlt},             {"LSHIFT+", kKeyModLeftShift},
        {"RSHIFT+", kKeyModRightShift},   {"SHIFT+", kKeyModShift},
        {"LMETA+", kKeyModLeftMeta},      {"RMETA+", kKeyModRightMeta},
        {"META+", kKeyModMeta},
    };
    return kModPrefixes;
}

UserInput StringToUserInput(const QString& string) {
    // Regex used to parse joystick input.
    static const QRegularExpression kJoyRegex("^Joy([0-9]+)-",
                                              QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kAxisRegex("^Axis([0-9]+)([-+])$",
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kButtonRegex("^Button([0-9]+)$",
                                                 QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kHatRegex(
        "^Hat([0-9]+)"
        "((N|North|U|Up|NE|NorthEast|UR|UpRight)|"
        "(S|South|D|Down|SW|SouthWest|DL|DownLeft)|"
        "(E|East|R|Right|SE|SouthEast|DR|DownRight)|"
        "(W|West|L|Left|NW|NorthWest|UL|UpLeft))$",
        QRegularExpression::CaseInsensitiveOption);

    if (string.isEmpty()) {
        return UserInput();
    }

    const QRegularExpressionMatch joy_match = kJoyRegex.match(string);
    if (joy_match.hasMatch()) {
        // Joystick.
        const int joy = joy_match.captured(1).toInt();
        const JoyId joy_id(joy - 1);
        const QString remainder = string.mid(joy_match.capturedEnd(0));

        const QRegularExpressionMatch axis_match = kAxisRegex.match(remainder);
        if (axis_match.hasMatch()) {
            const int key = axis_match.captured(1).toInt();
            const JoyControl control =
                axis_match.captured(2) == "+" ? JoyControl::AxisPlus : JoyControl::AxisMinus;
            return JoyInput(joy_id, control, static_cast<uint8_t>(key));
        }
        const QRegularExpressionMatch button_match = kButtonRegex.match(remainder);
        if (button_match.hasMatch()) {
            const int key = button_match.captured(1).toInt();
            return JoyInput(joy_id, JoyControl::Button, static_cast<uint8_t>(key));
        }
        const QRegularExpressionMatch hat_match = kHatRegex.match(remainder);
        if (hat_match.hasMatch()) {
            const int key = hat_match.captured(1).toInt();
            if (!hat_match.captured(3).isEmpty()) {
                return JoyInput(joy_id, JoyControl::HatNorth, static_cast<uint8_t>(key));
            } else if (!hat_match.captured(4).isEmpty()) {
                return JoyInput(joy_id, JoyControl::HatSouth, static_cast<uint8_t>(key));
            } else if (!hat_match.captured(5).isEmpty()) {
                return JoyInput(joy_id, JoyControl::HatEast, static_cast<uint8_t>(key));
            } else if (!hat_match.captured(6).isEmpty()) {
                return JoyInput(joy_id, JoyControl::HatWest, static_cast<uint8_t>(key));
            }
        }

        // Invalid.
        return UserInput();
    }

    // Not a joystick.

    // Non-ASCII (or unparseable) keyboard input are treated as a pair of
    // integers "key:mod".
    const QStringList pair = config::str_split(string, ":");
    if (pair.size() == 2) {
        bool key_ok = false, mod_ok = false;
        const int key = pair[0].toInt(&key_ok);
        const uint mod = pair[1].toUInt(&mod_ok);
        if (key_ok && mod_ok) {
            return KeyboardInput(key, static_cast<uint32_t>(mod));
        }
    }

    const QString upper = string.toUpper();

    const auto standalone = StandaloneModifiers().find(upper);
    if (standalone != StandaloneModifiers().end()) {
        // Stand-alone modifier key.
        return standalone->second;
    }

    // Strip modifier prefixes.
    uint32_t mod = kKeyModNone;
    QString remaining = upper;
    bool found = true;
    while (found) {
        found = false;
        for (const auto& prefix : ModPrefixes()) {
            if (remaining.startsWith(prefix.first)) {
                mod |= prefix.second;
                remaining = remaining.mid(prefix.first.length());
                found = true;
                break;
            }
        }
    }

    if (remaining.isEmpty()) {
        return UserInput();
    }

    // Single printable ASCII character.
    if (remaining.length() == 1) {
        const ushort c = remaining[0].unicode();
        if (c >= 0x20 && c < 0x7f) {
            return KeyboardInput(static_cast<int>(c), mod);
        }
        return UserInput();
    }

    // Named key.
    const auto named = KeyNameMap().find(remaining);
    if (named != KeyNameMap().end()) {
        return KeyboardInput(named->second, mod);
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

QString JoyId::ToConfigString() const {
    return QStringLiteral("Joy%1").arg(sdl_index_ + 1);
}

QString JoyId::ToLocalizedString() const {
    return Tr("Joystick %1").arg(sdl_index_ + 1);
}

QString JoyInput::ToConfigString() const {
    const QString joy_string = joy_.ToConfigString();
    switch (control_) {
        case JoyControl::AxisPlus:
            return QStringLiteral("%1-Axis%2+").arg(joy_string).arg(control_index_);
        case JoyControl::AxisMinus:
            return QStringLiteral("%1-Axis%2-").arg(joy_string).arg(control_index_);
        case JoyControl::Button:
            return QStringLiteral("%1-Button%2").arg(joy_string).arg(control_index_);
        case JoyControl::HatNorth:
            return QStringLiteral("%1-Hat%2N").arg(joy_string).arg(control_index_);
        case JoyControl::HatSouth:
            return QStringLiteral("%1-Hat%2S").arg(joy_string).arg(control_index_);
        case JoyControl::HatWest:
            return QStringLiteral("%1-Hat%2W").arg(joy_string).arg(control_index_);
        case JoyControl::HatEast:
            return QStringLiteral("%1-Hat%2E").arg(joy_string).arg(control_index_);
    }

    VBAM_NOTREACHED_RETURN(QString());
}

QString JoyInput::ToLocalizedString() const {
    const QString joy_string = joy_.ToLocalizedString();
    switch (control_) {
        case JoyControl::AxisPlus:
            return Tr("%1: Axis %2+").arg(joy_string).arg(control_index_);
        case JoyControl::AxisMinus:
            return Tr("%1: Axis %2-").arg(joy_string).arg(control_index_);
        case JoyControl::Button:
            return Tr("%1: Button %2").arg(joy_string).arg(control_index_);
        case JoyControl::HatNorth:
            return Tr("%1: Hat %2 North").arg(joy_string).arg(control_index_);
        case JoyControl::HatSouth:
            return Tr("%1: Hat %2 South").arg(joy_string).arg(control_index_);
        case JoyControl::HatWest:
            return Tr("%1: Hat %2 West").arg(joy_string).arg(control_index_);
        case JoyControl::HatEast:
            return Tr("%1: Hat %2 East").arg(joy_string).arg(control_index_);
    }

    VBAM_NOTREACHED_RETURN(QString());
}

QString KeyboardInput::ToConfigString() const {
    // Handle the modifier case separately.
    if (KeyIsModifier(key_)) {
        QString mod_str = ModToConfigString(mod_);
        if (!mod_str.isEmpty()) {
            mod_str.chop(1);
            return mod_str;
        }
        // Fallback for modifier keys without modifier flags.
        switch (key_) {
            case Qt::Key_Shift:
                return "SHIFT";
            case Qt::Key_Control:
                return "CTRL";
            case Qt::Key_Alt:
                return "ALT";
            case Qt::Key_Meta:
                return "META";
            default:
                return QString();
        }
    }

    if (key_ == ',' || key_ == ':') {
        // Special case for comma and colon to avoid parsing issues.
        return QStringLiteral("%1:%2").arg(key_).arg(mod_);
    }

    const QString key_name = KeyToConfigName(key_);
    if (key_name.isEmpty()) {
        // Unicode or unnamed key handling.
        return QStringLiteral("%1:%2").arg(key_).arg(mod_);
    }

    return ModToConfigString(mod_) + key_name;
}

QString KeyboardInput::ToLocalizedString() const {
    // Handle the modifier case separately.
    if (KeyIsModifier(key_)) {
        QString mod_str = ModToLocalizedString(mod_);
        if (!mod_str.isEmpty()) {
            mod_str.chop(1);
            return mod_str;
        }
        switch (key_) {
            case Qt::Key_Shift:
                return Tr("Shift");
            case Qt::Key_Control:
#if defined(__APPLE__)
                return Tr("Cmd");
#else
                return Tr("Ctrl");
#endif
            case Qt::Key_Alt:
                return Tr("Alt");
            case Qt::Key_Meta:
#if defined(__APPLE__)
                return Tr("Ctrl");
#else
                return Tr("Meta");
#endif
            default:
                return Tr("Key");
        }
    }

    return ModToLocalizedString(mod_) + KeyToLocalizedName(key_);
}

// static
std::unordered_set<UserInput> UserInput::FromConfigString(const QString& string) {
    std::unordered_set<UserInput> user_inputs;

    if (string.isEmpty()) {
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
QString UserInput::SpanToConfigString(const std::unordered_set<UserInput>& user_inputs) {
    QString config_string;
    if (user_inputs.empty()) {
        return config_string;
    }
    for (const UserInput& user_input : user_inputs) {
        config_string += user_input.ToConfigString() + ',';
    }
    config_string.chop(1);
    return config_string;
}

QString UserInput::ToConfigString() const {
    switch (device_) {
        case Device::Invalid:
            return QString();
        case Device::Keyboard:
            return keyboard_input().ToConfigString();
        case Device::Joystick:
            return joy_input().ToConfigString();
    }

    VBAM_NOTREACHED_RETURN(QString());
}

QString UserInput::ToLocalizedString() const {
    switch (device_) {
        case Device::Invalid:
            return QString();
        case Device::Keyboard:
            return keyboard_input().ToLocalizedString();
        case Device::Joystick:
            return joy_input().ToLocalizedString();
    }

    VBAM_NOTREACHED_RETURN(QString());
}

}  // namespace config
