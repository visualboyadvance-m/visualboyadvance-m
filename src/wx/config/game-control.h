#ifndef VBAM_WX_CONFIG_GAME_CONTROL_H_
#define VBAM_WX_CONFIG_GAME_CONTROL_H_

#include <array>
#include <map>
#include <set>

#include <optional.hpp>

#include <wx/string.h>

#include "config/user-input.h"

namespace config {

// Forward declaration.
class GameControlState;

//clang-format off
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

inline constexpr int kNbGameKeys = static_cast<size_t>(GameKey::Last) + 1;
inline constexpr int kNbJoypads = 4;

inline constexpr std::array<GameKey, kNbGameKeys> kAllGameKeys  = {
    GameKey::Up,
    GameKey::Down,
    GameKey::Left,
    GameKey::Right,
    GameKey::A,
    GameKey::B,
    GameKey::L,
    GameKey::R,
    GameKey::Select,
    GameKey::Start,
    GameKey::MotionUp,
    GameKey::MotionDown,
    GameKey::MotionLeft,
    GameKey::MotionRight,
    GameKey::MotionIn,
    GameKey::MotionOut,
    GameKey::AutoA,
    GameKey::AutoB,
    GameKey::Speed,
    GameKey::Capture,
    GameKey::Gameshark,
};
//clang-format on

// Conversion utility method. Returns empty string on failure.
// This is O(1).
wxString GameKeyToString(const GameKey& game_key);

// Conversion utility method. Returns std::nullopt on failure.
// This is O(log(kNbGameKeys)).
nonstd::optional<GameKey> StringToGameKey(const wxString& input);

// Abstraction for an in-game control, wich is made of a player index (from 0
// to 3), and a GameKey.
class GameControl {
public:
    // Converts a string to a GameControl. Returns std::nullopt on failure.
    static nonstd::optional<GameControl> FromString(const wxString& name);

    GameControl(int joypad, GameKey game_key);
    ~GameControl();

    wxString ToString() const { return config_string_; };

    bool operator==(const GameControl& other) const;
    bool operator!=(const GameControl& other) const;
    bool operator<(const GameControl& other) const;
    bool operator<=(const GameControl& other) const;
    bool operator>(const GameControl& other) const;
    bool operator>=(const GameControl& other) const;

private:
    const int joypad_;
    const GameKey game_key_;
    const wxString config_string_;

    friend class GameControlState;
};

// Tracks in-game input and computes the joypad value used to send control input
// data to the emulator.
class GameControlState {
public:
    // This is a global singleton.
    static GameControlState& Instance();

    // Disable copy constructor and assignment operator.
    GameControlState(const GameControlState&) = delete;
    GameControlState& operator=(const GameControlState&) = delete;

    // Processes `user_input` and updates the internal tracking state.
    // Returns true if `user_input` corresponds to a game input.
    bool OnInputPressed(const config::UserInput& user_input);
    bool OnInputReleased(const config::UserInput& user_input);

    // Clears all input.
    void Reset();

    // Recomputes internal bindinds. This is a potentially slow operation and
    // should only be called when the game input configuration has been changed.
    void OnGameBindingsChanged();

    uint32_t GetJoypad(int joypad) const;

private:
    GameControlState();
    ~GameControlState();

    std::map<config::UserInput, std::set<GameControl>> input_bindings_;
    std::map<GameControl, std::set<config::UserInput>> active_controls_;
    std::set<config::UserInput> keys_pressed_;
    std::array<uint32_t, kNbJoypads> joypads_;
};

}  // namespace config

#endif  // VBAM_WX_CONFIG_GAME_CONTROL_H_
