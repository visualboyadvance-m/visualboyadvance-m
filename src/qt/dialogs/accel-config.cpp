#include "qt/dialogs/accel-config.h"

#include <QAction>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "qt/cmd-ids.h"
#include "qt/config/cmdtab.h"
#include "qt/config/user-input.h"
#include "qt/main-window.h"
#include "qt/opts.h"
#include "qt/widgets/user-input-ctrl.h"

namespace dialogs {

namespace {

constexpr int kCommandRole = Qt::UserRole;
constexpr int kAssignedStringRole = Qt::UserRole + 1;
constexpr int kMessageStringRole = Qt::UserRole + 2;

QString StripMnemonic(QString text) {
    return text.remove(QLatin1Char('&'));
}

QString AppendString(const QString& prefix, int level, const QString& command_name) {
    return prefix + QString(2 * level, QLatin1Char(' ')) + command_name;
}

int CommandOfAction(const QAction* action) {
    bool ok = false;
    const int id = action->data().toInt(&ok);
    return ok ? id : cmd::kInvalid;
}

}  // namespace

// static
AccelConfig* AccelConfig::NewInstance(QWidget* parent,
                                      QMenuBar* menu,
                                      const config::BindingsProvider bindings_provider) {
    VBAM_CHECK(parent);
    VBAM_CHECK(menu);
    VBAM_CHECK(bindings_provider);
    return new AccelConfig(parent, menu, bindings_provider);
}

AccelConfig::AccelConfig(QWidget* parent, QMenuBar* menu,
                         const config::BindingsProvider bindings_provider)
    : BaseDialog(parent, "AccelConfig"), bindings_provider_(bindings_provider) {
    setWindowTitle(tr("Customize UI"));

    auto* layout = new QVBoxLayout(this);
    auto* grid = new QGridLayout();

    // Left: the command tree.
    grid->addWidget(new QLabel(tr("Commands:"), this), 0, 0);
    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setMinimumSize(300, 300);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    grid->addWidget(tree_, 1, 0, 3, 1);

    // Right: current keys + buttons.
    grid->addWidget(new QLabel(tr("Current Keys:"), this), 0, 1);
    current_keys_ = new QListWidget(this);
    current_keys_->setMinimumWidth(
        current_keys_->fontMetrics().horizontalAdvance(QStringLiteral("CTRL-ALT-SHIFT-ENTER")) +
        40);
    grid->addWidget(current_keys_, 1, 1);

    auto* buttons = new QVBoxLayout();
    assign_button_ = new QPushButton(tr("Assign"), this);
    remove_button_ = new QPushButton(tr("Remove"), this);
    reset_all_button_ = new QPushButton(tr("Reset All"), this);
    for (QPushButton* b : {assign_button_, remove_button_, reset_all_button_}) {
        b->setAutoDefault(false);
        buttons->addWidget(b);
    }
    buttons->addStretch(1);
    grid->addLayout(buttons, 1, 2);

    auto* assigned_row = new QHBoxLayout();
    assigned_row->addWidget(new QLabel(tr("Currently assigned to:"), this));
    currently_assigned_label_ = new QLabel(this);
    currently_assigned_label_->setWordWrap(true);
    assigned_row->addWidget(currently_assigned_label_, 1);
    grid->addLayout(assigned_row, 2, 1, 1, 2);

    auto* shortcut_row = new QHBoxLayout();
    shortcut_row->addWidget(new QLabel(tr("Shortcut Key:"), this));
    key_input_ = new widgets::UserInputCtrl(this);
    key_input_->SetMultiKey(false);
    shortcut_row->addWidget(key_input_, 1);
    grid->addLayout(shortcut_row, 3, 1, 1, 2);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid, 1);
    layout->addWidget(CreateOkCancel());

    // Populate the tree from the menu bar.
    QTreeWidgetItem* menu_item = new QTreeWidgetItem(tree_, {tr("Menu commands")});
    for (QAction* top : menu->actions()) {
        QMenu* sub = top->menu();
        if (!sub) {
            continue;
        }
        const QString label = StripMnemonic(top->text());
        auto* item = new QTreeWidgetItem(menu_item, {label});
        PopulateTreeWithMenu(item, sub, label + QLatin1Char('\n'), 1);
    }
    tree_->expandAll();

    // Bind the events.
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &AccelConfig::OnCommandSelected);
    connect(current_keys_, &QListWidget::itemSelectionChanged, this, &AccelConfig::OnKeySelected);
    connect(assign_button_, &QPushButton::clicked, this, &AccelConfig::OnAssignBinding);
    connect(remove_button_, &QPushButton::clicked, this, &AccelConfig::OnRemoveBinding);
    connect(reset_all_button_, &QPushButton::clicked, this, &AccelConfig::OnResetAll);
    connect(key_input_, &widgets::UserInputCtrl::inputsChanged, this, &AccelConfig::OnKeyInput);
}

void AccelConfig::AppendCommandItem(QTreeWidgetItem* parent, int command, const QString& prefix,
                                    int level) {
    const QString helper = config::GetCommandHelper(command);
    auto* item = new QTreeWidgetItem(parent, {helper});
    item->setData(0, kCommandRole, command);
    item->setData(0, kAssignedStringRole, AppendString(prefix, level, helper));
    item->setData(0, kMessageStringRole, helper);
    command_to_item_.emplace(command, item);
}

void AccelConfig::PopulateTreeWithMenu(QTreeWidgetItem* parent, QMenu* menu,
                                       const QString& prefix, int level) {
    // The Recent menu is rebuilt dynamically with only the existing files, so
    // list every File1..File10 command explicitly when we find it.
    bool is_recent_menu = false;
    for (QAction* action : menu->actions()) {
        if (CommandOfAction(action) == cmd::kRecentReset) {
            is_recent_menu = true;
            break;
        }
    }

    for (QAction* action : menu->actions()) {
        if (action->isSeparator()) {
            new QTreeWidgetItem(parent, {QStringLiteral("-----")});
        } else if (QMenu* sub = action->menu()) {
            const QString label = StripMnemonic(action->text());
            auto* item = new QTreeWidgetItem(parent, {label});
            PopulateTreeWithMenu(item, sub, AppendString(prefix, level, label) + QLatin1Char('\n'),
                                 level + 1);
        } else {
            const int command = CommandOfAction(action);
            if (is_recent_menu && command >= cmd::kFile1 && command <= cmd::kFile10) {
                continue;
            }
            if (command != cmd::kInvalid && config::IsCommandId(command)) {
                AppendCommandItem(parent, command, prefix, level);
            }
            // Else: a menu item with no command entry (e.g. the dynamically
            // built Language items); it has no stable id to bind a shortcut to.
        }
    }

    if (is_recent_menu) {
        for (int command = cmd::kFile1; command <= cmd::kFile10; command++) {
            AppendCommandItem(parent, command, prefix, level);
        }
    }
}

void AccelConfig::OnDialogShown() {
    // Reset the dialog.
    current_keys_->clear();
    tree_->clearSelection();
    tree_->expandAll();
    key_input_->Clear();
    assign_button_->setEnabled(false);
    remove_button_->setEnabled(false);
    currently_assigned_label_->clear();
    selected_command_ = -1;

    config_shortcuts_ = bindings_provider_()->Clone();
}

bool AccelConfig::OnAccept() {
    *bindings_provider_() = std::move(config_shortcuts_);
    config_shortcuts_ = config::Bindings();
    update_shortcut_opts();
    if (MainWindow* frame = qobject_cast<MainWindow*>(parentWidget())) {
        frame->ResetMenuAccelerators();
    }
    return true;
}

QString AccelConfig::AssignedString(int command) const {
    const auto iter = command_to_item_.find(command);
    if (iter == command_to_item_.end()) {
        return config::GetCommandHelper(command);
    }
    return iter->second->data(0, kAssignedStringRole).toString();
}

QString AccelConfig::MessageString(int command) const {
    const auto iter = command_to_item_.find(command);
    if (iter == command_to_item_.end()) {
        return config::GetCommandHelper(command);
    }
    return iter->second->data(0, kMessageStringRole).toString();
}

void AccelConfig::OnCommandSelected() {
    const QList<QTreeWidgetItem*> selected = tree_->selectedItems();
    if (selected.isEmpty() || !selected.front()->data(0, kCommandRole).isValid()) {
        selected_command_ = -1;
        PopulateCurrentKeys();
        return;
    }
    selected_command_ = selected.front()->data(0, kCommandRole).toInt();
    PopulateCurrentKeys();
}

void AccelConfig::OnKeySelected() {
    remove_button_->setEnabled(current_keys_->currentRow() >= 0 &&
                               !current_keys_->selectedItems().isEmpty());
}

void AccelConfig::OnRemoveBinding() {
    QListWidgetItem* item = current_keys_->currentItem();
    if (!item) {
        return;
    }
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString(item->data(Qt::UserRole).toString());
    for (const config::UserInput& input : inputs) {
        config_shortcuts_.UnassignInput(input);
    }
    PopulateCurrentKeys();
}

void AccelConfig::OnResetAll() {
    const auto confirmation = QMessageBox::question(
        this, tr("Confirm"), tr("This will clear all user-defined accelerators. Are you sure?"),
        QMessageBox::Yes | QMessageBox::No);
    if (confirmation != QMessageBox::Yes) {
        return;
    }

    config_shortcuts_ = config::Bindings();
    tree_->clearSelection();
    key_input_->Clear();
    PopulateCurrentKeys();
}

void AccelConfig::OnAssignBinding() {
    const config::UserInput user_input = key_input_->SingleInput();
    if (selected_command_ < 0 || !user_input) {
        return;
    }

    const nonstd::optional<config::Command> old_command =
        config_shortcuts_.CommandForInput(user_input);
    if (old_command != nonstd::nullopt) {
        QString old_command_name;

        // Require user confirmation to override.
        switch (old_command->tag()) {
            case config::Command::Tag::kGame:
                old_command_name = old_command->game().ToUXString();
                break;
            case config::Command::Tag::kShortcut:
                old_command_name = MessageString(old_command->shortcut().id());
                break;
        }

        const auto confirmation = QMessageBox::question(
            this, tr("Confirm"),
            tr("This will unassign \"%1\" from \"%2\". Are you sure?")
                .arg(user_input.ToLocalizedString(), old_command_name),
            QMessageBox::Yes | QMessageBox::No);
        if (confirmation != QMessageBox::Yes) {
            return;
        }
    }

    config_shortcuts_.AssignInputToCommand(user_input,
                                          config::ShortcutCommand(selected_command_));
    PopulateCurrentKeys();
}

void AccelConfig::OnKeyInput() {
    const config::UserInput user_input = key_input_->SingleInput();
    if (!user_input) {
        currently_assigned_label_->clear();
        assign_button_->setEnabled(false);
        return;
    }

    const auto command = config_shortcuts_.CommandForInput(user_input);
    if (!command) {
        // No existing assignment.
        currently_assigned_label_->clear();
    } else {
        // Existing assignment, inform the user.
        switch (command->tag()) {
            case config::Command::Tag::kGame:
                currently_assigned_label_->setText(command->game().ToUXString());
                break;
            case config::Command::Tag::kShortcut:
                currently_assigned_label_->setText(AssignedString(command->shortcut().id()));
                break;
        }
    }

    assign_button_->setEnabled(selected_command_ >= 0);
}

void AccelConfig::PopulateCurrentKeys() {
    const int previous_selection = current_keys_->currentRow();
    current_keys_->clear();

    if (selected_command_ < 0) {
        remove_button_->setEnabled(false);
        assign_button_->setEnabled(false);
        return;
    }

    const config::ShortcutCommand command(selected_command_);

    // Populate `current_keys`.
    int new_keys_count = 0;
    for (const auto& user_input : config_shortcuts_.InputsForCommand(command)) {
        auto* item = new QListWidgetItem(user_input.ToLocalizedString(), current_keys_);
        item->setData(Qt::UserRole, user_input.ToConfigString());
        new_keys_count++;
    }

    // Reset the selection accordingly.
    if (previous_selection < 0 || new_keys_count == 0) {
        current_keys_->setCurrentRow(-1);
        remove_button_->setEnabled(false);
    } else {
        current_keys_->setCurrentRow(std::min(previous_selection, new_keys_count - 1));
        remove_button_->setEnabled(true);
    }
    assign_button_->setEnabled(key_input_->SingleInput().is_valid());
}

}  // namespace dialogs
