#include "qt/config/cmdtab.h"

#include <algorithm>

#include <QCoreApplication>

namespace config {

namespace {

bool g_cmdtab_sorted = false;

void EnsureSorted() {
    if (!g_cmdtab_sorted) {
        SortCmdTab();
    }
}

}  // namespace

void SortCmdTab() {
    std::sort(cmdtab.begin(), cmdtab.end(), [](const cmditem& cmd1, const cmditem& cmd2) {
        return QString::compare(cmd1.cmd, cmd2.cmd) < 0;
    });
    g_cmdtab_sorted = true;
}

QString GetCommandINIEntry(int cmd_id) {
    for (const auto& cmd_item : cmdtab) {
        if (cmd_item.cmd_id == cmd_id) {
            return QStringLiteral("Keyboard/") + cmd_item.cmd;
        }
    }

    // Command not in the table -- degrade gracefully.
    return QString();
}

QString GetCommandHelper(int cmd_id) {
    for (const auto& cmd_item : cmdtab) {
        if (cmd_item.cmd_id == cmd_id) {
            return QCoreApplication::translate("vbam", cmd_item.name);
        }
    }

    // Command not in the table. Degrade gracefully; callers that build
    // bindable lists should gate on IsCommandId() first.
    return QString();
}

QString GetCommandName(int cmd_id) {
    for (const auto& cmd_item : cmdtab) {
        if (cmd_item.cmd_id == cmd_id) {
            return cmd_item.cmd;
        }
    }
    return QString();
}

bool IsCommandId(int cmd_id) {
    for (const auto& cmd_item : cmdtab) {
        if (cmd_item.cmd_id == cmd_id) {
            return true;
        }
    }
    return false;
}

nonstd::optional<int> CommandFromConfigString(const QString& config) {
    EnsureSorted();

    const auto iter = std::lower_bound(
        cmdtab.begin(), cmdtab.end(), config,
        [](const cmditem& item, const QString& value) { return QString::compare(item.cmd, value) < 0; });

    if (iter == cmdtab.end() || iter->cmd != config) {
        return nonstd::nullopt;
    }

    return iter->cmd_id;
}

}  // namespace config
