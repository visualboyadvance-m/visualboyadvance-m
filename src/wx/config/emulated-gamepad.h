#ifndef VBAM_WX_CONFIG_EMULATED_GAMEPAD_H_
#define VBAM_WX_CONFIG_EMULATED_GAMEPAD_H_

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include <wx/string.h>

#include "wx/config/bindings.h"
#include "wx/config/command.h"
#include "wx/config/user-input.h"

namespace config {

// Tracks in-game input and computes the joypad value used to send control input
// data to the emulator. This class should be kept as a singleton owned by the
// application.
class EmulatedGamepad final {
public:
    explicit EmulatedGamepad(const BindingsProvider bindings_provider);
    ~EmulatedGamepad() = default;

    // Disable copy constructor and assignment operator.
    EmulatedGamepad(const EmulatedGamepad&) = delete;
    EmulatedGamepad& operator=(const EmulatedGamepad&) = delete;

    // Processes `user_input` and updates the internal tracking state.
    // Returns true if `user_input` corresponds to a game input.
    bool OnInputPressed(const UserInput& user_input);
    bool OnInputReleased(const UserInput& user_input);

    // Clears all input.
    void Reset();

    uint32_t GetJoypad(size_t joypad) const;

private:
    std::unordered_map<GameCommand, std::unordered_set<UserInput>> active_controls_;
    std::array<uint32_t, kNbJoypads> joypads_;
    const BindingsProvider bindings_provider_;
};

using EmulatedGamepadProvider = std::function<EmulatedGamepad*()>;

}  // namespace config

#endif  // VBAM_WX_CONFIG_EMULATED_GAMEPAD_H_
