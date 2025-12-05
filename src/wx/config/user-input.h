#ifndef VBAM_WX_CONFIG_USER_INPUT_H_
#define VBAM_WX_CONFIG_USER_INPUT_H_

#include <cstdint>
#include <unordered_set>

#include "variant.hpp"

#include <wx/string.h>

#include "core/base/check.h"

namespace config {

// Extended modifier flags to distinguish left/right modifier keys.
// These are used in addition to wxKeyModifier to provide more precise
// keyboard input handling via SDL. The values are chosen to not conflict
// with wxKeyModifier values (which use bits 0-4).
enum KeyModFlag : uint32_t {
    kKeyModNone = 0,
    // Standard modifiers (matching wxKeyModifier for compatibility)
    kKeyModAlt = 0x0001,      // wxMOD_ALT
    kKeyModControl = 0x0002,  // wxMOD_CONTROL
    kKeyModShift = 0x0004,    // wxMOD_SHIFT
    kKeyModMeta = 0x0008,     // wxMOD_META
    // Extended modifiers for left/right distinction (use higher bits)
    kKeyModLeftShift = 0x0100,
    kKeyModRightShift = 0x0200,
    kKeyModLeftControl = 0x0400,
    kKeyModRightControl = 0x0800,
    kKeyModLeftAlt = 0x1000,
    kKeyModRightAlt = 0x2000,
    kKeyModLeftMeta = 0x4000,
    kKeyModRightMeta = 0x8000,
};

// Helper to check if extended modifiers are present (left/right specific)
constexpr bool HasExtendedModifiers(uint32_t mod) {
    return (mod & 0xFF00) != 0;
}

// Convert extended modifiers to base wxKeyModifier (for compatibility)
constexpr wxKeyModifier ToWxKeyModifier(uint32_t mod) {
    int result = wxMOD_NONE;
    if (mod & (kKeyModAlt | kKeyModLeftAlt | kKeyModRightAlt))
        result |= wxMOD_ALT;
    if (mod & (kKeyModControl | kKeyModLeftControl | kKeyModRightControl))
        result |= wxMOD_CONTROL;
    if (mod & (kKeyModShift | kKeyModLeftShift | kKeyModRightShift))
        result |= wxMOD_SHIFT;
    if (mod & (kKeyModMeta | kKeyModLeftMeta | kKeyModRightMeta))
        result |= wxMOD_META;
    return static_cast<wxKeyModifier>(result);
}

// Check if two modifier values match, considering that extended modifiers (L/R)
// should match their base counterparts. For example:
// - kKeyModLeftControl matches wxMOD_CONTROL or kKeyModControl
// - kKeyModLeftControl does NOT match kKeyModRightControl
// - wxMOD_CONTROL matches kKeyModLeftControl, kKeyModRightControl, or kKeyModControl
constexpr bool ModifiersMatch(uint32_t mod1, uint32_t mod2) {
    if (mod1 == mod2) return true;

    // Check each modifier type
    // Alt
    bool alt1_base = (mod1 & (kKeyModAlt | wxMOD_ALT)) != 0;
    bool alt1_left = (mod1 & kKeyModLeftAlt) != 0;
    bool alt1_right = (mod1 & kKeyModRightAlt) != 0;
    bool alt2_base = (mod2 & (kKeyModAlt | wxMOD_ALT)) != 0;
    bool alt2_left = (mod2 & kKeyModLeftAlt) != 0;
    bool alt2_right = (mod2 & kKeyModRightAlt) != 0;

    bool alt1_any = alt1_base || alt1_left || alt1_right;
    bool alt2_any = alt2_base || alt2_left || alt2_right;

    // If one has alt and the other doesn't, no match
    if (alt1_any != alt2_any) return false;
    // If both have specific L/R and they differ, no match
    if ((alt1_left || alt1_right) && (alt2_left || alt2_right)) {
        if (alt1_left != alt2_left || alt1_right != alt2_right) return false;
    }

    // Control
    bool ctrl1_base = (mod1 & (kKeyModControl | wxMOD_CONTROL)) != 0;
    bool ctrl1_left = (mod1 & kKeyModLeftControl) != 0;
    bool ctrl1_right = (mod1 & kKeyModRightControl) != 0;
    bool ctrl2_base = (mod2 & (kKeyModControl | wxMOD_CONTROL)) != 0;
    bool ctrl2_left = (mod2 & kKeyModLeftControl) != 0;
    bool ctrl2_right = (mod2 & kKeyModRightControl) != 0;

    bool ctrl1_any = ctrl1_base || ctrl1_left || ctrl1_right;
    bool ctrl2_any = ctrl2_base || ctrl2_left || ctrl2_right;

    if (ctrl1_any != ctrl2_any) return false;
    if ((ctrl1_left || ctrl1_right) && (ctrl2_left || ctrl2_right)) {
        if (ctrl1_left != ctrl2_left || ctrl1_right != ctrl2_right) return false;
    }

    // Shift
    bool shift1_base = (mod1 & (kKeyModShift | wxMOD_SHIFT)) != 0;
    bool shift1_left = (mod1 & kKeyModLeftShift) != 0;
    bool shift1_right = (mod1 & kKeyModRightShift) != 0;
    bool shift2_base = (mod2 & (kKeyModShift | wxMOD_SHIFT)) != 0;
    bool shift2_left = (mod2 & kKeyModLeftShift) != 0;
    bool shift2_right = (mod2 & kKeyModRightShift) != 0;

    bool shift1_any = shift1_base || shift1_left || shift1_right;
    bool shift2_any = shift2_base || shift2_left || shift2_right;

    if (shift1_any != shift2_any) return false;
    if ((shift1_left || shift1_right) && (shift2_left || shift2_right)) {
        if (shift1_left != shift2_left || shift1_right != shift2_right) return false;
    }

    // Meta
    bool meta1_base = (mod1 & (kKeyModMeta | wxMOD_META)) != 0;
    bool meta1_left = (mod1 & kKeyModLeftMeta) != 0;
    bool meta1_right = (mod1 & kKeyModRightMeta) != 0;
    bool meta2_base = (mod2 & (kKeyModMeta | wxMOD_META)) != 0;
    bool meta2_left = (mod2 & kKeyModLeftMeta) != 0;
    bool meta2_right = (mod2 & kKeyModRightMeta) != 0;

    bool meta1_any = meta1_base || meta1_left || meta1_right;
    bool meta2_any = meta2_base || meta2_left || meta2_right;

    if (meta1_any != meta2_any) return false;
    if ((meta1_left || meta1_right) && (meta2_left || meta2_right)) {
        if (meta1_left != meta2_left || meta1_right != meta2_right) return false;
    }

    return true;
}

// Normalize modifiers to a canonical form for hashing.
// Extended modifiers are converted to base modifiers unless both L and R are set.
constexpr uint32_t NormalizeModifiersForHash(uint32_t mod) {
    uint32_t result = 0;
    if (mod & (kKeyModAlt | kKeyModLeftAlt | kKeyModRightAlt | wxMOD_ALT))
        result |= wxMOD_ALT;
    if (mod & (kKeyModControl | kKeyModLeftControl | kKeyModRightControl | wxMOD_CONTROL))
        result |= wxMOD_CONTROL;
    if (mod & (kKeyModShift | kKeyModLeftShift | kKeyModRightShift | wxMOD_SHIFT))
        result |= wxMOD_SHIFT;
    if (mod & (kKeyModMeta | kKeyModLeftMeta | kKeyModRightMeta | wxMOD_META))
        result |= wxMOD_META;
    return result;
}

// Abstract representation of a keyboard input. This class is used to represent
// a key press or release event. It is used in the configuration system to
// represent a key binding.
class KeyboardInput final {
public:
    // Constructor with wxKeyModifier (legacy compatibility)
    constexpr explicit KeyboardInput(wxKeyCode key, wxKeyModifier mod = wxMOD_NONE)
        : key_(key), mod_(static_cast<uint32_t>(mod)) {}
    constexpr explicit KeyboardInput(char c, wxKeyModifier mod = wxMOD_NONE)
        : key_(static_cast<wxKeyCode>(c)), mod_(static_cast<uint32_t>(mod)) {}

    // Constructor with extended modifiers (KeyModFlag)
    constexpr explicit KeyboardInput(wxKeyCode key, uint32_t extended_mod)
        : key_(key), mod_(extended_mod) {}

    ~KeyboardInput() = default;

    constexpr wxKeyCode key() const { return key_; }

    // Returns wxKeyModifier for compatibility (loses L/R distinction)
    constexpr wxKeyModifier mod() const { return ToWxKeyModifier(mod_); }

    // Returns the full extended modifier flags (preserves L/R distinction)
    constexpr uint32_t mod_extended() const { return mod_; }

    // Check if this input uses extended (L/R specific) modifiers
    constexpr bool has_extended_modifiers() const { return HasExtendedModifiers(mod_); }

    wxString ToConfigString() const;
    wxString ToLocalizedString() const;

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
    const wxKeyCode key_;
    const uint32_t mod_;  // Extended modifier flags (KeyModFlag)
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

    wxString ToConfigString() const;
    wxString ToLocalizedString() const;

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

// Abstraction for a joystick input. This class is used to represent a joystick
// control press or release event. It is used in the configuration system to
// represent a joystick binding.
class JoyInput final {
public:
    constexpr JoyInput(JoyId joy, JoyControl control, uint8_t control_index)
        : joy_(joy), control_(control), control_index_(control_index) {}
    ~JoyInput() = default;

    constexpr JoyId joy() const { return joy_; }
    constexpr JoyControl control() const { return control_; }
    constexpr uint8_t control_index() const { return control_index_; }

    wxString ToConfigString() const;
    wxString ToLocalizedString() const;

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
// TODO: Most of these methods should be constexpr but nonstd::variant is not.
class UserInput {
public:
    // The device type for a user control.
    enum class Device { Invalid = 0, Keyboard, Joystick, Last = Joystick };

    // Constructor from a configuration string. Returns empty set on failure.
    static std::unordered_set<UserInput> FromConfigString(const wxString& string);

    // Converts a set of UserInput into a configuration string. This
    // recomputes the configuration string every time and should not be used
    // for comparison purposes.
    // TODO: Replace std::unordered_set with std::span when the code base uses C++20.
    static wxString SpanToConfigString(const std::unordered_set<UserInput>& user_inputs);

    // Invalid UserInput, mainly used for comparison.
    UserInput() : device_(Device::Invalid), input_(nonstd::monostate{}) {}

    // Constructors for joystick and keyboard inputs. This object can be
    // implicitly constructed from JoyInput and KeyboardInput.
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

    // Converts to a configuration string for saving.
    wxString ToConfigString() const;

    // Converts to a localized string for display.
    wxString ToLocalizedString() const;

    // Comparison operators.
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
        // Use normalized modifiers for hashing so that equivalent inputs hash the same
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

#endif  // VBAM_WX_CONFIG_USER_INPUT_H_
