#ifndef VBAM_QT_CONFIG_USER_INPUT_H_
#define VBAM_QT_CONFIG_USER_INPUT_H_

#include <cstdint>
#include <unordered_set>

#include "variant.hpp"

#include <QString>

#include "core/base/check.h"

namespace config {

// Modifier flags for keyboard inputs. The low nibble holds the generic
// modifiers, the high byte the left/right specific ones. These are our own
// values (not Qt::KeyboardModifier, which uses the high bits of an int) so
// they can be stored compactly and compared with ModifiersMatch() below.
enum KeyModFlag : uint32_t {
    kKeyModNone = 0,
    // Generic modifiers.
    kKeyModAlt = 0x0001,
    kKeyModControl = 0x0002,  // Ctrl (Command on macOS, like Qt::ControlModifier)
    kKeyModShift = 0x0004,
    kKeyModMeta = 0x0008,  // Meta (the actual Control key on macOS)
    // Extended modifiers for left/right distinction.
    kKeyModLeftShift = 0x0100,
    kKeyModRightShift = 0x0200,
    kKeyModLeftControl = 0x0400,
    kKeyModRightControl = 0x0800,
    kKeyModLeftAlt = 0x1000,
    kKeyModRightAlt = 0x2000,
    kKeyModLeftMeta = 0x4000,
    kKeyModRightMeta = 0x8000,
};

// Helper to check if extended modifiers are present (left/right specific).
constexpr bool HasExtendedModifiers(uint32_t mod) {
    return (mod & 0xFF00) != 0;
}

// Collapses extended (L/R) modifiers to their generic counterparts.
constexpr uint32_t ToGenericModifiers(uint32_t mod) {
    uint32_t result = kKeyModNone;
    if (mod & (kKeyModAlt | kKeyModLeftAlt | kKeyModRightAlt))
        result |= kKeyModAlt;
    if (mod & (kKeyModControl | kKeyModLeftControl | kKeyModRightControl))
        result |= kKeyModControl;
    if (mod & (kKeyModShift | kKeyModLeftShift | kKeyModRightShift))
        result |= kKeyModShift;
    if (mod & (kKeyModMeta | kKeyModLeftMeta | kKeyModRightMeta))
        result |= kKeyModMeta;
    return result;
}

// Check if two modifier values match, considering that extended modifiers
// (L/R) match their generic counterparts:
// - kKeyModLeftControl matches kKeyModControl
// - kKeyModLeftControl does NOT match kKeyModRightControl
// - kKeyModControl matches kKeyModLeftControl, kKeyModRightControl or
//   kKeyModControl
constexpr bool ModifiersMatchOne(uint32_t mod1,
                                 uint32_t mod2,
                                 uint32_t generic,
                                 uint32_t left,
                                 uint32_t right) {
    const bool any1 = (mod1 & (generic | left | right)) != 0;
    const bool any2 = (mod2 & (generic | left | right)) != 0;
    if (any1 != any2)
        return false;
    const bool lr1 = (mod1 & (left | right)) != 0;
    const bool lr2 = (mod2 & (left | right)) != 0;
    if (lr1 && lr2) {
        return ((mod1 & left) != 0) == ((mod2 & left) != 0) &&
               ((mod1 & right) != 0) == ((mod2 & right) != 0);
    }
    return true;
}

constexpr bool ModifiersMatch(uint32_t mod1, uint32_t mod2) {
    if (mod1 == mod2)
        return true;
    return ModifiersMatchOne(mod1, mod2, kKeyModAlt, kKeyModLeftAlt, kKeyModRightAlt) &&
           ModifiersMatchOne(mod1, mod2, kKeyModControl, kKeyModLeftControl,
                             kKeyModRightControl) &&
           ModifiersMatchOne(mod1, mod2, kKeyModShift, kKeyModLeftShift, kKeyModRightShift) &&
           ModifiersMatchOne(mod1, mod2, kKeyModMeta, kKeyModLeftMeta, kKeyModRightMeta);
}

// Normalize modifiers to a canonical form for hashing.
constexpr uint32_t NormalizeModifiersForHash(uint32_t mod) {
    return ToGenericModifiers(mod);
}

// Abstract representation of a keyboard input: a Qt key code (Qt::Key, stored
// as an int so this header needs no Qt GUI include) plus KeyModFlag modifiers.
// Used in the configuration system to represent a key binding.
class KeyboardInput final {
public:
    constexpr explicit KeyboardInput(int key, uint32_t mod = kKeyModNone)
        : key_(key), mod_(mod) {}
    ~KeyboardInput() = default;

    // The Qt::Key value.
    constexpr int key() const { return key_; }

    // Returns the generic modifiers (loses L/R distinction).
    constexpr uint32_t mod() const { return ToGenericModifiers(mod_); }

    // Returns the full extended modifier flags (preserves L/R distinction).
    constexpr uint32_t mod_extended() const { return mod_; }

    constexpr bool has_extended_modifiers() const { return HasExtendedModifiers(mod_); }

    // Configuration string, e.g. "CTRL+O", "SHIFT+F1", "LCTRL+A", "F12", "a".
    QString ToConfigString() const;
    // Localized display string, e.g. "Ctrl+O".
    QString ToLocalizedString() const;

    bool operator==(const KeyboardInput& other) const {
        return key_ == other.key_ && ModifiersMatch(mod_, other.mod_);
    }
    bool operator!=(const KeyboardInput& other) const { return !(*this == other); }
    bool operator<(const KeyboardInput& other) const {
        if (key_ == other.key_) {
            return mod_ < other.mod_;
        } else {
            return key_ < other.key_;
        }
    }
    bool operator<=(const KeyboardInput& other) const { return *this < other || *this == other; }
    bool operator>(const KeyboardInput& other) const { return !(*this <= other); }
    bool operator>=(const KeyboardInput& other) const { return !(*this < other); }

private:
    const int key_;
    const uint32_t mod_;  // KeyModFlag bits
};

// One of the possible joystick controls.
enum class JoyControl {
    AxisPlus = 0,
    AxisMinus,
    Button,
    HatNorth,
    HatSouth,
    HatWest,
    HatEast,
    Last = HatEast
};

// Abstraction for a single joystick. In the current implementation, this
// encapsulates an `sdl_index_`.
class JoyId final {
public:
    static JoyId Invalid();

    constexpr explicit JoyId(int sdl_index) : sdl_index_(sdl_index){};
    ~JoyId() = default;

    QString ToConfigString() const;
    QString ToLocalizedString() const;

    constexpr bool operator==(const JoyId& other) const { return sdl_index_ == other.sdl_index_; }
    constexpr bool operator!=(const JoyId& other) const { return sdl_index_ != other.sdl_index_; }
    constexpr bool operator<(const JoyId& other) const { return sdl_index_ < other.sdl_index_; }
    constexpr bool operator<=(const JoyId& other) const { return sdl_index_ <= other.sdl_index_; }
    constexpr bool operator>(const JoyId& other) const { return sdl_index_ > other.sdl_index_; }
    constexpr bool operator>=(const JoyId& other) const { return sdl_index_ >= other.sdl_index_; }

private:
    JoyId() = delete;

    const int sdl_index_;

    friend struct std::hash<config::JoyId>;
};

// Abstraction for a joystick input.
class JoyInput final {
public:
    constexpr JoyInput(JoyId joy, JoyControl control, uint8_t control_index)
        : joy_(joy), control_(control), control_index_(control_index) {}
    ~JoyInput() = default;

    constexpr JoyId joy() const { return joy_; }
    constexpr JoyControl control() const { return control_; }
    constexpr uint8_t control_index() const { return control_index_; }

    QString ToConfigString() const;
    QString ToLocalizedString() const;

    constexpr bool operator==(const JoyInput& other) const {
        return joy_ == other.joy_ && control_ == other.control_ &&
               control_index_ == other.control_index_;
    }
    constexpr bool operator!=(const JoyInput& other) const { return !(*this == other); }
    constexpr bool operator<(const JoyInput& other) const {
        if (joy_ == other.joy_) {
            if (control_ == other.control_) {
                return control_index_ < other.control_index_;
            } else {
                return control_ < other.control_;
            }
        } else {
            return joy_ < other.joy_;
        }
    }
    constexpr bool operator<=(const JoyInput& other) const {
        return *this < other || *this == other;
    }
    constexpr bool operator>(const JoyInput& other) const { return !(*this <= other); }
    constexpr bool operator>=(const JoyInput& other) const { return !(*this < other); }

private:
    const JoyId joy_;
    const JoyControl control_;
    const uint8_t control_index_;
};

// Abstraction for a user input, which can come from a keyboard or a joystick.
class UserInput {
public:
    enum class Device { Invalid = 0, Keyboard, Joystick, Last = Joystick };

    // Constructor from a configuration string ("CTRL+O,Joy1/Button0"). Returns
    // an empty set on failure.
    static std::unordered_set<UserInput> FromConfigString(const QString& string);

    // Converts a set of UserInput into a configuration string.
    static QString SpanToConfigString(const std::unordered_set<UserInput>& user_inputs);

    UserInput() : device_(Device::Invalid), input_(nonstd::monostate{}) {}
    UserInput(JoyInput joy_input) : device_(Device::Joystick), input_(joy_input) {}
    UserInput(KeyboardInput keyboard_input)
        : device_(Device::Keyboard), input_(keyboard_input) {}

    Device device() const { return device_; }

    const KeyboardInput& keyboard_input() const {
        VBAM_CHECK(is_keyboard());
        return nonstd::get<KeyboardInput>(input_);
    };

    const JoyInput& joy_input() const {
        VBAM_CHECK(is_joystick());
        return nonstd::get<JoyInput>(input_);
    };

    bool is_valid() const { return device_ != Device::Invalid; }
    operator bool() const { return is_valid(); }

    bool is_keyboard() const { return device_ == Device::Keyboard; }
    bool is_joystick() const { return device_ == Device::Joystick; }

    QString ToConfigString() const;
    QString ToLocalizedString() const;

    bool operator==(const UserInput& other) const {
        return device_ == other.device_ && input_ == other.input_;
    }
    bool operator!=(const UserInput& other) const { return !(*this == other); }
    bool operator<(const UserInput& other) const {
        if (device_ == other.device_) {
            return input_ < other.input_;
        } else {
            return device_ < other.device_;
        }
    }
    bool operator<=(const UserInput& other) const {
        return *this < other || *this == other;
    }
    bool operator>(const UserInput& other) const { return !(*this <= other); }
    bool operator>=(const UserInput& other) const { return !(*this < other); }

private:
    const Device device_;
    const nonstd::variant<nonstd::monostate, JoyInput, KeyboardInput> input_;
};

}  // namespace config

// Specializations for hash functions for all of the above classes.
template <>
struct std::hash<config::JoyId> {
    std::size_t operator()(const config::JoyId& joy_id) const noexcept {
        return std::hash<int>{}(joy_id.sdl_index_);
    }
};

template <>
struct std::hash<config::JoyInput> {
    std::size_t operator()(const config::JoyInput& joy_input) const noexcept {
        const std::size_t hash1 = std::hash<config::JoyId>{}(joy_input.joy());
        const std::size_t hash2 = std::hash<int>{}(static_cast<int>(joy_input.control()));
        const std::size_t hash3 = std::hash<int>{}(joy_input.control_index());
        return hash1 ^ hash2 ^ hash3;
    }
};

template <>
struct std::hash<config::KeyboardInput> {
    std::size_t operator()(const config::KeyboardInput& keyboard_input) const noexcept {
        const std::size_t hash1 = std::hash<int>{}(keyboard_input.key());
        const std::size_t hash2 =
            std::hash<uint32_t>{}(config::NormalizeModifiersForHash(keyboard_input.mod_extended()));
        return hash1 ^ hash2;
    }
};

template <>
struct std::hash<config::UserInput> {
    std::size_t operator()(const config::UserInput& user_input) const noexcept {
        const std::size_t device_hash = std::hash<int>{}(static_cast<int>(user_input.device()));
        switch (user_input.device()) {
            case config::UserInput::Device::Invalid:
                return device_hash;
            case config::UserInput::Device::Joystick:
                return device_hash ^ std::hash<config::JoyInput>{}(user_input.joy_input());
            case config::UserInput::Device::Keyboard:
                return device_hash ^
                       std::hash<config::KeyboardInput>{}(user_input.keyboard_input());
        }

        VBAM_NOTREACHED_RETURN(0);
    }
};

#endif  // VBAM_QT_CONFIG_USER_INPUT_H_
