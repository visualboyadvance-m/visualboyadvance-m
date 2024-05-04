#ifndef VBAM_WX_CONFIG_CMDTAB_H_
#define VBAM_WX_CONFIG_CMDTAB_H_

#include <vector>

#include <wx/string.h>

// Forward declaration.
class wxMenuItem;

// List of all commands with their descriptions
// sorted by cmd field for binary searching
// filled in by copy-events.cmake
struct cmditem {
    const wxString cmd;
    const wxString name;
    int cmd_id;
    int mask_flags;  // if non-0, one of the flags must be turned on in win
    // to enable this command
    wxMenuItem* mi;  // the menu item to invoke command, if present
};

extern std::vector<cmditem> cmdtab;

// Initializer for struct cmditem
cmditem new_cmditem(const wxString cmd = "",
                    const wxString name = "",
                    int cmd_id = 0,
                    int mask_flags = 0,
                    wxMenuItem* mi = nullptr);

// for binary search
bool cmditem_lt(const struct cmditem& cmd1, const struct cmditem& cmd2);

// here are those conditions
enum { CMDEN_GB = (1 << 0), // GB ROM loaded
    CMDEN_GBA = (1 << 1), // GBA ROM loaded
    // the rest imply the above, unless:
    //   _ANY -> does not imply either
    //   _GBA -> only implies GBA
    CMDEN_REWIND = (1 << 2), // rewind states available
    CMDEN_SREC = (1 << 3), // sound recording in progress
    CMDEN_NSREC = (1 << 4), // no sound recording
    CMDEN_VREC = (1 << 5), // video recording
    CMDEN_NVREC = (1 << 6), // no video recording
    CMDEN_GREC = (1 << 7), // game recording
    CMDEN_NGREC = (1 << 8), // no game recording
    CMDEN_GPLAY = (1 << 9), // game playback
    CMDEN_NGPLAY = (1 << 10), // no game playback
    CMDEN_SAVST = (1 << 11), // any save states
    CMDEN_GDB = (1 << 12), // gdb connected
    CMDEN_NGDB_GBA = (1 << 13), // gdb not connected
    CMDEN_NGDB_ANY = (1 << 14), // gdb not connected
    CMDEN_NREC_ANY = (1 << 15), // not a/v recording
    CMDEN_LINK_ANY = (1 << 16), // link enabled

    CMDEN_NEVER = (1 << 31) // never (for NOOP)
};
#define ONLOAD_CMDEN (CMDEN_NSREC | CMDEN_NVREC | CMDEN_NGREC | CMDEN_NGPLAY)
#define UNLOAD_CMDEN_KEEP (CMDEN_NGDB_ANY | CMDEN_NREC_ANY | CMDEN_LINK_ANY)

#endif  // VBAM_WX_CONFIG_CMDTAB_H_
