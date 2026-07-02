#include "wx/config/cmdtab.h"

#include <algorithm>

#include <wx/wxcrt.h>

#include "core/base/check.h"

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

        // Command not in the table -- degrade gracefully (see GetCommandHelper).
        return wxEmptyString;
    }

    wxString GetCommandHelper(int command) {
        for (const auto& cmd_item : cmdtab) {
            if (cmd_item.cmd_id == command) {
                return cmd_item.name;
            }
        }

        // Command not in the table (e.g. a menu item created with wxID_ANY, which
        // has no command-event entry). Returning via VBAM_NOTREACHED here is
        // undefined behavior in release builds -- the discarded return value
        // yields a corrupt wxString whose later copy crashes. Degrade gracefully.
        // Callers that build bindable lists should gate on IsCommandId() first.
        return wxEmptyString;
    }

    bool IsCommandId(int command) {
        for (const auto& cmd_item : cmdtab) {
            if (cmd_item.cmd_id == command) {
                return true;
            }
        }
        return false;
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
