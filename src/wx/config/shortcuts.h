#ifndef VBAM_WX_CONFIG_SHORTCUTS_H_
#define VBAM_WX_CONFIG_SHORTCUTS_H_

#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <utility>

#include "config/user-input.h"

// by default, only 9 recent items
#define wxID_FILE10 (wxID_FILE9 + 1)

namespace config {

// Represents a set of shortcuts from a user input (keyboard or joypad) to a
// command. Internally, this class keeps track of all the necessary data for
// resolving a user input to a command at runtime and for updating the INI file.
class Shortcuts {
public:
    Shortcuts();
    ~Shortcuts() = default;

    Shortcuts(Shortcuts&&) noexcept = default;
    Shortcuts& operator=(Shortcuts&&) noexcept = default;

    // Disable copy and copy assignment operator.
    // Clone() is provided only for the configuration window, this class
    // should otherwise be treated as move-only.
    Shortcuts(const Shortcuts&) = delete;
    Shortcuts& operator=(const Shortcuts&) = delete;

    // Returns the configuration for the INI file.
    // Internally, there are global default system inputs that are immediately
    // available on first run. For the configuration saved in the [Keyboard]
    // section of the vbam.ini file, we only keep track of the following:
    // - Disabled default input. These appear under [Keyboard/NOOP].
    // - User-added custom bindings. These appear under [Keyboard/CommandName].
    std::vector<std::pair<int, wxString>> GetConfiguration() const;

    // Returns the list of input currently configured for `command`.
    std::set<UserInput> InputsForCommand(int command) const;

    // Returns the command currently assigned to `input` or nullptr if none.
    int CommandForInput(const UserInput& input) const;

    // Returns the set of joysticks used by shortcuts.
    std::set<wxJoystick> Joysticks() const;

    // Returns a copy of this object. This can be an expensive operation and
    // should only be used to modify the currently active shortcuts
    // configuration.
    Shortcuts Clone() const;

    // Assigns `input` to `command`. Silently unassigns `input` if it is already
    // assigned to another command.
    void AssignInputToCommand(const UserInput& input, int command);

    // Removes `input` assignment. No-op if `input` is not assigned. `input`
    // must be a valid UserInput.
    void UnassignInput(const UserInput& input);

private:
    // Faster constructor for explitit copy.
    Shortcuts(const std::unordered_map<int, std::set<UserInput>>& command_to_inputs,
              const std::map<UserInput, int>& input_to_command,
              const std::map<UserInput, int>& disabled_defaults);

    // Helper method to unassign a binding used by the default configuration.
    // This requires special handling since the INI configuration is a diff
    // between the default bindings and the user configuration.
    void UnassignDefaultBinding(const UserInput& input);

    // Map of command to their associated input set.
    std::unordered_map<int, std::set<UserInput>> command_to_inputs_;
    // Reverse map of the above. An input can only map to a single command.
    std::map<UserInput, int> input_to_command_;
    // Disabled default inputs. This is used to easily retrieve the
    // configuration to save in the INI file.
    std::map<UserInput, int> disabled_defaults_;
};

}  // namespace config

#endif  // VBAM_WX_CONFIG_SHORTCUTS_H_
