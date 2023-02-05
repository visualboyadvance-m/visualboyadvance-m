#ifndef WX_OPTS_H
#define WX_OPTS_H

#include <map>

#include <wx/string.h>
#include <wx/vidmode.h>

#include "config/game-control.h"
#include "config/user-input.h"
#include "wx/keyedit.h"

// Forward declaration.
class wxFileHistory;

// Default joystick bindings.
extern const std::map<config::GameControl, std::set<config::UserInput>>
    kDefaultBindings;

extern struct opts_t {
    opts_t();
    // while I would normally put large objects in front to reduce gaps,
    // I instead organized this by opts.cpp table order

    /// Display
    bool bilinear = true;
    wxVideoMode fs_mode;
    int max_threads = 0;
    bool retain_aspect = true;
    bool keep_on_top = false;

    /// GB
    wxString gb_bios;
    bool gb_colorizer_hack = false;
    bool gb_lcd_filter = false;
    wxString gbc_bios;
    bool print_auto_page = true;
    bool print_screen_cap = false;
    wxString gb_rom_dir;
    wxString gbc_rom_dir;

    /// GBA
    wxString gba_bios;
    bool gba_lcd_filter = false;
    bool link_auto = false;
    bool link_hacks = true;
    // quick fix for issues #48 and #445
    wxString link_host = "127.0.0.1";
    wxString server_ip = "*";
    uint32_t link_port = 5738;
    bool link_proto = false;
    int link_timeout = 500;
    int gba_link_type;
    wxString gba_rom_dir;

    /// General
    bool autoload_state = false;
    bool autoload_cheats = false;
    wxString battery_dir;
    long last_update;
    wxString last_updated_filename;
    bool recent_freeze = false;
    wxString recording_dir;
    int rewind_interval = 0;
    wxString scrshot_dir;
    wxString state_dir;
    bool statusbar = false;

    /// Joypad
    std::map<config::GameControl, std::set<config::UserInput>>
        game_control_bindings;
    int autofire_rate = 1;
    int default_stick = 1;

    /// Keyboard
    wxAcceleratorEntry_v accels;

    /// Core
    bool gdb_break_on_load  = false;
    int gdb_port = 55555;
    int link_num_players = 2;
    int max_scale = 0;
    bool use_bios_file_gb = false;
    bool use_bios_file_gba = false;
    bool use_bios_file_gbc = false;
    bool vsync = false;

    /// Sound
    int audio_api = 0;
    // 10 fixes stuttering on mac with openal, as opposed to 5
    // also should be better for modern hardware in general
    int audio_buffers = 10;
    wxString audio_dev;
    int sound_en = 0x30f; // soundSetEnable()
    int gba_sound_filter = 50;
    bool soundInterpolation;
    bool gb_declick = true;
    int gb_echo = 20;
    bool gb_effects_config_enabled;
    bool dsound_hw_accel;
    int gb_stereo = 15;
    bool gb_effects_config_surround;
    int sound_qual = 1; // soundSetSampleRate() / gbSoundSetSampleRate()
    int sound_vol = 100; // soundSetVolume()
    bool upmix = false; // xa2 only

    /// Recent
    wxFileHistory* recent = nullptr;

    /// UI Config
    bool hide_menu_bar = true;

    /// wxWindows
    // wxWidgets-generated options (opaque)
} gopts;

extern const wxAcceleratorEntryUnicode default_accels[];
extern const int num_def_accels;

// call to load config (once)
// will write defaults for options not present and delete bad opts
// will also initialize opts[] array translations
void load_opts();
// call whenever opt vars change
// will detect changes and write config if necessary
void update_opts();
// returns true if option name correct; prints error if val invalid
void opt_set(const wxString& name, const wxString& val);

#endif /* WX_OPTS_H */
