#ifndef VBAM_WX_OPTS_H_
#define VBAM_WX_OPTS_H_

#include <map>

#include <wx/string.h>
#include <wx/vidmode.h>

#include "wx/config/game-control.h"
#include "wx/config/shortcuts.h"
#include "wx/config/user-input.h"

// Forward declaration.
class wxFileHistory;

// Default joystick bindings.
extern const std::map<config::GameControl, std::set<config::UserInput>>
    kDefaultBindings;

extern struct opts_t {
    opts_t();

    /// Display
    wxVideoMode fs_mode;

    /// GBA
    wxString gba_bios;
    // quick fix for issues #48 and #445
    wxString link_host = "127.0.0.1";
    wxString server_ip = "*";
    uint32_t link_port = 5738;
    int link_timeout = 500;
    int gba_link_type;

    /// General
    int rewind_interval = 0;

    /// Joypad
    std::map<config::GameControl, std::set<config::UserInput>>
        game_control_bindings;
    int autofire_rate = 1;

    /// Keyboard
    config::Shortcuts shortcuts;

    /// Core
    int gdb_port = 55555;
    int link_num_players = 2;
    int max_scale = 0;

    /// Sound
    int sound_en = 0x30f; // soundSetEnable()

    /// Recent
    wxFileHistory* recent = nullptr;

    /// UI Config
    bool hide_menu_bar = true;
    bool suspend_screensaver = false;

    /// wxWindows
    // wxWidgets-generated options (opaque)
} gopts;

// call to load config (once)
// will write defaults for options not present and delete bad opts
// will also initialize opts[] array translations
void load_opts(bool first_time_launch);
// call whenever opt vars change
// will detect changes and write config if necessary
void update_opts();
// Updates the joypad options.
void update_joypad_opts();
// Updates the shortcut options.
void update_shortcut_opts();
// returns true if option name correct; prints error if val invalid
void opt_set(const wxString& name, const wxString& val);

#endif // VBAM_WX_OPTS_H_
