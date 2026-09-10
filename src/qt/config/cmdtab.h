#ifndef VBAM_QT_CONFIG_CMDTAB_H_
#define VBAM_QT_CONFIG_CMDTAB_H_

#include <vector>

#include <optional.hpp>

#include <QString>

#include "qt/cmd-ids.h"

// Forward declaration.
class QAction;

// List of all commands with their descriptions. Sorted by cmd field (see
// SortCmdTab()) for binary searching. Filled in by cmdtab-data.cpp.
struct cmditem {
    QString cmd;       // config name, e.g. "OPEN", "LoadGame01"
    const char* name;  // untranslated helper/menu text, e.g. "Open ROM..."
    int cmd_id;        // cmd::Id
    int mask_flags;    // if non-0, one of the flags must be turned on in win
                       // to enable this command
    QAction* action;   // the menu action invoking this command, if present
};

extern std::vector<cmditem> cmdtab;

namespace config {
    // Sorts cmdtab by config name. Must be called once at startup before
    // CommandFromConfigString().
    void SortCmdTab();

    // Returns the command INI entry name for the given command ID, i.e. the
    // command name prefixed with "Keyboard/". Empty string if unknown.
    // Examples:
    // * cmd::kOpen -> "Keyboard/OPEN"
    // * cmd::kNoop -> "Keyboard/NOOP"
    QString GetCommandINIEntry(int cmd_id);

    // Returns the (translated) command helper string for the given command ID.
    // Empty string if unknown.
    QString GetCommandHelper(int cmd_id);

    // Returns the command config name (no "Keyboard/" prefix) for `cmd_id`.
    QString GetCommandName(int cmd_id);

    // Returns the command ID for the given command config name, without the
    // "Keyboard/" prefix. Examples:
    // * "OPEN" -> cmd::kOpen
    // * "Keyboard/OPEN" -> nonstd::nullopt
    // * "NOOP" -> cmd::kNoop
    nonstd::optional<int> CommandFromConfigString(const QString& config);

    // True if `cmd_id` is a known, bindable command (present in cmdtab).
    bool IsCommandId(int cmd_id);
}

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
    CMDEN_LINK_OFF = (1 << 17), // no link session active

    CMDEN_NEVER = (1 << 31) // never (for NOOP)
};
#define ONLOAD_CMDEN (CMDEN_NSREC | CMDEN_NVREC | CMDEN_NGREC | CMDEN_NGPLAY)
#define UNLOAD_CMDEN_KEEP (CMDEN_NGDB_ANY | CMDEN_NREC_ANY | CMDEN_LINK_ANY | CMDEN_LINK_OFF)

#endif  // VBAM_QT_CONFIG_CMDTAB_H_
