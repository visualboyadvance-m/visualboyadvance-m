#include "wx/config/cmdtab.h"

#include <wx/wxcrt.h>

// Initializer for struct cmditem
cmditem new_cmditem(const wxString cmd,
                    const wxString name,
                    int cmd_id,
                    int mask_flags,
                    wxMenuItem* mi) {
    return cmditem {cmd, name, cmd_id, mask_flags, mi};
}

bool cmditem_lt(const struct cmditem& cmd1, const struct cmditem& cmd2) {
    return wxStrcmp(cmd1.cmd, cmd2.cmd) < 0;
}
