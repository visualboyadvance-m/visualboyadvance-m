#include "wx/config/bindings.h"

#include <wx/string.h>
#include <wx/translation.h>
#include <wx/xrc/xmlres.h>
#include <unordered_set>

#include "wx/config/user-input.h"

#define VBAM_BINDINGS_INTERNAL_INCLUDE
#include "wx/config/internal/bindings-internal.h"
#undef VBAM_BINDINGS_INTERNAL_INCLUDE

namespace config {

namespace {

int NoopCommand() {
    static const int noop = XRCID("NOOP");
    return noop;
}

}  // namespace

// static
const std::unordered_set<UserInput>& Bindings::DefaultInputsForCommand(const Command& command) {
    return internal::DefaultInputsForCommand(command);
}

Bindings::Bindings() {
    // Set up default shortcuts.
    for (const auto& iter : internal::DefaultInputs()) {
        for (const auto& input : iter.second) {
            AssignInputToCommand(input, iter.first);
        }
    }
}

Bindings::Bindings(
    const std::unordered_map<Command, std::unordered_set<UserInput>>& control_to_inputs,
    const std::unordered_map<UserInput, Command>& input_to_control,
    const std::unordered_map<UserInput, ShortcutCommand>& disabled_defaults)
    : control_to_inputs_(control_to_inputs.begin(), control_to_inputs.end()),
      input_to_control_(input_to_control.begin(), input_to_control.end()),
      disabled_defaults_(disabled_defaults.begin(), disabled_defaults.end()) {}

std::vector<std::pair<int, wxString>> Bindings::GetKeyboardConfiguration() const {
    std::vector<std::pair<int, wxString>> config;
    config.reserve(control_to_inputs_.size() + 1);

    if (!disabled_defaults_.empty()) {
        std::unordered_set<UserInput> noop_inputs;
        for (const auto& iter : disabled_defaults_) {
            noop_inputs.insert(iter.first);
        }
        config.push_back(std::make_pair(NoopCommand(), UserInput::SpanToConfigString(noop_inputs)));
    }

    for (const auto& iter : control_to_inputs_) {
        if (iter.first.is_game()) {
            // We only consider shortcut assignments here.
            continue;
        }

        // Gather the inputs for this command.
        std::unordered_set<UserInput> inputs;
        for (const auto& input : iter.second) {
            if (internal::IsDefaultInputForCommand(iter.first, input)) {
                // Default assignments are ignored.
                continue;
            }
            // Not a default input.
            inputs.insert(input);
        }

        if (!inputs.empty()) {
            const int command_id = iter.first.shortcut().id();
            config.push_back(std::make_pair(command_id, UserInput::SpanToConfigString(inputs)));
        }
    }

    return config;
}

std::vector<std::pair<GameCommand, wxString>> Bindings::GetJoypadConfiguration() const {
    std::vector<std::pair<GameCommand, wxString>> config;
    config.reserve(kNbGameKeys * kNbJoypads);

    for (const auto& game_command : internal::kOrderedGameCommands) {
        const auto iter = control_to_inputs_.find(Command(game_command));
        if (iter == control_to_inputs_.end()) {
            config.push_back(std::make_pair(game_command, wxEmptyString));
            continue;
        }

        const std::unordered_set<UserInput>& inputs = iter->second;
        config.push_back(std::make_pair(game_command, UserInput::SpanToConfigString(inputs)));
    }

    return config;
}

std::unordered_set<UserInput> Bindings::InputsForCommand(const Command& command) const {
    if (command.is_shortcut() && command.shortcut().id() == NoopCommand()) {
        std::unordered_set<UserInput> noop_inputs;
        for (const auto& iter : disabled_defaults_) {
            noop_inputs.insert(iter.first);
        }
        return noop_inputs;
    }

    auto iter = control_to_inputs_.find(command);
    if (iter == control_to_inputs_.end()) {
        return {};
    }
    return iter->second;
}

nonstd::optional<Command> Bindings::CommandForInput(const UserInput& input) const {
    const auto iter = input_to_control_.find(input);
    if (iter == input_to_control_.end()) {
        return nonstd::nullopt;
    }
    return iter->second;
}

Bindings Bindings::Clone() const {
    return Bindings(this->control_to_inputs_, this->input_to_control_, this->disabled_defaults_);
}

void Bindings::AssignInputToCommand(const UserInput& input, const Command& command) {
    if (command.is_shortcut() && command.shortcut().id() == NoopCommand()) {
        // "Assigning to Noop" means unassinging the default binding.
        UnassignDefaultBinding(input);
        return;
    }

    // Remove the existing binding if it exists.
    auto iter = input_to_control_.find(input);
    if (iter != input_to_control_.end()) {
        UnassignInput(input);
    }

    if (command.is_shortcut()) {
        const ShortcutCommand& shortcut_command = command.shortcut();
        auto disabled_iter = disabled_defaults_.find(input);
        if (disabled_iter != disabled_defaults_.end()) {
            const ShortcutCommand& original_command = disabled_iter->second;
            if (original_command == shortcut_command) {
                // Restoring a disabled input. Remove from the disabled set.
                disabled_defaults_.erase(disabled_iter);
            }
            // Then, just continue normally.
        }
    }

    control_to_inputs_[command].emplace(input);
    input_to_control_.emplace(std::make_pair(input, command));
}

void Bindings::AssignInputsToCommand(const std::unordered_set<UserInput>& inputs,
                                      const Command& command) {
    // Remove the existing binding if it exists.
    const auto iter = control_to_inputs_.find(command);
    if (iter != control_to_inputs_.end()) {
        // We need to make a copy here because the iterator is going to be invalidated.
        const std::unordered_set<UserInput> inputs_to_unassign = iter->second;
        for (const UserInput& user_input : inputs_to_unassign) {
            UnassignInput(user_input);
        }
    }

    for (const UserInput& user_input : inputs) {
        AssignInputToCommand(user_input, command);
    }
}

void Bindings::UnassignInput(const UserInput& input) {
    assert(input);

    auto iter = input_to_control_.find(input);
    if (iter == input_to_control_.end()) {
        // Input not found, nothing to do.
        return;
    }

    if (iter->second.is_shortcut()) {
        if (internal::IsDefaultInputForCommand(iter->second, input)) {
            // Unassigning a default binding has some special handling.
            UnassignDefaultBinding(input);
            return;
        }
    }

    // Otherwise, just remove it from the 2 maps.
    auto command_iter = control_to_inputs_.find(iter->second);
    assert(command_iter != control_to_inputs_.end());

    command_iter->second.erase(input);
    if (command_iter->second.empty()) {
        // Remove empty set.
        control_to_inputs_.erase(command_iter);
    }
    input_to_control_.erase(iter);
}

void Bindings::ClearCommandAssignments(const Command& command) {
    auto iter = control_to_inputs_.find(command);
    if (iter == control_to_inputs_.end()) {
        // Command not found, nothing to do.
        return;
    }

    for (const UserInput& input : iter->second) {
        input_to_control_.erase(input);
    }
    control_to_inputs_.erase(iter);
}

void Bindings::UnassignDefaultBinding(const UserInput& input) {
    auto input_iter = input_to_control_.find(input);
    if (input_iter == input_to_control_.end()) {
        // This can happen if the INI file provided by the user has an invalid
        // option. In this case, just silently ignore it.
        return;
    }

    if (!input_iter->second.is_shortcut()) {
        return;
    }

    if (!internal::IsDefaultInputForCommand(input_iter->second, input)) {
        // As above, we have already removed the default binding, ignore it.
        return;
    }

    auto command_iter = control_to_inputs_.find(input_iter->second);
    assert(command_iter != control_to_inputs_.end());

    command_iter->second.erase(input);
    if (command_iter->second.empty()) {
        control_to_inputs_.erase(command_iter);
    }

    disabled_defaults_.emplace(std::make_pair(input, input_iter->second.shortcut()));
    input_to_control_.erase(input_iter);
}

}  // namespace config
