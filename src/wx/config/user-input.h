#ifndef VBAM_WX_CONFIG_USER_INPUT_H_
#define VBAM_WX_CONFIG_USER_INPUT_H_

#include <cstdint>
#include <unordered_set>

#include "variant.hpp"

#include <wx/string.h>

#include "core/base/check.h"

namespace config {

// Abstract representation of a keyboard input. This class is used to represent
// a key press or release event. It is used in the configuration system to
// represent a key binding.
class KeyboardInput final {
public:
    constexpr explicit KeyboardInput(wxKeyCode key, wxKeyModifier mod = wxMOD_NONE)
        : key_(key), mod_(mod) {}
    constexpr explicit KeyboardInput(char c, wxKeyModifier mod = wxMOD_NONE)
        : key_(static_cast<wxKeyCode>(c)), mod_(mod) {}

    ~KeyboardInput() = default;

    constexpr wxKeyCode key() const { return key_; }
    constexpr wxKeyModifier mod() const { return mod_; }

    wxString ToConfigString() const;
    wxString ToLocalizedString() const;

    bool operator==(const KeyboardInput& other) const {
        return key_ == other.key_ && mod_ == other.mod_;
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
    const wxKeyModifier mod_;
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
        const std::size_t hash2 = std::hash<int>{}(keyboard_input.mod());
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

        VBAM_NOTREACHED();
        return 0;
    }
};

#endif  // VBAM_WX_CONFIG_USER_INPUT_H_
