#include "dialogs/accel-config.h"

#include <wx/ctrlsub.h>
#include <wx/event.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/xrc/xmlres.h>

#include "config/shortcuts.h"
#include "config/user-input.h"
#include "dialogs/validated-child.h"
#include "opts.h"
#include "widgets/user-input-ctrl.h"
#include "wxvbam.h"

namespace dialogs {

namespace {

// Client data holding a config::UserInput.
class UserInputClientData : public wxClientData {
public:
    explicit UserInputClientData(const config::UserInput& user_input) : user_input_(user_input) {}
    explicit UserInputClientData(config::UserInput&& user_input)
        : user_input_(std::move(user_input)) {}
    ~UserInputClientData() override = default;

    const config::UserInput& user_input() const { return user_input_; }

private:
    const config::UserInput user_input_;
};

// Holds a Command reference and the corresponding string used for current
// assignment reference for fast access at configuration time. Owned by
// the corresponding wxTreeItem.
class CommandTreeItemData : public wxTreeItemData {
public:
    CommandTreeItemData(int command, wxString assigned_string, wxString message_string)
        : wxTreeItemData(),
          command_(command),
          assigned_string_(std::move(assigned_string)),
          message_string_(std::move(message_string)) {}
    ~CommandTreeItemData() override = default;

    int command() const { return command_; }
    const wxString& assigned_string() const { return assigned_string_; };
    const wxString& message_string() const { return message_string_; };

private:
    const int command_;
    const wxString assigned_string_;
    const wxString message_string_;
};

wxString AppendString(const wxString& prefix, int level, const wxString& command_name) {
    return prefix + wxString(' ', 2 * level) + command_name;
}

wxString AppendMenuItem(const wxString& prefix, int level, const wxMenuItem* menu_item) {
    return AppendString(prefix, level,
                        menu_item->GetItemLabelText() + (menu_item->IsSubMenu() ? "\n" : ""));
}

void AppendItemToTree(std::unordered_map<int, wxTreeItemId>* command_to_item_id,
                      wxTreeCtrl* tree,
                      const wxTreeItemId& parent,
                      int command,
                      const wxString& prefix,
                      int level) {
    int i = 0;
    for (; i < ncmds; i++) {
        if (command == cmdtab[i].cmd_id) {
            break;
        }
    }
    assert(i < ncmds);

    const wxTreeItemId tree_item_id =
        tree->AppendItem(parent,
                         /*text=*/cmdtab[i].name,
                         /*image=*/-1,
                         /*selImage=*/-1,
                         /*data=*/
                         new CommandTreeItemData(
                             command, AppendString(prefix, level, cmdtab[i].name), cmdtab[i].name));
    command_to_item_id->emplace(command, tree_item_id);
}

// Built the initial tree control from the menu.
void PopulateTreeWithMenu(std::unordered_map<int, wxTreeItemId>* command_to_item_id,
                          wxTreeCtrl* tree,
                          const wxTreeItemId& parent,
                          wxMenu* menu,
                          const wxMenu* recents,
                          const wxString& prefix,
                          int level = 1) {
    for (auto menu_item : menu->GetMenuItems()) {
        if (menu_item->IsSeparator()) {
            tree->AppendItem(parent, "-----");
        } else if (menu_item->IsSubMenu()) {
            if (menu_item->GetSubMenu() == recents) {
                // This has to be done manually because not all recents are always populated.
                const wxTreeItemId recents_parent =
                    tree->AppendItem(parent, menu_item->GetItemLabelText());
                const wxString recents_prefix = AppendMenuItem(prefix, level, menu_item);
                for (int i = wxID_FILE1; i <= wxID_FILE10; i++) {
                    AppendItemToTree(command_to_item_id, tree, recents_parent, i, recents_prefix,
                                     level + 1);
                }
            } else {
                const wxTreeItemId sub_parent =
                    tree->AppendItem(parent, menu_item->GetItemLabelText());
                PopulateTreeWithMenu(command_to_item_id, tree, sub_parent, menu_item->GetSubMenu(),
                                     recents, AppendMenuItem(prefix, level, menu_item), level + 1);
            }
        } else {
            AppendItemToTree(command_to_item_id, tree, parent, menu_item->GetId(), prefix, level);
        }
    }
}

}  // namespace

// static
AccelConfig* AccelConfig::NewInstance(wxWindow* parent, wxMenuBar* menu, wxMenu* recents) {
    assert(parent);
    assert(menu);
    assert(recents);
    return new AccelConfig(parent, menu, recents);
}

AccelConfig::AccelConfig(wxWindow* parent, wxMenuBar* menu, wxMenu* recents)
    : wxDialog(), keep_on_top_styler_(this) {
    assert(parent);
    assert(menu);

    // Load the dialog XML.
    const bool success = wxXmlResource::Get()->LoadDialog(this, parent, "AccelConfig");
    assert(success);

    // Loads the various dialog elements.
    tree_ = GetValidatedChild<wxTreeCtrl>(this, "Commands");
    current_keys_ = GetValidatedChild<wxListBox>(this, "Current");
    assign_button_ = GetValidatedChild(this, "Assign");
    remove_button_ = GetValidatedChild(this, "Remove");
    key_input_ = GetValidatedChild<widgets::UserInputCtrl>(this, "Shortcut");
    currently_assigned_label_ = GetValidatedChild<wxControl>(this, "AlreadyThere");

    // Configure the key input.
    key_input_->MoveBeforeInTabOrder(assign_button_);

    // Populate the tree from the menu.
    wxTreeItemId root_id = tree_->AddRoot("root");
    wxTreeItemId menu_id = tree_->AppendItem(root_id, _("Menu commands"));
    for (size_t i = 0; i < menu->GetMenuCount(); i++) {
        wxTreeItemId id = tree_->AppendItem(menu_id, menu->GetMenuLabelText(i));
        PopulateTreeWithMenu(&command_to_item_id_, tree_, id, menu->GetMenu(i), recents,
                             menu->GetMenuLabelText(i) + '\n');
    }
    tree_->ExpandAll();
    tree_->SelectItem(menu_id);

    // Set the initial tree size.
    wxSize size = tree_->GetBestSize();
    size.SetHeight(std::min(200, size.GetHeight()));
    tree_->SetSize(size);
    size.SetWidth(-1);  // maybe allow it to become bigger
    tree_->SetSizeHints(size, size);

    int w, h;
    current_keys_->GetTextExtent("CTRL-ALT-SHIFT-ENTER", &w, &h);
    size.Set(w, h);
    current_keys_->SetMinSize(size);

    // Compute max size for currently_assigned_label_.
    size.Set(0, 0);
    for (const auto& iter : command_to_item_id_) {
        const CommandTreeItemData* item_data =
            static_cast<const CommandTreeItemData*>(tree_->GetItemData(iter.second));
        assert(item_data);

        currently_assigned_label_->GetTextExtent(item_data->assigned_string(), &w, &h);
        size.SetWidth(std::max(w, size.GetWidth()));
        size.SetHeight(std::max(h, size.GetHeight()));
    }
    currently_assigned_label_->SetMinSize(size);
    currently_assigned_label_->SetSizeHints(size);

    // Finally, bind the events.
    Bind(wxEVT_SHOW, &AccelConfig::OnDialogShown, this, GetId());
    Bind(wxEVT_TREE_SEL_CHANGING, &AccelConfig::OnCommandSelected, this, tree_->GetId());
    Bind(wxEVT_TREE_SEL_CHANGED, &AccelConfig::OnCommandSelected, this, tree_->GetId());
    Bind(wxEVT_LISTBOX, &AccelConfig::OnKeySelected, this, current_keys_->GetId());
    Bind(wxEVT_BUTTON, &AccelConfig::OnValidate, this, wxID_OK);
    Bind(wxEVT_BUTTON, &AccelConfig::OnAssignBinding, this, assign_button_->GetId());
    Bind(wxEVT_BUTTON, &AccelConfig::OnRemoveBinding, this, remove_button_->GetId());
    Bind(wxEVT_BUTTON, &AccelConfig::OnResetAll, this, XRCID("ResetAll"));
    Bind(wxEVT_TEXT, &AccelConfig::OnKeyInput, this, key_input_->GetId());

    // And fit everything nicely.
    Fit();
}

void AccelConfig::OnDialogShown(wxShowEvent& ev) {
    // Let the event propagate.
    ev.Skip();

    if (!ev.IsShown()) {
        return;
    }

    // Reset the dialog.
    current_keys_->Clear();
    tree_->Unselect();
    tree_->ExpandAll();
    key_input_->Clear();
    assign_button_->Enable(false);
    remove_button_->Enable(false);
    currently_assigned_label_->SetLabel("");

    config_shortcuts_ = gopts.shortcuts.Clone();
}

void AccelConfig::OnValidate(wxCommandEvent& ev) {
    gopts.shortcuts = std::move(config_shortcuts_);
    ev.Skip();
}

void AccelConfig::OnCommandSelected(wxTreeEvent& ev) {
    const CommandTreeItemData* command_tree_data =
        static_cast<const CommandTreeItemData*>(tree_->GetItemData(ev.GetItem()));

    if (!command_tree_data) {
        selected_command_ = 0;
        PopulateCurrentKeys();
        ev.Veto();
        return;
    }

    if (ev.GetEventType() == wxEVT_COMMAND_TREE_SEL_CHANGING) {
        ev.Skip();
        return;
    }

    selected_command_ = command_tree_data->command();
    PopulateCurrentKeys();
}

void AccelConfig::OnKeySelected(wxCommandEvent&) {
    remove_button_->Enable(current_keys_->GetSelection() != wxNOT_FOUND);
}

void AccelConfig::OnRemoveBinding(wxCommandEvent&) {
    const int selection = current_keys_->GetSelection();
    if (selection == wxNOT_FOUND) {
        return;
    }

    config_shortcuts_.UnassignInput(
        static_cast<UserInputClientData*>(current_keys_->GetClientObject(selection))->user_input());
    PopulateCurrentKeys();
}

void AccelConfig::OnResetAll(wxCommandEvent&) {
    const int confirmation = wxMessageBox(
        _("This will clear all user-defined accelerators. Are you sure?"), _("Confirm"), wxYES_NO);
    if (confirmation != wxYES) {
        return;
    }

    config_shortcuts_ = config::Shortcuts();
    tree_->Unselect();
    key_input_->Clear();
    PopulateCurrentKeys();
}

void AccelConfig::OnAssignBinding(wxCommandEvent&) {
    const wxTreeItemId selected_id = tree_->GetSelection();
    const config::UserInput user_input = key_input_->SingleInput();
    if (!selected_id.IsOk() || !user_input) {
        return;
    }

    const CommandTreeItemData* data =
        static_cast<CommandTreeItemData*>(tree_->GetItemData(selected_id));
    if (!data) {
        return;
    }

    const int old_command = config_shortcuts_.CommandForInput(user_input);
    if (old_command != 0) {
        const auto iter = command_to_item_id_.find(old_command);
        assert(iter != command_to_item_id_.end());
        const CommandTreeItemData* old_command_item_data =
            static_cast<const CommandTreeItemData*>(tree_->GetItemData(iter->second));
        assert(old_command_item_data);

        // Require user confirmation to override.
        const int confirmation =
            wxMessageBox(wxString::Format(_("This will unassign \"%s\" from \"%s\". Are you sure?"),
                                          user_input.ToLocalizedString(),
                                          old_command_item_data->message_string()),
                         _("Confirm"), wxYES_NO);
        if (confirmation != wxYES) {
            return;
        }
    }

    config_shortcuts_.AssignInputToCommand(user_input, data->command());
    PopulateCurrentKeys();
}

void AccelConfig::OnKeyInput(wxCommandEvent&) {
    const config::UserInput user_input = key_input_->SingleInput();
    if (!user_input) {
        currently_assigned_label_->SetLabel(wxEmptyString);
        assign_button_->Enable(false);
        return;
    }

    const int command = config_shortcuts_.CommandForInput(user_input);
    if (command == 0) {
        // No existing assignment.
        currently_assigned_label_->SetLabel(wxEmptyString);
    } else {
        // Existing assignment, inform the user.
        const auto iter = command_to_item_id_.find(command);
        assert(iter != command_to_item_id_.end());
        currently_assigned_label_->SetLabel(
            static_cast<CommandTreeItemData*>(tree_->GetItemData(iter->second))->assigned_string());
    }

    assign_button_->Enable(true);
}

void AccelConfig::PopulateCurrentKeys() {
    const int previous_selection = current_keys_->GetSelection();
    current_keys_->Clear();

    if (selected_command_ == 0) {
        return;
    }

    // Populate `current_keys`.
    int new_keys_count = 0;
    for (const auto& user_input : config_shortcuts_.InputsForCommand(selected_command_)) {
        current_keys_->Append(user_input.ToLocalizedString(), new UserInputClientData(user_input));
        new_keys_count++;
    }

    // Reset the selection accordingly.
    if (previous_selection == wxNOT_FOUND || new_keys_count == 0) {
        current_keys_->SetSelection(wxNOT_FOUND);
        remove_button_->Enable(false);
    } else {
        current_keys_->SetSelection(std::min(previous_selection, new_keys_count - 1));
        remove_button_->Enable(true);
    }

}

}  // namespace dialogs
