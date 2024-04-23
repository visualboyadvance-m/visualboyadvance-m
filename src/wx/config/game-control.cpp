#include "wx/config/game-control.h"

#include "wx/opts.h"
#include "wx/strutils.h"
#include "wx/wxlogdebug.h"

namespace config {

namespace {

constexpr uint32_t kBitKeyA = (1 << 0);
constexpr uint32_t kBitKeyB = (1 << 1);
constexpr uint32_t kBitKeySelect = (1 << 2);
constexpr uint32_t kBitKeyStart = (1 << 3);
constexpr uint32_t kBitKeyRight = (1 << 4);
constexpr uint32_t kBitKeyLeft = (1 << 5);
constexpr uint32_t kBitKeyUp = (1 << 6);
constexpr uint32_t kBitKeyDown = (1 << 7);
constexpr uint32_t kBitKeyR = (1 << 8);
constexpr uint32_t kBitKeyL = (1 << 9);
constexpr uint32_t kBitKeySpeed = (1 << 10);
constexpr uint32_t kBitKeyCapture = (1 << 11);
constexpr uint32_t kBitKeyGameShark = (1 << 12);
constexpr uint32_t kBitKeyAutoA = (1 << 13);
constexpr uint32_t kBitKeyAutoB = (1 << 14);
constexpr uint32_t kBitKeyMotionUp = (1 << 15);
constexpr uint32_t kBitKeyMotionDown = (1 << 16);
constexpr uint32_t kBitKeyMotionLeft = (1 << 17);
constexpr uint32_t kBitKeyMotionRight = (1 << 18);
constexpr uint32_t kBitKeyMotionIn = (1 << 19);
constexpr uint32_t kBitKeyMotionOut = (1 << 20);

// clang-format off
constexpr std::array<uint32_t, kNbGameKeys> kBitMask = {
    kBitKeyUp,
    kBitKeyDown,
    kBitKeyLeft,
    kBitKeyRight,
    kBitKeyA,
    kBitKeyB,
    kBitKeyL,
    kBitKeyR,
    kBitKeySelect,
    kBitKeyStart,
    kBitKeyMotionUp,
    kBitKeyMotionDown,
    kBitKeyMotionLeft,
    kBitKeyMotionRight,
    kBitKeyMotionIn,
    kBitKeyMotionOut,
    kBitKeyAutoA,
    kBitKeyAutoB,
    kBitKeySpeed,
    kBitKeyCapture,
    kBitKeyGameShark,
};
// clang-format on

inline int GameKeyToInt(const GameKey& game_key) {
    return static_cast<int>(game_key);
}

// Returns true if `joypad` is in a valid joypad range.
inline bool JoypadInRange(const int& joypad) {
    constexpr int kMinJoypadIndex = 0;
    return joypad >= kMinJoypadIndex && joypad < kNbJoypads;
}

}  // namespace

// clang-format off
wxString GameKeyToString(const GameKey& game_key) {
    // Note: this must match GUI widget names or GUI won't work
    // This array's order determines tab order as well
    static const std::array<wxString, kNbGameKeys> kGameKeyStrings = {
        "Up",
        "Down",
        "Left",
        "Right",
        "A",
        "B",
        "L",
        "R",
        "Select",
        "Start",
        "MotionUp",
        "MotionDown",
        "MotionLeft",
        "MotionRight",
        "MotionIn",
        "MotionOut",
        "AutoA",
        "AutoB",
        "Speed",
        "Capture",
        "GS",
    };
    return kGameKeyStrings[GameKeyToInt(game_key)];
}

nonstd::optional<GameKey> StringToGameKey(const wxString& input) {
    static const std::map<wxString, GameKey> kStringToGameKey = {
        { "Up",          GameKey::Up },
        { "Down",        GameKey::Down },
        { "Left",        GameKey::Left },
        { "Right",       GameKey::Right },
        { "A",           GameKey::A },
        { "B",           GameKey::B },
        { "L",           GameKey::L },
        { "R",           GameKey::R },
        { "Select",      GameKey::Select },
        { "Start",       GameKey::Start },
        { "MotionUp",    GameKey::MotionUp },
        { "MotionDown",  GameKey::MotionDown },
        { "MotionLeft",  GameKey::MotionLeft },
        { "MotionRight", GameKey::MotionRight },
        { "MotionIn",    GameKey::MotionIn },
        { "MotionOut",   GameKey::MotionOut },
        { "AutoA",       GameKey::AutoA },
        { "AutoB",       GameKey::AutoB },
        { "Speed",       GameKey::Speed },
        { "Capture",     GameKey::Capture },
        { "GS",          GameKey::Gameshark },
    };

    const auto iter = kStringToGameKey.find(input);
    if (iter == kStringToGameKey.end()) {
        return nonstd::nullopt;
    }
    return iter->second;
}
// clang-format on

// static
nonstd::optional<GameControl> GameControl::FromString(const wxString& name) {
    static const wxString kJoypad("Joypad");
    if (!wxStrncmp(name, kJoypad, kJoypad.size())) {
        wxLogDebug("Doesn't start with joypad");
        return nonstd::nullopt;
    }

    auto parts = strutils::split(name, "/");
    if (parts.size() != 3) {
        wxLogDebug("Wrong split size: %d", parts.size());
        return nonstd::nullopt;
    }

    const int joypad = parts[1][0] - wxT('1');
    if (!JoypadInRange(joypad)) {
        wxLogDebug("Wrong joypad index: %d", joypad);
        return nonstd::nullopt;
    }

    nonstd::optional<GameKey> game_key = StringToGameKey(parts[2]);
    if (!game_key) {
        wxLogDebug("Failed to parse game_key: %s", parts[2]);
        return nonstd::nullopt;
    }

    return GameControl(joypad, game_key.value());
}

GameControl::GameControl(int joypad, GameKey game_key)
    : joypad_(joypad),
      game_key_(game_key),
      config_string_(wxString::Format("Joypad/%d/%s",
                                      joypad_ + 1,
                                      GameKeyToString(game_key_))) {
    assert(JoypadInRange(joypad_));
}
GameControl::~GameControl() = default;

bool GameControl::operator==(const GameControl& other) const {
    return joypad_ == other.joypad_ && game_key_ == other.game_key_;
}
bool GameControl::operator!=(const GameControl& other) const {
    return !(*this == other);
}
bool GameControl::operator<(const GameControl& other) const {
    if (joypad_ != other.joypad_) {
        return joypad_ < other.joypad_;
    }
    if (game_key_ != other.game_key_) {
        return game_key_ < other.game_key_;
    }
    return false;
}
bool GameControl::operator<=(const GameControl& other) const {
    return !(*this > other);
}
bool GameControl::operator>(const GameControl& other) const {
    return other < *this;
}
bool GameControl::operator>=(const GameControl& other) const {
    return !(*this < other);
}

GameControlState& GameControlState::Instance() {
    static GameControlState g_game_control_state;
    return g_game_control_state;
}

GameControlState::GameControlState() : joypads_({0, 0, 0, 0}) {}
GameControlState::~GameControlState() = default;

bool GameControlState::OnInputPressed(const config::UserInput& user_input) {
    assert(user_input);

    const auto& game_keys = input_bindings_.find(user_input);
    if (game_keys == input_bindings_.end()) {
        // No associated game control for `user_input`.
        return false;
    }

    auto iter = keys_pressed_.find(user_input);
    if (iter != keys_pressed_.end()) {
        // Double press is noop.
        return true;
    }

    // Remember the key pressed.
    keys_pressed_.emplace(user_input);

    // Update all corresponding controls.
    for (const GameControl& game_control : game_keys->second) {
        active_controls_[game_control].emplace(user_input);
        joypads_[game_control.joypad_] |=
            kBitMask[GameKeyToInt(game_control.game_key_)];
    }

    return true;
}

bool GameControlState::OnInputReleased(const config::UserInput& user_input) {
    assert(user_input);

    const auto& game_keys = input_bindings_.find(user_input);
    if (game_keys == input_bindings_.end()) {
        // No associated game control for `user_input`.
        return false;
    }

    auto iter = keys_pressed_.find(user_input);
    if (iter == keys_pressed_.end()) {
        // Double release is noop.
        return true;
    }

    // Release the key pressed.
    keys_pressed_.erase(iter);

    // Update all corresponding controls.
    for (const GameControl& game_control : game_keys->second) {
        auto active_controls = active_controls_.find(game_control);
        if (active_controls == active_controls_.end()) {
            // This should never happen.
            assert(false);
            return true;
        }

        active_controls->second.erase(user_input);
        if (active_controls->second.empty()) {
            // Actually release control.
            active_controls_.erase(active_controls);
            joypads_[game_control.joypad_] &=
                ~kBitMask[GameKeyToInt(game_control.game_key_)];
        }
    }

    return true;
}

void GameControlState::Reset() {
    active_controls_.clear();
    keys_pressed_.clear();
    joypads_.fill(0);
}

void GameControlState::OnGameBindingsChanged() {
    // We should reset to ensure no key remains accidentally pressed following a
    // configuration change.
    Reset();

    input_bindings_.clear();
    for (const auto& iter : gopts.game_control_bindings) {
        for (const auto& user_input : iter.second) {
            input_bindings_[user_input].emplace(iter.first);
        }
    }
}

uint32_t GameControlState::GetJoypad(int joypad) const {
    assert(JoypadInRange(joypad));
    return joypads_[joypad];
}

}  // namespace config
