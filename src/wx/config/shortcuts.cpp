#include "wx/config/shortcuts.h"

#include <wx/string.h>
#include <wx/translation.h>
#include <wx/xrc/xmlres.h>

#include "wx/config/user-input.h"

#define VBAM_SHORTCUTS_INTERNAL_INCLUDE
#include "wx/config/internal/shortcuts-internal.h"
#undef VBAM_SHORTCUTS_INTERNAL_INCLUDE

namespace config {

namespace {

int NoopCommand() {
    static const int noop = XRCID("NOOP");
    return noop;
}

}  // namespace

Shortcuts::Shortcuts() {
    // Set up default shortcuts.
    for (const auto& iter : internal::DefaultShortcuts()) {
        AssignInputToCommand(iter.second, iter.first);
    }
}

Shortcuts::Shortcuts(const std::unordered_map<int, std::set<UserInput>>& command_to_inputs,
                     const std::map<UserInput, int>& input_to_command,
                     const std::map<UserInput, int>& disabled_defaults)
    : command_to_inputs_(command_to_inputs.begin(), command_to_inputs.end()),
      input_to_command_(input_to_command.begin(), input_to_command.end()),
      disabled_defaults_(disabled_defaults.begin(), disabled_defaults.end()) {}

std::vector<std::pair<int, wxString>> Shortcuts::GetConfiguration() const {
    std::vector<std::pair<int, wxString>> config;
    config.reserve(command_to_inputs_.size() + 1);

    if (!disabled_defaults_.empty()) {
        std::set<UserInput> noop_inputs;
        for (const auto& iter : disabled_defaults_) {
            noop_inputs.insert(iter.first);
        }
        config.push_back(std::make_pair(NoopCommand(), UserInput::SpanToConfigString(noop_inputs)));
    }

    for (const auto& iter : command_to_inputs_) {
        std::set<UserInput> inputs;
        for (const auto& input : iter.second) {
            if (internal::DefaultShortcutForCommand(iter.first) != input) {
                // Not a default input.
                inputs.insert(input);
            }
        }
        if (!inputs.empty()) {
            config.push_back(std::make_pair(iter.first, UserInput::SpanToConfigString(inputs)));
        }
    }

    return config;
}

std::set<UserInput> Shortcuts::InputsForCommand(int command) const {
    if (command == NoopCommand()) {
        std::set<UserInput> noop_inputs;
        for (const auto& iter : disabled_defaults_) {
            noop_inputs.insert(iter.first);
        }
        return noop_inputs;
    }

    auto iter = command_to_inputs_.find(command);
    if (iter == command_to_inputs_.end()) {
        return {};
    }
    return iter->second;
}

int Shortcuts::CommandForInput(const UserInput& input) const {
    const auto iter = input_to_command_.find(input);
    if (iter == input_to_command_.end()) {
        return 0;
    }
    return iter->second;
}

Shortcuts Shortcuts::Clone() const {
    return Shortcuts(this->command_to_inputs_, this->input_to_command_, this->disabled_defaults_);
}

void Shortcuts::AssignInputToCommand(const UserInput& input, int command) {
    if (command == NoopCommand()) {
        // "Assigning to Noop" means unassinging the default binding.
        UnassignDefaultBinding(input);
        return;
    }

    // Remove the existing binding if it exists.
    auto iter = input_to_command_.find(input);
    if (iter != input_to_command_.end()) {
        UnassignInput(input);
    }

    auto disabled_iter = disabled_defaults_.find(input);
    if (disabled_iter != disabled_defaults_.end()) {
        int original_command = disabled_iter->second;
        if (original_command == command) {
            // Restoring a disabled input. Remove from the disabled set.
            disabled_defaults_.erase(disabled_iter);
        }
        // Then, just continue normally.
    }

    command_to_inputs_[command].emplace(input);
    input_to_command_[input] = command;
}

void Shortcuts::UnassignInput(const UserInput& input) {
    assert(input);

    auto iter = input_to_command_.find(input);
    if (iter == input_to_command_.end()) {
        // Input not found, nothing to do.
        return;
    }

    if (internal::DefaultShortcutForCommand(iter->second) == input) {
        // Unassigning a default binding has some special handling.
        UnassignDefaultBinding(input);
        return;
    }

    // Otherwise, just remove it from the 2 maps.
    auto command_iter = command_to_inputs_.find(iter->second);
    assert(command_iter != command_to_inputs_.end());

    command_iter->second.erase(input);
    if (command_iter->second.empty()) {
        // Remove empty set.
        command_to_inputs_.erase(command_iter);
    }
    input_to_command_.erase(iter);
}

void Shortcuts::UnassignDefaultBinding(const UserInput& input) {
    auto input_iter = input_to_command_.find(input);
    if (input_iter == input_to_command_.end()) {
        // This can happen if the INI file provided by the user has an invalid
        // option. In this case, just silently ignore it.
        return;
    }

    if (internal::DefaultShortcutForCommand(input_iter->second) != input) {
        // As above, we have already removed the default binding, ignore it.
        return;
    }

    auto command_iter = command_to_inputs_.find(input_iter->second);
    assert(command_iter != command_to_inputs_.end());

    command_iter->second.erase(input);
    if (command_iter->second.empty()) {
        command_to_inputs_.erase(command_iter);
    }

    disabled_defaults_[input] = input_iter->second;
    input_to_command_.erase(input_iter);
}

}  // namespace config
