#include "wx/gamecontrol.h"

#include "../strutils.h"
#include "opts.h"
#include "wx/log.h"
#include "wxlogdebug.h"

namespace {

constexpr uint32_t kBitKeyA =           (1 << 0);
constexpr uint32_t kBitKeyB =           (1 << 1);
constexpr uint32_t kBitKeySelect =      (1 << 2);
constexpr uint32_t kBitKeyStart =       (1 << 3);
constexpr uint32_t kBitKeyRight =       (1 << 4);
constexpr uint32_t kBitKeyLeft =        (1 << 5);
constexpr uint32_t kBitKeyUp =          (1 << 6);
constexpr uint32_t kBitKeyDown =        (1 << 7);
constexpr uint32_t kBitKeyR =           (1 << 8);
constexpr uint32_t kBitKeyL =           (1 << 9);
constexpr uint32_t kBitKeySpeed =       (1 << 10);
constexpr uint32_t kBitKeyCapture =     (1 << 11);
constexpr uint32_t kBitKeyGameShark =   (1 << 12);
constexpr uint32_t kBitKeyAutoA =       (1 << 13);
constexpr uint32_t kBitKeyAutoB =       (1 << 14);
constexpr uint32_t kBitKeyMotionUp =    (1 << 15);
constexpr uint32_t kBitKeyMotionDown =  (1 << 16);
constexpr uint32_t kBitKeyMotionLeft =  (1 << 17);
constexpr uint32_t kBitKeyMotionRight = (1 << 18);
constexpr uint32_t kBitKeyMotionIn =    (1 << 19);
constexpr uint32_t kBitKeyMotionOut =   (1 << 20);

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

inline int GameKeyToInt(const wxGameKey& game_key) {
    return static_cast<std::underlying_type<wxGameKey>::type>(
        game_key);
}

// Returns true if `joypad` is in a valid joypad range.
inline bool JoypadInRange(const int& joypad) {
    constexpr int kMinJoypadIndex = 0;
    return joypad >= kMinJoypadIndex && joypad < kNbJoypads;
}

}  // namespace

wxString GameKeyToString(const wxGameKey& game_key) {
    // Note: this must match GUI widget names or GUI won't work
    // This array's order determines tab order as well
    static const std::array<wxString, kNbGameKeys> kGameKeyStrings = {
        wxT("Up"),
        wxT("Down"),
        wxT("Left"),
        wxT("Right"),
        wxT("A"),
        wxT("B"),
        wxT("L"),
        wxT("R"),
        wxT("Select"),
        wxT("Start"),
        wxT("MotionUp"),
        wxT("MotionDown"),
        wxT("MotionLeft"),
        wxT("MotionRight"),
        wxT("MotionIn"),
        wxT("MotionOut"),
        wxT("AutoA"),
        wxT("AutoB"),
        wxT("Speed"),
        wxT("Capture"),
        wxT("GS"),
    };
    return kGameKeyStrings[GameKeyToInt(game_key)];
}

nonstd::optional<wxGameKey> StringToGameKey(const wxString& input) {
    static const std::map<wxString, wxGameKey> kStringToGameKey = {
        { wxT("Up"),          wxGameKey::Up },
        { wxT("Down"),        wxGameKey::Down },
        { wxT("Left"),        wxGameKey::Left },
        { wxT("Right"),       wxGameKey::Right },
        { wxT("A"),           wxGameKey::A },
        { wxT("B"),           wxGameKey::B },
        { wxT("L"),           wxGameKey::L },
        { wxT("R"),           wxGameKey::R },
        { wxT("Select"),      wxGameKey::Select },
        { wxT("Start"),       wxGameKey::Start },
        { wxT("MotionUp"),    wxGameKey::MotionUp },
        { wxT("MotionDown"),  wxGameKey::MotionDown },
        { wxT("MotionLeft"),  wxGameKey::MotionLeft },
        { wxT("MotionRight"), wxGameKey::MotionRight },
        { wxT("MotionIn"),    wxGameKey::MotionIn },
        { wxT("MotionOut"),   wxGameKey::MotionOut },
        { wxT("AutoA"),       wxGameKey::AutoA },
        { wxT("AutoB"),       wxGameKey::AutoB },
        { wxT("Speed"),       wxGameKey::Speed },
        { wxT("Capture"),     wxGameKey::Capture },
        { wxT("GS"),          wxGameKey::Gameshark },
    };

    const auto iter = kStringToGameKey.find(input);
    if (iter == kStringToGameKey.end()) {
        return nonstd::nullopt;
    }
    return iter->second;
}

// static
nonstd::optional<wxGameControl> wxGameControl::FromString(const wxString &name) {
    static const wxString kJoypad(wxT("Joypad"));
    if (!wxStrncmp(name, kJoypad, kJoypad.size())) {
        wxLogDebug("Doesn't start with joypad");
        return nonstd::nullopt;
    }

    auto parts = str_split(name, wxT("/"));
    if (parts.size() != 3) {
        wxLogDebug("Wrong split size: %d", parts.size());
        return nonstd::nullopt;
    }

    const int joypad = parts[1][0] - wxT('1');
    if (!JoypadInRange(joypad)) {
        wxLogDebug("Wrong joypad index: %d", joypad);
        return nonstd::nullopt;
    }

    nonstd::optional<wxGameKey> game_key = StringToGameKey(parts[2]);
    if (!game_key) {
        wxLogDebug("Failed to parse game_key: %s", parts[2]);
        return nonstd::nullopt;
    }

    return wxGameControl(joypad, game_key.value());
}

wxGameControl::wxGameControl(int joypad, wxGameKey game_key) :
    joypad_(joypad),
    game_key_(game_key),
    config_string_(wxString::Format(
        wxT("Joypad/%d/%s"), joypad_ + 1, GameKeyToString(game_key_))) {
    assert(JoypadInRange(joypad_));
}
wxGameControl::~wxGameControl() = default;

bool wxGameControl::operator==(const wxGameControl& other) const {
    return joypad_ == other.joypad_ && game_key_ == other.game_key_;
}
bool wxGameControl::operator!=(const wxGameControl& other) const {
    return !(*this == other);
}
bool wxGameControl::operator<(const wxGameControl& other) const {
    if (joypad_ != other.joypad_) {
        return joypad_ < other.joypad_;
    }
    if (game_key_ != other.game_key_) {
        return game_key_ < other.game_key_;
    }
    return false;
}
bool wxGameControl::operator<=(const wxGameControl& other) const {
    return !(*this > other);
}
bool wxGameControl::operator>(const wxGameControl& other) const {
    return other < *this;
}
bool wxGameControl::operator>=(const wxGameControl& other) const {
    return !(*this < other);
}

wxGameControlState& wxGameControlState::Instance() {
    static wxGameControlState g_game_control_state;
    return g_game_control_state;
}

wxGameControlState::wxGameControlState() : joypads_({0, 0, 0, 0}) {}
wxGameControlState::~wxGameControlState() = default;

bool wxGameControlState::OnInputPressed(const wxUserInput& user_input) {
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
    for (const wxGameControl& game_control : game_keys->second) {
        active_controls_[game_control].emplace(user_input);
        joypads_[game_control.joypad_] |=
            kBitMask[GameKeyToInt(game_control.game_key_)];
    }

    return true;
}

bool wxGameControlState::OnInputReleased(const wxUserInput& user_input) {
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
    for (const wxGameControl& game_control : game_keys->second) {
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

void wxGameControlState::Reset() {
    active_controls_.clear();
    keys_pressed_.clear();
    joypads_.fill(0);
}

void wxGameControlState::OnGameBindingsChanged() {
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

uint32_t wxGameControlState::GetJoypad(int joypad) const {
    assert(JoypadInRange(joypad));
    return joypads_[joypad];
}

