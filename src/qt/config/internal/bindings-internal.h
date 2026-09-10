#ifndef VBAM_BINDINGS_INTERNAL_INCLUDE
#error "Do not include "config/internal/bindings-internal.h" outside of the implementation."
#endif

#include <array>
#include <unordered_map>
#include <unordered_set>

#include "qt/config/command.h"
#include "qt/config/user-input.h"

namespace config {
namespace internal {

// Returns the map of commands to their default inputs.
const std::unordered_map<Command, std::unordered_set<UserInput>>& DefaultInputs();

// Returns the default inputs for the given `command`.
// Returns an empty set if there are no default inputs for `command`.
const std::unordered_set<UserInput>& DefaultInputsForCommand(const Command& command);

// Returns true if `input` is the default input for `command`.
bool IsDefaultInputForCommand(const Command& command, const UserInput& input);

// Every game command, in INI/UI order.
const std::array<GameCommand, kNbGameKeys * kNbJoypads>& OrderedGameCommands();

}  // namespace internal
}  // namespace config
