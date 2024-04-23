#ifndef VBAM_WX_DIALOGS_ACCEL_CONFIG_H_
#define VBAM_WX_DIALOGS_ACCEL_CONFIG_H_

#include <unordered_map>

#include <wx/treectrl.h>

#include "wx/config/shortcuts.h"
#include "wx/dialogs/base-dialog.h"

// Forward declarations.
class wxControl;
class wxListBox;
class wxMenu;
class wxMenuBar;
class wxWindow;

namespace widgets {
class UserInputCtrl;
}

namespace dialogs {

// Manages the shortcuts editor dialog.
class AccelConfig : public BaseDialog {
public:
    static AccelConfig* NewInstance(wxWindow* parent,
                                    wxMenuBar* menu_bar,
                                    wxMenu* recents,
                                    const config::ShortcutsProvider shortcuts_provider);

    ~AccelConfig() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    AccelConfig(wxWindow* parent,
                wxMenuBar* menu_bar,
                wxMenu* recents,
                const config::ShortcutsProvider shortcuts_provider);

    // Re-initializes the configuration.
    void OnDialogShown(wxShowEvent& ev);

    // On OK, saves the global shortcuts.
    void OnValidate(wxCommandEvent& ev);

    // Fills in the key list.
    void OnCommandSelected(wxTreeEvent& ev);

    // after selecting a key in key list, enable Remove button
    void OnKeySelected(wxCommandEvent& ev);

    // remove selected binding
    void OnRemoveBinding(wxCommandEvent& ev);

    // wipe out all user bindings
    void OnResetAll(wxCommandEvent& ev);

    // remove old key binding, add new key binding, and update GUI
    void OnAssignBinding(wxCommandEvent& ev);

    // update curas and maybe enable asb
    // Called when the input text box is updated. Finds current binding.
    void OnKeyInput(wxCommandEvent& ev);

    // Helper method to populate `current_keys_` whenever the configuration or
    // selection has changed.
    void PopulateCurrentKeys();

    wxTreeCtrl* tree_;
    wxListBox* current_keys_;
    wxWindow* assign_button_;
    wxWindow* remove_button_;
    widgets::UserInputCtrl* key_input_;
    wxControl* currently_assigned_label_;
    std::unordered_map<int, wxTreeItemId> command_to_item_id_;

    config::Shortcuts config_shortcuts_;
    int selected_command_ = 0;

    const config::ShortcutsProvider shortcuts_provider_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_ACCEL_CONFIG_H_
