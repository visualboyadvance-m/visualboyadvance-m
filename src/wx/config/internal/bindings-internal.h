#ifndef VBAM_BINDINGS_INTERNAL_INCLUDE
#error "Do not include "config/internal/bindings-internal.h" outside of the implementation."
#endif

#include <array>
#include <unordered_set>
#include <unordered_map>

#include "wx/config/command.h"
#include "wx/config/user-input.h"

namespace config {
namespace internal {

// Returns the map of commands to their default inputs.
const std::unordered_map<Command, std::unordered_set<UserInput>>& DefaultInputs();

// Returns the default inputs for the given `command`.
// Returns an empty set if there are no default inputs for `command`.
const std::unordered_set<UserInput>& DefaultInputsForCommand(const Command& command);

// Returns true if `input` is the default input for `command`.
bool IsDefaultInputForCommand(const Command& command, const UserInput& input);

// clang-format off
static constexpr std::array<GameCommand, kNbGameKeys * kNbJoypads> kOrderedGameCommands = {
    GameCommand(GameJoy(0), GameKey::Up),
    GameCommand(GameJoy(0), GameKey::Down),
    GameCommand(GameJoy(0), GameKey::Left),
    GameCommand(GameJoy(0), GameKey::Right),
    GameCommand(GameJoy(0), GameKey::A),
    GameCommand(GameJoy(0), GameKey::B),
    GameCommand(GameJoy(0), GameKey::L),
    GameCommand(GameJoy(0), GameKey::R),
    GameCommand(GameJoy(0), GameKey::Select),
    GameCommand(GameJoy(0), GameKey::Start),
    GameCommand(GameJoy(0), GameKey::MotionUp),
    GameCommand(GameJoy(0), GameKey::MotionDown),
    GameCommand(GameJoy(0), GameKey::MotionLeft),
    GameCommand(GameJoy(0), GameKey::MotionRight),
    GameCommand(GameJoy(0), GameKey::MotionIn),
    GameCommand(GameJoy(0), GameKey::MotionOut),
    GameCommand(GameJoy(0), GameKey::AutoA),
    GameCommand(GameJoy(0), GameKey::AutoB),
    GameCommand(GameJoy(0), GameKey::Speed),
    GameCommand(GameJoy(0), GameKey::Capture),
    GameCommand(GameJoy(0), GameKey::Gameshark),

    GameCommand(GameJoy(1), GameKey::Up),
    GameCommand(GameJoy(1), GameKey::Down),
    GameCommand(GameJoy(1), GameKey::Left),
    GameCommand(GameJoy(1), GameKey::Right),
    GameCommand(GameJoy(1), GameKey::A),
    GameCommand(GameJoy(1), GameKey::B),
    GameCommand(GameJoy(1), GameKey::L),
    GameCommand(GameJoy(1), GameKey::R),
    GameCommand(GameJoy(1), GameKey::Select),
    GameCommand(GameJoy(1), GameKey::Start),
    GameCommand(GameJoy(1), GameKey::MotionUp),
    GameCommand(GameJoy(1), GameKey::MotionDown),
    GameCommand(GameJoy(1), GameKey::MotionLeft),
    GameCommand(GameJoy(1), GameKey::MotionRight),
    GameCommand(GameJoy(1), GameKey::MotionIn),
    GameCommand(GameJoy(1), GameKey::MotionOut),
    GameCommand(GameJoy(1), GameKey::AutoA),
    GameCommand(GameJoy(1), GameKey::AutoB),
    GameCommand(GameJoy(1), GameKey::Speed),
    GameCommand(GameJoy(1), GameKey::Capture),
    GameCommand(GameJoy(1), GameKey::Gameshark),

    GameCommand(GameJoy(2), GameKey::Up),
    GameCommand(GameJoy(2), GameKey::Down),
    GameCommand(GameJoy(2), GameKey::Left),
    GameCommand(GameJoy(2), GameKey::Right),
    GameCommand(GameJoy(2), GameKey::A),
    GameCommand(GameJoy(2), GameKey::B),
    GameCommand(GameJoy(2), GameKey::L),
    GameCommand(GameJoy(2), GameKey::R),
    GameCommand(GameJoy(2), GameKey::Select),
    GameCommand(GameJoy(2), GameKey::Start),
    GameCommand(GameJoy(2), GameKey::MotionUp),
    GameCommand(GameJoy(2), GameKey::MotionDown),
    GameCommand(GameJoy(2), GameKey::MotionLeft),
    GameCommand(GameJoy(2), GameKey::MotionRight),
    GameCommand(GameJoy(2), GameKey::MotionIn),
    GameCommand(GameJoy(2), GameKey::MotionOut),
    GameCommand(GameJoy(2), GameKey::AutoA),
    GameCommand(GameJoy(2), GameKey::AutoB),
    GameCommand(GameJoy(2), GameKey::Speed),
    GameCommand(GameJoy(2), GameKey::Capture),
    GameCommand(GameJoy(2), GameKey::Gameshark),

    GameCommand(GameJoy(3), GameKey::Up),
    GameCommand(GameJoy(3), GameKey::Down),
    GameCommand(GameJoy(3), GameKey::Left),
    GameCommand(GameJoy(3), GameKey::Right),
    GameCommand(GameJoy(3), GameKey::A),
    GameCommand(GameJoy(3), GameKey::B),
    GameCommand(GameJoy(3), GameKey::L),
    GameCommand(GameJoy(3), GameKey::R),
    GameCommand(GameJoy(3), GameKey::Select),
    GameCommand(GameJoy(3), GameKey::Start),
    GameCommand(GameJoy(3), GameKey::MotionUp),
    GameCommand(GameJoy(3), GameKey::MotionDown),
    GameCommand(GameJoy(3), GameKey::MotionLeft),
    GameCommand(GameJoy(3), GameKey::MotionRight),
    GameCommand(GameJoy(3), GameKey::MotionIn),
    GameCommand(GameJoy(3), GameKey::MotionOut),
    GameCommand(GameJoy(3), GameKey::AutoA),
    GameCommand(GameJoy(3), GameKey::AutoB),
    GameCommand(GameJoy(3), GameKey::Speed),
    GameCommand(GameJoy(3), GameKey::Capture),
    GameCommand(GameJoy(3), GameKey::Gameshark),
};
// clang-format on

}  // namespace internal
}  // namespace config
