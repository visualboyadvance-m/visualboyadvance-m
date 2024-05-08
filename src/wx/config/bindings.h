#ifndef VBAM_WX_CONFIG_BINDINGS_H_
#define VBAM_WX_CONFIG_BINDINGS_H_

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "wx/config/command.h"
#include "wx/config/user-input.h"

// wxWidgets only goes up to `wxID_FILE9` but we want 10 recent files.
#define wxID_FILE10 (wxID_FILE9 + 1)

namespace config {

// Bindings is a class that manages the association between commands and
// user inputs. It is used to manage the shortcuts configuration. The class
// provides methods to assign and unassign inputs to commands, as well as
// retrieve the current configuration for the INI file.
class Bindings {
public:
    // Returns the list of default inputs for `command`.
    static const std::unordered_set<UserInput>& DefaultInputsForCommand(const Command& command);

    Bindings();
    ~Bindings() = default;

    Bindings(Bindings&&) noexcept = default;
    Bindings& operator=(Bindings&&) noexcept = default;

    // Disable copy and copy assignment operator.
    // `Clone()` is provided only for the configuration window, this class
    // should otherwise be treated as move-only. If you wish to access the
    // Bindings configuration, do it from `wxGetApp().bindings()`.
    Bindings(const Bindings&) = delete;
    Bindings& operator=(const Bindings&) = delete;

    // Returns the shortcuts configuration for the INI file.
    // Internally, there are global default system inputs that are immediately
    // available on first run. For the configuration saved in the [Keyboard]
    // section of the vbam.ini file, we only keep track of the following:
    // - Disabled default input. These appear under [Keyboard/NOOP].
    // - User-added custom bindings. These appear under [Keyboard/CommandName].
    // Essentially, this is a diff between the default shortcuts and the user
    // configuration.
    std::vector<std::pair<wxString, wxString>> GetKeyboardConfiguration() const;

    // Returns the game control configuration for the INI file. These go in the
    // [Joypad] section of the INI file.
    std::vector<std::pair<GameCommand, wxString>> GetJoypadConfiguration() const;

    // Returns the list of input currently configured for `command`.
    std::unordered_set<UserInput> InputsForCommand(const Command& command) const;

    // Returns the Command currently assigned to `input` or nullopt if none.
    nonstd::optional<Command> CommandForInput(const UserInput& input) const;

    // Returns a copy of this object. This can be an expensive operation and
    // should only be used to modify the currently active shortcuts
    // configuration.
    Bindings Clone() const;

    // Assigns `input` to `command`. Silently unassigns `input` if it is already
    // assigned to another command.
    void AssignInputToCommand(const UserInput& input, const Command& command);

    // Assigns `inputs` to `command`. Silently unassigns any of `inputs` if they
    // are already assigned to another command. Any input previously assigned to
    // `command` will be cleared.
    void AssignInputsToCommand(const std::unordered_set<UserInput>& inputs, const Command& command);

    // Removes `input` assignment. No-op if `input` is not assigned. `input`
    // must be a valid UserInput. Call will assert otherwise. Call will assert otherwise.
    void UnassignInput(const UserInput& input);

    // Removes all assignments for `command`. No-op if `command` has no assignment.
    void ClearCommandAssignments(const Command& command);

private:
    // Faster constructor for explicit copy.
    Bindings(
        const std::unordered_map<Command, std::unordered_set<UserInput>>& control_to_inputs,
        const std::unordered_map<UserInput, Command>& input_to_control,
        const std::unordered_map<UserInput, ShortcutCommand>& disabled_defaults);

    // Helper method to unassign a binding used by the default configuration.
    // This requires special handling since the INI configuration is a diff
    // between the default bindings and the user configuration.
    void UnassignDefaultBinding(const UserInput& input);

    // Map of command to their associated input set.
    std::unordered_map<Command, std::unordered_set<UserInput>> control_to_inputs_;
    // Reverse map of the above. An input can only map to a single command.
    std::unordered_map<UserInput, Command> input_to_control_;
    // Disabled default shortcuts. This is used to easily retrieve the
    // configuration to save in the INI file.
    std::unordered_map<UserInput, ShortcutCommand> disabled_defaults_;
};

using BindingsProvider = std::function<Bindings*()>;

}  // namespace config

#endif  // VBAM_WX_CONFIG_BINDINGS_H_
