#ifndef VBAM_WX_CONFIG_COMMAND_H_
#define VBAM_WX_CONFIG_COMMAND_H_

#include <array>
#include <functional>

#include <optional.hpp>
#include <variant.hpp>

#include <wx/string.h>

#include "core/base/check.h"

namespace config {

// clang-format off
// Represents an in-game input.
enum class GameKey {
    Up = 0,
    Down,
    Left,
    Right,
    A,
    B,
    L,
    R,
    Select,
    Start,
    MotionUp,
    MotionDown,
    MotionLeft,
    MotionRight,
    MotionIn,
    MotionOut,
    AutoA,
    AutoB,
    Speed,
    Capture,
    Gameshark,
    Last = Gameshark
};
static constexpr size_t kNbGameKeys = static_cast<size_t>(GameKey::Last) + 1;

static constexpr std::array<GameKey, config::kNbGameKeys> kAllGameKeys = {
    GameKey::Up,         GameKey::Down,        GameKey::Left,
    GameKey::Right,      GameKey::A,           GameKey::B,
    GameKey::L,          GameKey::R,           GameKey::Select,
    GameKey::Start,      GameKey::MotionUp,    GameKey::MotionDown,
    GameKey::MotionLeft, GameKey::MotionRight, GameKey::MotionIn,
    GameKey::MotionOut,  GameKey::AutoA,       GameKey::AutoB,
    GameKey::Speed,      GameKey::Capture,     GameKey::Gameshark,
};
// clang-format on

static constexpr size_t kNbJoypads = 4;

// Represents an emulated joypad. The internal index is zero-based.
class GameJoy {
public:
    constexpr explicit GameJoy(size_t index) : index_(index) { VBAM_CHECK(index < kNbJoypads); }

    // The underlying zero-based index for this emulated joypad.
    constexpr size_t index() const { return index_; }

    // For display and INI purposes, the index is one-based.
    constexpr size_t ux_index() const { return index_ + 1; }

    constexpr bool operator==(const GameJoy& other) const { return index_ == other.index_; }
    constexpr bool operator!=(const GameJoy& other) const { return !(*this == other); }
    constexpr bool operator<(const GameJoy& other) const { return index_ < other.index_; }
    constexpr bool operator<=(const GameJoy& other) const {
        return *this < other || *this == other;
    }
    constexpr bool operator>(const GameJoy& other) const { return !(*this <= other); }
    constexpr bool operator>=(const GameJoy& other) const { return !(*this < other); }

private:
    const size_t index_;
};

static constexpr std::array<GameJoy, config::kNbJoypads> kAllGameJoys = {
    GameJoy(0),
    GameJoy(1),
    GameJoy(2),
    GameJoy(3),
};

// A Game Command is represented by a `joypad` number (1 to 4) and a
// `game_key`. `joypad` MUST be in the 1-4 range. Debug checks will assert
// otherwise.
class GameCommand final {
public:
    constexpr GameCommand(GameJoy joypad, GameKey game_key)
        : joypad_(joypad), game_key_(game_key) {}

    constexpr GameJoy joypad() const { return joypad_; }
    constexpr GameKey game_key() const { return game_key_; }
    wxString ToConfigString() const;
    wxString ToUXString() const;

    constexpr bool operator==(const GameCommand& other) const {
        return joypad_ == other.joypad_ && game_key_ == other.game_key_;
    }
    constexpr bool operator!=(const GameCommand& other) const { return !(*this == other); }
    constexpr bool operator<(const GameCommand& other) const {
        if (joypad_ == other.joypad_) {
            return game_key_ < other.game_key_;
        } else {
            return joypad_ < other.joypad_;
        }
    }
    constexpr bool operator<=(const GameCommand& other) const {
        return *this < other || *this == other;
    }
    constexpr bool operator>(const GameCommand& other) const { return !(*this <= other); }
    constexpr bool operator>=(const GameCommand& other) const { return !(*this < other); }

private:
    const GameJoy joypad_;
    const GameKey game_key_;
};

// A Shortcut Command is represented by the wx command ID.
class ShortcutCommand final {
public:
    constexpr explicit ShortcutCommand(int id) : id_(id) {}

    constexpr int id() const { return id_; }
    wxString ToConfigString() const;

    constexpr bool operator==(const ShortcutCommand& other) const { return id_ == other.id_; }
    constexpr bool operator!=(const ShortcutCommand& other) const { return !(*this == other); }
    constexpr bool operator<(const ShortcutCommand& other) const { return id_ < other.id_; }
    constexpr bool operator<=(const ShortcutCommand& other) const {
        return *this < other || *this == other;
    }
    constexpr bool operator>(const ShortcutCommand& other) const { return !(*this <= other); }
    constexpr bool operator>=(const ShortcutCommand& other) const { return !(*this < other); }

private:
    const int id_;
};

// Conversion utility method. Returns empty string on failure.
// This is O(1).
wxString GameKeyToString(const GameKey& game_key);

// Conversion utility method. Returns std::nullopt on failure.
// This is O(log(kNbGameKeys)).
nonstd::optional<GameKey> StringToGameKey(const wxString& input);

// Represents a Command for the emulator software. This can be either a Game
// Command (a button is pressed on the emulated device) or a Shortcut Command
// (a user-specified shortcut is activated).
// This is mainly used by the Bindings class to handle assignment in a common
// manner and prevent the user from assigning the same input to multiple
// Commands, Game or Shortcut.
class Command final {
public:
    enum class Tag {
        kGame = 0,
        kShortcut,
    };

    // Converts a string to a Command. Returns std::nullopt on failure.
    static nonstd::optional<Command> FromString(const wxString& name);

    // Game Command constructor.
    Command(GameCommand game_control) : tag_(Tag::kGame), control_(game_control) {}

    // Shortcut Command constructors.
    Command(ShortcutCommand shortcut_control) : tag_(Tag::kShortcut), control_(shortcut_control) {}

    ~Command() = default;

    // Returns the type of the value stored by the current object.
    Tag tag() const { return tag_; }

    bool is_game() const { return tag() == Tag::kGame; }
    bool is_shortcut() const { return tag() == Tag::kShortcut; }

    const GameCommand& game() const {
        VBAM_CHECK(is_game());
        return nonstd::get<GameCommand>(control_);
    }

    const ShortcutCommand& shortcut() const {
        VBAM_CHECK(is_shortcut());
        return nonstd::get<ShortcutCommand>(control_);
    }

    bool operator==(const Command& other) const {
        return tag_ == other.tag_ && control_ == other.control_;
    }
    bool operator!=(const Command& other) const { return !(*this == other); }
    bool operator<(const Command& other) const {
        if (tag_ == other.tag_) {
            switch (tag_) {
                case Tag::kGame:
                    return game() < other.game();
                case Tag::kShortcut:
                    return shortcut() < other.shortcut();
            }

            VBAM_NOTREACHED_RETURN(false);
        } else {
            return tag_ < other.tag_;
        }
    }
    bool operator<=(const Command& other) const { return *this < other || *this == other; }
    bool operator>(const Command& other) const { return !(*this <= other); }
    bool operator>=(const Command& other) const { return !(*this < other); }

private:
    const Tag tag_;
    const nonstd::variant<GameCommand, ShortcutCommand> control_;
};

}  // namespace config

// Specializations for hash functions for all of the above classes.
template <>
struct std::hash<config::GameKey> {
    std::size_t operator()(const config::GameKey& game_key) const noexcept {
        return std::hash<size_t>{}(static_cast<size_t>(game_key));
    }
};

template <>
struct std::hash<config::GameJoy> {
    std::size_t operator()(const config::GameJoy& game_joy) const noexcept {
        return std::hash<size_t>{}(static_cast<size_t>(game_joy.index()));
    }
};

template <>
struct std::hash<config::GameCommand> {
    std::size_t operator()(const config::GameCommand& game_control) const noexcept {
        const std::size_t hash1 = std::hash<config::GameJoy>{}(game_control.joypad());
        const std::size_t hash2 = std::hash<config::GameKey>{}(game_control.game_key());
        return hash1 ^ hash2;
    }
};

template <>
struct std::hash<config::ShortcutCommand> {
    std::size_t operator()(const config::ShortcutCommand& shortcut) const noexcept {
        return std::hash<int>{}(shortcut.id());
    }
};

template <>
struct std::hash<config::Command> {
    std::size_t operator()(const config::Command& control) const noexcept {
        switch (control.tag()) {
            case config::Command::Tag::kGame:
                return std::hash<config::GameCommand>{}(control.game());
            case config::Command::Tag::kShortcut:
                return std::hash<config::ShortcutCommand>{}(control.shortcut());
        }

        VBAM_NOTREACHED_RETURN(0);
    }
};

#endif  // VBAM_WX_CONFIG_COMMAND_H_
