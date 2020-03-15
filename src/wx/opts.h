#ifndef WX_OPTS_H
#define WX_OPTS_H

#include <vector>

#define NUM_KEYS 21
extern const wxString joynames[NUM_KEYS];
extern wxJoyKeyBinding defkeys_keyboard[NUM_KEYS];  // keyboard defaults

extern std::vector<std::vector<wxJoyKeyBinding>> defkeys_joystick;  // joystick defaults

extern struct opts_t {
    opts_t();
    // while I would normally put large objects in front to reduce gaps,
    // I instead organized this by opts.cpp table order

    /// Display
    bool bilinear;
    int filter;
    wxString filter_plugin;
    int ifb;
    wxVideoMode fs_mode;
    int max_threads;
    int render_method;
    double video_scale;
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
    wxJoyKeyBinding_v joykey_bindings[4][NUM_KEYS];
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

    /// wxWindows
    // wxWidgets-generated options (opaque)
} gopts;

extern struct opt_desc {
    wxString opt;
    const char* cmd;
    wxString desc;
    wxString* stropt;
    int* intopt;
    wxString enumvals;
    double min, max;
    bool* boolopt;
    double* doubleopt;
    uint32_t* uintopt;
    // current configured value
    wxString curstr;
    int curint;
    double curdouble;
    uint32_t curuint;
#define curbool curint
} opts[];

// Initializer for struct opt_desc
opt_desc new_opt_desc(wxString opt = wxT(""), const char* cmd = NULL, wxString desc = wxT(""),
                      wxString* stropt = NULL, int* intopt = NULL, wxString enumvals = wxT(""),
                      double min = 0, double max = 0, bool* boolopt = NULL,
                      double* doubleopt = NULL, uint32_t* uintopt = NULL, wxString curstr = wxT(""),
                      int curint = 0, double curdouble = 0, uint32_t curuint = 0);

extern const int num_opts;

extern const wxAcceleratorEntry default_accels[];
extern const int num_def_accels;

// call to setup default keys.
void set_default_keys();
// call to load config (once)
// will write defaults for options not present and delete bad opts
// will also initialize opts[] array translations
void load_opts();
// call whenever opt vars change
// will detect changes and write config if necessary
void update_opts();
// returns true if option name correct; prints error if val invalid
bool opt_set(const wxString& name, const wxString& val);

#endif /* WX_OPTS_H */
