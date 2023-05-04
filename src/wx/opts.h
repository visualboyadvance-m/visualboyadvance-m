#ifndef WX_OPTS_H
#define WX_OPTS_H

#include <map>

#include <wx/string.h>
#include <wx/vidmode.h>

#include "config/game-control.h"
#include "config/user-input.h"
#include "wxutil.h"

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
    int default_stick = 1;

    /// Keyboard
    wxAcceleratorEntry_v accels;

    /// Core
    int gdb_port = 55555;
    int link_num_players = 2;
    int max_scale = 0;

    /// Sound
    int audio_api = 0;
    // 10 fixes stuttering on mac with openal, as opposed to 5
    // also should be better for modern hardware in general
    int audio_buffers = 10;
    wxString audio_dev;
    int sound_en = 0x30f; // soundSetEnable()
    int gba_sound_filter = 50;
    int gb_echo = 20;
    bool dsound_hw_accel;
    int gb_stereo = 15;
    int sound_qual = 1; // soundSetSampleRate() / gbSoundSetSampleRate()
    int sound_vol = 100; // soundSetVolume()
    bool upmix = false; // xa2 only

    /// Recent
    wxFileHistory* recent = nullptr;

    /// UI Config
    bool hide_menu_bar = true;
    bool suspend_screensaver = false;

    /// wxWindows
    // wxWidgets-generated options (opaque)
} gopts;

extern const wxAcceleratorEntryUnicode default_accels[];
extern const int num_def_accels;

// call to load config (once)
// will write defaults for options not present and delete bad opts
// will also initialize opts[] array translations
void load_opts(bool first_time_launch);
// call whenever opt vars change
// will detect changes and write config if necessary
void update_opts();
// returns true if option name correct; prints error if val invalid
void opt_set(const wxString& name, const wxString& val);

#endif /* WX_OPTS_H */
