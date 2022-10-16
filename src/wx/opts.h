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
    bool bilinear;
    wxVideoMode fs_mode;
    int max_threads;
    bool retain_aspect;
    bool keep_on_top;

    /// GB
    wxString gb_bios;
    wxString gbc_bios;
    // uint16_t systemGbPalette[8*3];
    bool print_auto_page, print_screen_cap;
    wxString gb_rom_dir;
    wxString gbc_rom_dir;

    /// GBA
    wxString gba_bios;
    int gba_link_type;
    wxString link_host;
    wxString server_ip;
    uint32_t link_port;
    int link_proto;
    bool link_auto;
    wxString gba_rom_dir;

    /// General
    bool autoload_state, autoload_cheats;
    wxString battery_dir;
    long last_update;
    wxString last_updated_filename;
    bool recent_freeze;
    wxString recording_dir;
    int rewind_interval;
    wxString scrshot_dir;
    wxString state_dir;
    int statusbar;

    /// Joypad
    std::map<config::GameControl, std::set<config::UserInput>>
        game_control_bindings;
    int autofire_rate;
    int default_stick;

    /// Keyboard
    wxAcceleratorEntry_v accels;

    /// Sound
    int audio_api;
    int audio_buffers;
    wxString audio_dev;
    int sound_en; // soundSetEnable()
    int gba_sound_filter;
    bool soundInterpolation;
    bool gb_declick;
    int gb_echo;
    bool gb_effects_config_enabled;
    bool dsound_hw_accel;
    int gb_stereo;
    bool gb_effects_config_surround;
    int sound_qual; // soundSetSampleRate() / gbSoundSetSampleRate()
    int sound_vol; // soundSetVolume()
    bool upmix; // xa2 only

    /// Recent
    wxFileHistory* recent;

    /// UI Config
    bool hide_menu_bar;

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
