#ifndef VBAM_SHORTCUTS_INTERNAL_INCLUDE
#error "Do not include "config/internal/shortcuts-internal.h" outside of the implementation."
#endif

#include <set>
#include <unordered_map>

#include "config/user-input.h"

namespace config {
namespace internal {

// Returns the map of commands to their default shortcut.
const std::unordered_map<int, UserInput>& DefaultShortcuts();

// Returns the default shortcut for the given `command`.
// Returns an Invalid UserInput if there is no default shortcut for `command`.
UserInput DefaultShortcutForCommand(int command);

}  // namespace internal
}  // namespace config
