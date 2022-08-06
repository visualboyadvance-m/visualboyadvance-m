#ifndef _WX_GAME_CONTROL_H_
#define _WX_GAME_CONTROL_H_

#include <array>
#include <map>
#include "nonstd/optional.hpp"
#include <set>
#include <wx/string.h>

#include "wx/userinput.h"

// Forward declaration.
class wxGameControlState;

// Represents an in-game input.
enum class wxGameKey {
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

inline constexpr int kNbGameKeys =
    static_cast<std::underlying_type<wxGameKey>::type>(wxGameKey::Last) + 1;
inline constexpr int kNbJoypads = 4;

inline constexpr std::array<wxGameKey, kNbGameKeys> kAllGameKeys  = {
    wxGameKey::Up,
    wxGameKey::Down,
    wxGameKey::Left,
    wxGameKey::Right,
    wxGameKey::A,
    wxGameKey::B,
    wxGameKey::L,
    wxGameKey::R,
    wxGameKey::Select,
    wxGameKey::Start,
    wxGameKey::MotionUp,
    wxGameKey::MotionDown,
    wxGameKey::MotionLeft,
    wxGameKey::MotionRight,
    wxGameKey::MotionIn,
    wxGameKey::MotionOut,
    wxGameKey::AutoA,
    wxGameKey::AutoB,
    wxGameKey::Speed,
    wxGameKey::Capture,
    wxGameKey::Gameshark,
};

// Conversion utility method. Returns empty string on failure.
// This is O(1).
wxString GameKeyToString(const wxGameKey& game_key);

// Conversion utility method. Returns std::nullopt on failure.
// This is O(log(kNbGameKeys)).
nonstd::optional<wxGameKey> StringToGameKey(const wxString& input);

// Abstraction for an in-game control, wich is made of a player index (from 0
// to 3), and a wxGameKey.
class wxGameControl {
public:
    // Converts a string to a wxGameControl. Returns std::nullopt on failure.
    static nonstd::optional<wxGameControl> FromString(const wxString& name);

    wxGameControl(int joypad, wxGameKey game_key);
    ~wxGameControl();

    wxString ToString() const { return config_string_; };

    bool operator==(const wxGameControl& other) const;
    bool operator!=(const wxGameControl& other) const;
    bool operator<(const wxGameControl& other) const;
    bool operator<=(const wxGameControl& other) const;
    bool operator>(const wxGameControl& other) const;
    bool operator>=(const wxGameControl& other) const;

private:
    const int joypad_;
    const wxGameKey game_key_;
    const wxString config_string_;

    friend class wxGameControlState;
};

// Tracks in-game input and computes the joypad value used to send control input
// data to the emulator.
class wxGameControlState {
public:
    // This is a global singleton.
    static wxGameControlState& Instance();

    // Disable copy constructor and assignment operator.
    wxGameControlState(const wxGameControlState&) = delete;
    wxGameControlState& operator=(const wxGameControlState&) = delete;

    // Processes `user_input` and updates the internal tracking state.
    // Returns true if `user_input` corresponds to a game input.
    bool OnInputPressed(const wxUserInput& user_input);
    bool OnInputReleased(const wxUserInput& user_input);

    // Clears all input.
    void Reset();

    // Recomputes internal bindinds. This is a potentially slow operation and
    // should only be called when the game input configuration has been changed.
    void OnGameBindingsChanged();

    uint32_t GetJoypad(int joypad) const;

private:
    wxGameControlState();
    ~wxGameControlState();

    std::map<wxUserInput, std::set<wxGameControl>> input_bindings_;
    std::map<wxGameControl, std::set<wxUserInput>> active_controls_;
    std::set<wxUserInput> keys_pressed_;
    std::array<uint32_t, kNbJoypads> joypads_;
};

#endif  // _WX_GAME_CONTROL_H_
