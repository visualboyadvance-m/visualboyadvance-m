#include "wx/config/cmdtab.h"

#include <algorithm>

#include <wx/wxcrt.h>

// Initializer for struct cmditem
cmditem new_cmditem(const wxString cmd,
                    const wxString name,
                    int cmd_id,
                    int mask_flags,
                    wxMenuItem* mi) {
    return cmditem {cmd, name, cmd_id, mask_flags, mi};
}

namespace config {
    wxString GetCommandINIEntry(int command) {
        for (const auto& cmd_item : cmdtab) {
            if (cmd_item.cmd_id == command) {
                return wxString::Format("Keyboard/%s", cmd_item.cmd);
            }
        }

        // Command not found. This should never happen.
        assert(false);
        return wxEmptyString;
    }

    wxString GetCommandHelper(int command) {
        for (const auto& cmd_item : cmdtab) {
            if (cmd_item.cmd_id == command) {
                return cmd_item.name;
            }
        }

        // Command not found. This should never happen.
        assert(false);
        return wxEmptyString;
    }

    nonstd::optional<int> CommandFromConfigString(const wxString& config) {
        const cmditem dummy = new_cmditem(config);
        const auto iter = std::lower_bound(cmdtab.begin(), cmdtab.end(), dummy, [](const cmditem& cmd1, const cmditem& cmd2) {
            return wxStrcmp(cmd1.cmd, cmd2.cmd) < 0;
        });

        if (iter == cmdtab.end() || iter->cmd != config) {
            return nonstd::nullopt;
        }

        return iter->cmd_id;
    }
}  // namespace config
