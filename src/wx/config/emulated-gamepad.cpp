#include "wx/config/emulated-gamepad.h"

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

inline size_t GameKeyToInt(const GameKey& game_key) {
    return static_cast<size_t>(game_key);
}

}  // namespace

EmulatedGamepad::EmulatedGamepad(const BindingsProvider bindings_provider)
    : joypads_({0, 0, 0, 0}), bindings_provider_(bindings_provider) {}

bool EmulatedGamepad::OnInputPressed(const config::UserInput& user_input) {
    assert(user_input);

    const auto command = bindings_provider_()->CommandForInput(user_input);
    if (!command || !command->is_game()) {
        // No associated game control for `user_input`.
        return false;
    }

    // Update the corresponding control.
    auto iter = active_controls_.find(command->game());
    if (iter == active_controls_.end()) {
        iter = active_controls_
                   .insert(std::make_pair(command->game(), std::unordered_set<UserInput>()))
                   .first;
    }

    iter->second.emplace(user_input);
    joypads_[command->game().joypad().index()] |=
        kBitMask[GameKeyToInt(command->game().game_key())];
    return true;
}

bool EmulatedGamepad::OnInputReleased(const config::UserInput& user_input) {
    assert(user_input);

    const auto command = bindings_provider_()->CommandForInput(user_input);
    if (!command || !command->is_game()) {
        // No associated game control for `user_input`.
        return false;
    }

    // Update the corresponding control.
    auto iter = active_controls_.find(command->game());
    if (iter == active_controls_.end()) {
        // Double release is noop.
        return true;
    }

    iter->second.erase(user_input);
    if (iter->second.empty()) {
        // Actually release control.
        active_controls_.erase(iter);
        joypads_[command->game().joypad().index()] &=
            ~kBitMask[GameKeyToInt(command->game().game_key())];
    }
    return true;
}

void EmulatedGamepad::Reset() {
    active_controls_.clear();
    joypads_.fill(0);
}

uint32_t EmulatedGamepad::GetJoypad(size_t joypad) const {
    if (joypad >= kNbJoypads) {
        return 0;
    }
    return joypads_[joypad];
}

}  // namespace config
