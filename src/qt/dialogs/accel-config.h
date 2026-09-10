#ifndef VBAM_QT_DIALOGS_ACCEL_CONFIG_H_
#define VBAM_QT_DIALOGS_ACCEL_CONFIG_H_

#include <unordered_map>

#include "qt/config/bindings.h"
#include "qt/config/command.h"
#include "qt/dialogs/base-dialog.h"

class QLabel;
class QListWidget;
class QMenu;
class QMenuBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace widgets {
class UserInputCtrl;
}

namespace dialogs {

// The shortcut (accelerator) configuration dialog: a tree of the menu
// commands, the inputs currently bound to the selected command, and an input
// capture control to add a binding. Edits are made on a copy of the bindings
// and committed on OK.
class AccelConfig final : public BaseDialog {
    Q_OBJECT

public:
    static AccelConfig* NewInstance(QWidget* parent,
                                    QMenuBar* menu,
                                    const config::BindingsProvider bindings_provider);
    ~AccelConfig() override = default;

protected:
    void OnDialogShown() override;
    bool OnAccept() override;

private:
    AccelConfig(QWidget* parent, QMenuBar* menu, const config::BindingsProvider bindings_provider);

    void PopulateTreeWithMenu(QTreeWidgetItem* parent, QMenu* menu, const QString& prefix,
                              int level);
    void AppendCommandItem(QTreeWidgetItem* parent, int command, const QString& prefix,
                           int level);

    void OnCommandSelected();
    void OnKeySelected();
    void OnRemoveBinding();
    void OnResetAll();
    void OnAssignBinding();
    void OnKeyInput();
    void PopulateCurrentKeys();

    // Display strings of a command item.
    QString AssignedString(int command) const;
    QString MessageString(int command) const;

    const config::BindingsProvider bindings_provider_;
    config::Bindings config_shortcuts_;

    QTreeWidget* tree_;
    QListWidget* current_keys_;
    QPushButton* assign_button_;
    QPushButton* remove_button_;
    QPushButton* reset_all_button_;
    widgets::UserInputCtrl* key_input_;
    QLabel* currently_assigned_label_;

    std::unordered_map<int, QTreeWidgetItem*> command_to_item_;
    int selected_command_ = -1;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_ACCEL_CONFIG_H_
