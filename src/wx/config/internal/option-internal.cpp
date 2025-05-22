#include "wx/config/option.h"

// Helper implementation file to define and compile all of these huge constants
// separately. These should not be updated very often, so having these in a
// separate file improves incremental build time.

#include <algorithm>
#include <limits>
#include <map>

#include <wx/log.h>
#include <wx/translation.h>

#include "core/base/check.h"
#include "core/base/system.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaSound.h"
#include "wx/opts.h"

#define VBAM_OPTION_INTERNAL_INCLUDE
#include "wx/config/internal/option-internal.h"
#undef VBAM_OPTION_INTERNAL_INCLUDE

struct CoreOptions coreOptions;

namespace config {

namespace {

// clang-format off
// These MUST follow the same order as the definitions of the enum.
// Adding an option without adding to this array will result in a compiler
// error since kNbFilters is automatically updated.
static const std::array<wxString, kNbFilters> kFilterStrings = {
    "none",
    "2xsai",
    "super2xsai",
    "supereagle",
    "pixelate",
    "advmame",
    "bilinear",
    "bilinearplus",
    "scanlines",
    "tvmode",
    "hq2x",
    "lq2x",
    "simple2x",
    "simple3x",
    "hq3x",
    "simple4x",
    "hq4x",
    "xbrz2x",
    "xbrz3x",
    "xbrz4x",
    "xbrz5x",
    "xbrz6x",
    "plugin",
};

// These MUST follow the same order as the definitions of the enum.
// Adding an option without adding to this array will result in a compiler
// error since kNbInterframes is automatically updated.
static const std::array<wxString, kNbInterframes> kInterframeStrings = {
    "none",
    "smart",
    "motionblur",
};

// These MUST follow the same order as the definitions of the enum.
// Adding an option without adding to this array will result in a compiler
// error since kNbRenderMethods is automatically updated.
static const std::array<wxString, kNbRenderMethods> kRenderMethodStrings = {
    "simple",
    "opengl",
    "sdl_video",
#if defined(__WXMSW__) && !defined(NO_D3D)
    "direct3d",
#elif defined(__WXMAC__)
    "quartz2d",
#ifndef NO_METAL
    "metal",
#endif
#endif
};

// These MUST follow the same order as the definitions of the enum above.
// Adding an option without adding to this array will result in a compiler
// error since kNbAudioApis is automatically updated.
static const std::array<wxString, kNbAudioApis> kAudioApiStrings = {
#if defined(VBAM_ENABLE_OPENAL)
    "openal",
#endif
    "sdl_audio",
#if defined(__WXMSW__)
    "directsound",
#endif
#if defined(VBAM_ENABLE_XAUDIO2)
    "xaudio2",
#endif
#if defined(VBAM_ENABLE_FAUDIO)
    "faudio",
#endif
#if defined(__WXMAC__)
    "coreaudio",
#endif
};

// These MUST follow the same order as the definitions of the enum above.
// Adding an option without adding to this array will result in a compiler
// error since kNbSoundQualities is automatically updated.
static const std::array<wxString, kNbSoundRate> kAudioRateStrings = {
    "48",
    "44",
    "22",
    "11",
};
// clang-format on

// Builds the "all enum values" string for a given array defined above.
template <std::size_t SIZE>
wxString AllEnumValuesForArray(const std::array<wxString, SIZE>& input) {
    // 15 characters per option is a reasonable higher bound. Probably.
    static constexpr size_t kMaxOptionLength = 15u;

    wxString all_options;
    all_options.reserve(kMaxOptionLength * SIZE);

    std::for_each(input.cbegin(), input.cend(),
                  [&all_options](const auto& elt) {
                      all_options.append(elt);
                      all_options.append('|');
                  });
    // Remove last value
    all_options.RemoveLast(1u);
    return all_options;
}

}  // namespace

// static
std::array<Option, kNbOptions>& Option::All() {
    struct OwnedOptions {
        /// Display
        bool bilinear = true;
        Filter filter = Filter::kNone;
        wxString filter_plugin = wxEmptyString;
        Interframe interframe = Interframe::kNone;
        bool keep_on_top = false;
        int32_t max_threads = 0;

#if defined(__WXMAC__) && !defined(NO_METAL)
        RenderMethod render_method = RenderMethod::kMetal;
#else
#if defined(NO_OGL)
        //RenderMethod render_method = RenderMethod::kSimple;
        RenderMethod render_method = RenderMethod::kSDL;
#else
        RenderMethod render_method = RenderMethod::kOpenGL;
#endif
#endif
        double video_scale = 3;
        bool retain_aspect = true;

        /// GB
        wxString gb_bios = wxEmptyString;
        bool colorizer_hack = false;
        bool gb_lcd_filter = false;
        wxString gbc_bios = wxEmptyString;
        bool print_auto_page = true;
        bool print_screen_cap = false;
        wxString gb_rom_dir = wxEmptyString;
        wxString gbc_rom_dir = wxEmptyString;

        /// GBA
        bool gba_lcd_filter = false;
#ifndef NO_LINK
        bool link_auto = false;
        bool link_hacks = true;
        bool link_proto = false;
#endif
        wxString gba_rom_dir;

        /// Core
        bool agb_print = false;
        bool auto_frame_skip = false;
        bool auto_patch = true;
        bool autoload_cheats = false;
        uint32_t capture_format = 0;
        bool disable_status_messages = false;
        uint32_t flash_size = 0;
        int32_t frame_skip = 0;
        bool gdb_break_on_load  = false;
        bool pause_when_inactive = false;
        uint32_t show_speed = 0;
        bool show_speed_transparent = false;
        bool use_bios_file_gb = false;
        bool use_bios_file_gba = false;
        bool use_bios_file_gbc = false;
        bool vsync = false;

        /// General
        bool autoload_state = false;
        wxString battery_dir = wxEmptyString;
        bool recent_freeze = false;
        wxString recording_dir = wxEmptyString;
        wxString screenshot_dir = wxEmptyString;
        wxString state_dir = wxEmptyString;
        bool statusbar = false;
        uint32_t ini_version = kIniLatestVersion;

        /// Joypad
        uint32_t default_stick = 1;
        bool sdl_game_controller_mode = true;

        /// Geometry
        bool fullscreen = false;
        bool window_maximized = false;
        uint32_t window_height = 0;
        uint32_t window_width = 0;
        int32_t window_pos_x = -1;
        int32_t window_pos_y = -1;

        /// UI
        bool allow_keyboard_background_input = false;
        bool allow_joystick_background_input = true;

        /// Sound
#if defined(__WXMAC__)
        AudioApi audio_api = AudioApi::kCoreAudio;
#elif defined(VBAM_ENABLE_FAUDIO)
        AudioApi audio_api = AudioApi::kFAudio;
#elif defined(VBAM_ENABLE_XAUDIO2)
        AudioApi audio_api = AudioApi::kXAudio2;
#elif defined(VBAM_ENABLE_OPENAL)
        AudioApi audio_api = AudioApi::kOpenAL;
#else
        AudioApi audio_api = AudioApi::kSDL;
#endif
        wxString audio_dev;
        // 10 fixes stuttering on mac with openal, as opposed to 5
        // also should be better for modern hardware in general
        int32_t audio_buffers = 10;
        int32_t gba_sound_filtering = 50;
        bool gb_declicking = true;
        int32_t gb_echo = 20;
        bool gb_effects_config_enabled = false;
        int32_t gb_stereo = 15;
        bool gb_effects_config_surround = false;
        AudioRate sound_quality = AudioRate::k44kHz;
        bool dsound_hw_accel = false;
        bool upmix = false;
        int32_t volume = 100;
        uint32_t bitdepth = 3;
        wxString sdlrenderer = wxString("default");
    };
    static OwnedOptions g_owned_opts;

    // These MUST follow the same order as the definitions in OptionID.
    // Adding an option without adding to this array will result in a compiler
    // error since kNbOptions is automatically updated.
    // This will be initialized on the first call, in load_opts(), ensuring the
    // translation initialization has already happened.
    // clang-format off
    static std::array<Option, kNbOptions> g_all_opts = {
        /// Display
        Option(OptionID::kDispBilinear, &g_owned_opts.bilinear),
        Option(OptionID::kDispFilter, &g_owned_opts.filter),
        Option(OptionID::kDispFilterPlugin, &g_owned_opts.filter_plugin),
        Option(OptionID::kDispIFB, &g_owned_opts.interframe),
        Option(OptionID::kBitDepth, &g_owned_opts.bitdepth, 0, 3),
        Option(OptionID::kDispKeepOnTop, &g_owned_opts.keep_on_top),
        Option(OptionID::kDispMaxThreads, &g_owned_opts.max_threads, 0, 256),
        Option(OptionID::kDispRenderMethod, &g_owned_opts.render_method),
        Option(OptionID::kDispScale, &g_owned_opts.video_scale, 1, 6),
        Option(OptionID::kDispStretch, &g_owned_opts.retain_aspect),
        Option(OptionID::kSDLRenderer, &g_owned_opts.sdlrenderer),

        /// GB
        Option(OptionID::kGBBiosFile, &g_owned_opts.gb_bios),
        Option(OptionID::kGBColorOption, &gbColorOption),
        Option(OptionID::kGBColorizerHack, &g_owned_opts.colorizer_hack),
        Option(OptionID::kGBLCDFilter, &g_owned_opts.gb_lcd_filter),
        Option(OptionID::kGBGBCBiosFile, &g_owned_opts.gbc_bios),
        Option(OptionID::kGBPalette0, systemGbPalette),
        Option(OptionID::kGBPalette1, systemGbPalette + 8),
        Option(OptionID::kGBPalette2, systemGbPalette + 16),
        Option(OptionID::kGBPrintAutoPage, &g_owned_opts.print_auto_page),
        Option(OptionID::kGBPrintScreenCap, &g_owned_opts.print_screen_cap),
        Option(OptionID::kGBROMDir, &g_owned_opts.gb_rom_dir),
        Option(OptionID::kGBGBCROMDir, &g_owned_opts.gbc_rom_dir),

        /// GBA
        Option(OptionID::kGBABiosFile, &gopts.gba_bios),
        Option(OptionID::kGBALCDFilter, &g_owned_opts.gba_lcd_filter),
#ifndef NO_LINK
        Option(OptionID::kGBALinkAuto, &g_owned_opts.link_auto),
        Option(OptionID::kGBALinkFast, &g_owned_opts.link_hacks),
        Option(OptionID::kGBALinkHost, &gopts.link_host),
        Option(OptionID::kGBAServerIP, &gopts.server_ip),
        Option(OptionID::kGBALinkPort, &gopts.link_port, 0, 65535),
        Option(OptionID::kGBALinkProto, &g_owned_opts.link_proto),
        Option(OptionID::kGBALinkTimeout, &gopts.link_timeout, 0, 9999999),
        Option(OptionID::kGBALinkType, &gopts.gba_link_type, 0, 5),
#endif
        Option(OptionID::kGBAROMDir, &g_owned_opts.gba_rom_dir),

        /// General
        Option(OptionID::kGenAutoLoadLastState, &g_owned_opts.autoload_state),
        Option(OptionID::kGenBatteryDir, &g_owned_opts.battery_dir),
        Option(OptionID::kGenFreezeRecent, &g_owned_opts.recent_freeze),
        Option(OptionID::kGenRecordingDir, &g_owned_opts.recording_dir),
        Option(OptionID::kGenRewindInterval, &gopts.rewind_interval, 0, 600),
        Option(OptionID::kGenScreenshotDir, &g_owned_opts.screenshot_dir),
        Option(OptionID::kGenStateDir, &g_owned_opts.state_dir),
        Option(OptionID::kGenStatusBar, &g_owned_opts.statusbar),
        Option(OptionID::kGenIniVersion, &g_owned_opts.ini_version, 0, std::numeric_limits<uint32_t>::max()),

        /// Joypad
        Option(OptionID::kJoy),
        Option(OptionID::kJoyAutofireThrottle, &gopts.autofire_rate, 1, 1000),
        Option(OptionID::kJoyDefault, &g_owned_opts.default_stick, 1, 4),
        Option(OptionID::kSDLGameControllerMode, &g_owned_opts.sdl_game_controller_mode),

        /// Keyboard
        Option(OptionID::kKeyboard),

        /// Core
        Option(OptionID::kPrefAgbPrint, &g_owned_opts.agb_print),
        Option(OptionID::kPrefAutoFrameSkip, &g_owned_opts.auto_frame_skip),
        Option(OptionID::kPrefAutoPatch, &g_owned_opts.auto_patch),
        Option(OptionID::kPrefAutoSaveLoadCheatList, &g_owned_opts.autoload_cheats),
        Option(OptionID::kPrefBorderAutomatic, &gbBorderAutomatic),
        Option(OptionID::kPrefBorderOn, &gbBorderOn),
        Option(OptionID::kPrefCaptureFormat, &g_owned_opts.capture_format, 0, 1),
        Option(OptionID::kPrefCheatsEnabled, &coreOptions.cheatsEnabled, 0, 1),
        Option(OptionID::kPrefDisableStatus, &g_owned_opts.disable_status_messages),
        Option(OptionID::kPrefEmulatorType, &gbEmulatorType, 0, 5),
        Option(OptionID::kPrefFlashSize, &g_owned_opts.flash_size, 0, 1),
        Option(OptionID::kPrefFrameSkip, &g_owned_opts.frame_skip, -1, 9),
        Option(OptionID::kPrefGBPaletteOption, &gbPaletteOption, 0, 2),
        Option(OptionID::kPrefGBPrinter, &coreOptions.gbPrinterEnabled, 0, 1),
        Option(OptionID::kPrefGDBBreakOnLoad, &g_owned_opts.gdb_break_on_load),
        Option(OptionID::kPrefGDBPort, &gopts.gdb_port, 0, 65535),
#ifndef NO_LINK
        Option(OptionID::kPrefLinkNumPlayers, &gopts.link_num_players, 2, 4),
#endif
        Option(OptionID::kPrefMaxScale, &gopts.max_scale, 0, 100),
        Option(OptionID::kPrefPauseWhenInactive, &g_owned_opts.pause_when_inactive),
        Option(OptionID::kPrefRTCEnabled, &coreOptions.rtcEnabled, 0, 1),
        Option(OptionID::kPrefSaveType, &coreOptions.cpuSaveType, 0, 5),
        Option(OptionID::kPrefShowSpeed, &g_owned_opts.show_speed, 0, 2),
        Option(OptionID::kPrefShowSpeedTransparent, &g_owned_opts.show_speed_transparent),
        Option(OptionID::kPrefSkipBios, &coreOptions.skipBios),
        Option(OptionID::kPrefSkipSaveGameCheats, &coreOptions.skipSaveGameCheats, 0, 1),
        Option(OptionID::kPrefSkipSaveGameBattery, &coreOptions.skipSaveGameBattery, 0, 1),
        Option(OptionID::kPrefThrottle, &coreOptions.throttle, 0, 450),
        Option(OptionID::kPrefSpeedupThrottle, &coreOptions.speedup_throttle, 0, 450),
        Option(OptionID::kPrefSpeedupFrameSkip, &coreOptions.speedup_frame_skip, 0, 40),
        Option(OptionID::kPrefSpeedupThrottleFrameSkip, &coreOptions.speedup_throttle_frame_skip),
        Option(OptionID::kPrefSpeedupMute, &coreOptions.speedup_mute),
        Option(OptionID::kPrefUseBiosGB, &g_owned_opts.use_bios_file_gb),
        Option(OptionID::kPrefUseBiosGBA, &g_owned_opts.use_bios_file_gba),
        Option(OptionID::kPrefUseBiosGBC, &g_owned_opts.use_bios_file_gbc),
        Option(OptionID::kPrefVsync, &g_owned_opts.vsync),

        /// Geometry
        Option(OptionID::kGeomFullScreen, &g_owned_opts.fullscreen),
        Option(OptionID::kGeomIsMaximized, &g_owned_opts.window_maximized),
        Option(OptionID::kGeomWindowHeight, &g_owned_opts.window_height, 0, 99999),
        Option(OptionID::kGeomWindowWidth, &g_owned_opts.window_width, 0, 99999),
        Option(OptionID::kGeomWindowX, &g_owned_opts.window_pos_x, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()),
        Option(OptionID::kGeomWindowY, &g_owned_opts.window_pos_y, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()),

        /// UI
        Option(OptionID::kUIAllowKeyboardBackgroundInput, &g_owned_opts.allow_keyboard_background_input),
        Option(OptionID::kUIAllowJoystickBackgroundInput, &g_owned_opts.allow_joystick_background_input),
        Option(OptionID::kUIHideMenuBar, &gopts.hide_menu_bar),
        Option(OptionID::kUISuspendScreenSaver, &gopts.suspend_screensaver),

        /// Sound
        Option(OptionID::kSoundAudioAPI, &g_owned_opts.audio_api),
        Option(OptionID::kSoundAudioDevice, &g_owned_opts.audio_dev),
        Option(OptionID::kSoundBuffers, &g_owned_opts.audio_buffers, 2, 10),
        Option(OptionID::kSoundEnable, &gopts.sound_en, 0, 0x30f),
        Option(OptionID::kSoundGBAFiltering, &g_owned_opts.gba_sound_filtering, 0, 100),
        Option(OptionID::kSoundGBAInterpolation, &g_gbaSoundInterpolation),
        Option(OptionID::kSoundGBDeclicking, &g_owned_opts.gb_declicking),
        Option(OptionID::kSoundGBEcho, &g_owned_opts.gb_echo, 0, 100),
        Option(OptionID::kSoundGBEnableEffects, &g_owned_opts.gb_effects_config_enabled),
        Option(OptionID::kSoundGBStereo, &g_owned_opts.gb_stereo, 0, 100),
        Option(OptionID::kSoundGBSurround, &g_owned_opts.gb_effects_config_surround),
        Option(OptionID::kSoundAudioRate, &g_owned_opts.sound_quality),
        Option(OptionID::kSoundDSoundHWAccel, &g_owned_opts.dsound_hw_accel),
        Option(OptionID::kSoundUpmix, &g_owned_opts.upmix),
        Option(OptionID::kSoundVolume, &g_owned_opts.volume, 0, 200),
    };
    // clang-format on
    return g_all_opts;
}

namespace internal {

// These MUST follow the same order as the definitions in OptionID.
// Adding an option without adding to this array will result in a compiler
// error since kNbOptions is automatically updated.
const std::array<OptionData, kNbOptions + 1> kAllOptionsData = {
    /// Display
    OptionData{"Display/Bilinear", "Bilinear", _("Use bilinear filter with 3d renderer")},
    OptionData{"Display/Filter", "", _("Full-screen filter to apply")},
    OptionData{"Display/FilterPlugin", "", _("Filter plugin library")},
    OptionData{"Display/IFB", "", _("Interframe blending function")},
    OptionData{"Display/BitDepth", "BitDepth", _("Bit depth")},
    OptionData{"Display/KeepOnTop", "KeepOnTop", _("Keep window on top")},
    OptionData{"Display/MaxThreads", "Multithread",
               _("Maximum number of threads to run filters in")},
    OptionData{"Display/RenderMethod", "",
               _("Render method; if unsupported, simple method will be used")},
    OptionData{"Display/Scale", "", _("Default scale factor")},
    OptionData{"Display/Stretch", "RetainAspect", _("Retain aspect ratio when resizing")},
    OptionData{"Display/SDLRenderer", "", _("SDL renderer")},

    /// GB
    OptionData{"GB/BiosFile", "", _("BIOS file to use for Game Boy, if enabled")},
    OptionData{"GB/ColorOption", "GBColorOption", _("Game Boy color enhancement, if enabled")},
    OptionData{"GB/ColorizerHack", "ColorizerHack", _("Enable DX Colorization Hacks")},
    OptionData{"GB/LCDFilter", "GBLcdFilter", _("Apply LCD filter, if enabled")},
    OptionData{"GB/GBCBiosFile", "", _("BIOS file to use for Game Boy Color, if enabled")},
    OptionData{"GB/Palette0", "",
               _("The default palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555).")},
    OptionData{"GB/Palette1", "",
               _("The first user palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555).")},
    OptionData{"GB/Palette2", "",
               _("The second user palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555).")},
    OptionData{"GB/PrintAutoPage", "PrintGather",
               _("Automatically gather a full page before printing")},
    OptionData{"GB/PrintScreenCap", "PrintSnap",
               _("Automatically save printouts as screen captures with -print "
                 "suffix")},
    OptionData{"GB/ROMDir", "", _("Directory to look for ROM files")},
    OptionData{"GB/GBCROMDir", "", _("Directory to look for Game Boy Color ROM files")},

    /// GBA
    OptionData{"GBA/BiosFile", "", _("BIOS file to use, if enabled")},
    OptionData{
        "GBA/LCDFilter",
        "GBALcdFilter",
        _("Apply LCD filter, if enabled"),
    },
#ifndef NO_LINK
    OptionData{
        "GBA/LinkAuto",
        "LinkAuto",
        _("Enable link at boot"),
    },
    OptionData{
        "GBA/LinkFast",
        "SpeedOn",
        _("Enable faster network protocol by default"),
    },
    OptionData{"GBA/LinkHost", "", _("Default network link client host")},
    OptionData{"GBA/ServerIP", "", _("Default network link server IP to bind")},
    OptionData{"GBA/LinkPort", "", _("Default network link port (server and client)")},
    OptionData{"GBA/LinkProto", "LinkProto", _("Default network protocol")},
    OptionData{"GBA/LinkTimeout", "LinkTimeout", _("Link timeout (ms)")},
    OptionData{"GBA/LinkType", "LinkType", _("Link cable type")},
#endif
    OptionData{"GBA/ROMDir", "", _("Directory to look for ROM files")},

    /// General
    OptionData{"General/AutoLoadLastState", "", _("Automatically load last saved state")},
    OptionData{"General/BatteryDir", "",
               _("Directory to store game save files (relative paths are "
                 "relative to ROM; blank is config dir)")},
    OptionData{"General/FreezeRecent", "", _("Freeze recent load list")},
    OptionData{"General/RecordingDir", "",
               _("Directory to store A / V and game recordings (relative paths "
                 "are relative to ROM)")},
    OptionData{"General/RewindInterval", "",
               _("Number of seconds between rewind snapshots (0 to disable)")},
    OptionData{"General/ScreenshotDir", "",
               _("Directory to store screenshots (relative paths are relative "
                 "to ROM)")},
    OptionData{"General/StateDir", "",
               _("Directory to store saved state files (relative paths are "
                 "relative to BatteryDir)")},
    OptionData{"General/StatusBar", "StatusBar", _("Enable status bar")},
    OptionData{"General/IniVersion", "", _("INI file version (DO NOT MODIFY)")},

    /// Joypad
    OptionData{"Joypad/*/*", "",
               _("The parameter Joypad/<n>/<button> contains a comma-separated "
                 "list of key names which map to joypad #<n> button <button>. "
                 "Button is one of Up, Down, Left, Right, A, B, L, R, Select, "
                 "Start, MotionUp, MotionDown, MotionLeft, MotionRight, AutoA, "
                 "AutoB, Speed, Capture, GS")},
    OptionData{"Joypad/AutofireThrottle", "", _("The autofire toggle period, in frames (1/60 s)")},
    OptionData{"Joypad/Default", "", _("The number of the stick to use in single-player mode")},
    OptionData{"Joypad/SDLGameControllerMode", "SDLGameControllerMode",
               _("Whether to enable SDL GameController mode")},

    /// Keyboard
    OptionData{"Keyboard/*", "",
               _("The parameter Keyboard/<cmd> contains a comma-separated list "
                 "of key names (e.g. Alt-Shift-F1).  When the named key is "
                 "pressed, the command <cmd> is executed.")},

    /// Core
    OptionData{"preferences/agbPrint", "AGBPrinter", _("Enable AGB debug print")},
    OptionData{"preferences/autoFrameSkip", "FrameSkipAuto", _("Auto skip frames")},
    OptionData{"preferences/autoPatch", "ApplyPatches",
               _("Apply IPS / UPS / IPF patches if found")},
    OptionData{"preferences/autoSaveLoadCheatList", "",
               _("Automatically save and load cheat list")},
    OptionData{
        "preferences/borderAutomatic",
        "",
        _("Automatically enable border for Super Game Boy games"),
    },
    OptionData{"preferences/borderOn", "", _("Always enable border")},
    OptionData{"preferences/captureFormat", "", _("Screen capture file format")},
    OptionData{"preferences/cheatsEnabled", "", _("Enable cheats")},
    OptionData{"preferences/disableStatus", "NoStatusMsg", _("Disable on-screen status messages")},
    OptionData{"preferences/emulatorType", "", _("Type of system to emulate")},
    OptionData{"preferences/flashSize", "", _("Flash size 0 = 64 KB 1 = 128 KB")},
    OptionData{"preferences/frameSkip", "FrameSkip",
               _("Skip frames. Values are 0-9 or -1 to skip automatically "
                 "based on time.")},
    OptionData{"preferences/gbPaletteOption", "", _("The palette to use")},
    OptionData{"preferences/gbPrinter", "Printer", _("Enable printer emulation")},
    OptionData{"preferences/gdbBreakOnLoad", "DebugGDBBreakOnLoad",
               _("Break into GDB after loading the game.")},
    OptionData{"preferences/gdbPort", "DebugGDBPort", _("Port to connect GDB to")},
#ifndef NO_LINK
    OptionData{"preferences/LinkNumPlayers", "", _("Number of players in network")},
#endif
    OptionData{"preferences/maxScale", "", _("Maximum scale factor (0 = no limit)")},
    OptionData{"preferences/pauseWhenInactive", "PauseWhenInactive",
               _("Pause game when main window loses focus")},
    OptionData{"preferences/rtcEnabled", "RTC",
               _("Enable RTC (vba-over.ini override is rtcEnabled")},
    OptionData{"preferences/saveType", "", _("Native save (\"battery\") hardware type")},
    OptionData{"preferences/showSpeed", "", _("Show speed indicator")},
    OptionData{"preferences/showSpeedTransparent", "Transparent",
               _("Draw on-screen messages transparently")},
    OptionData{"preferences/skipBios", "SkipIntro", _("Skip BIOS initialization")},
    OptionData{"preferences/skipSaveGameCheats", "",
               _("Do not overwrite cheat list when loading state")},
    OptionData{"preferences/skipSaveGameBattery", "",
               _("Do not overwrite native (battery) save when loading state")},
    OptionData{"preferences/throttle", "",
               _("Throttle game speed, even when accelerated (0-450 %, 0 = no "
                 "throttle)")},
    OptionData{"preferences/speedupThrottle", "",
               _("Set throttle for speedup key (0-3000 %, 0 = no throttle)")},
    OptionData{"preferences/speedupFrameSkip", "",
               _("Number of frames to skip with speedup (instead of speedup "
                 "throttle)")},
    OptionData{"preferences/speedupThrottleFrameSkip", "",
               _("Use frame skip for speedup throttle")},
    OptionData{"preferences/speedupMute", "",
               _("Mute sound during speedup")},
    OptionData{"preferences/useBiosGB", "BootRomGB", _("Use the specified BIOS file for Game Boy")},
    OptionData{"preferences/useBiosGBA", "BootRomEn", _("Use the specified BIOS file")},
    OptionData{"preferences/useBiosGBC", "BootRomGBC",
               _("Use the specified BIOS file for Game Boy Color")},
    OptionData{"preferences/vsync", "VSync", _("Wait for vertical sync")},

    /// Geometry
    OptionData{"geometry/fullScreen", "Fullscreen", _("Enter fullscreen mode at startup")},
    OptionData{"geometry/isMaximized", "Maximized", _("Window maximized")},
    OptionData{"geometry/windowHeight", "Height", _("Window height at startup")},
    OptionData{"geometry/windowWidth", "Width", _("Window width at startup")},
    OptionData{"geometry/windowX", "X", _("Window axis X position at startup")},
    OptionData{"geometry/windowY", "Y", _("Window axis Y position at startup")},

    /// UI
    OptionData{"ui/allowKeyboardBackgroundInput", "AllowKeyboardBackgroundInput",
               _("Capture key events while on background")},
    OptionData{"ui/allowJoystickBackgroundInput", "AllowJoystickBackgroundInput",
               _("Capture joy events while on background")},
    OptionData{"ui/hideMenuBar", "HideMenuBar", _("Hide menu bar when mouse is inactive")},
    OptionData{"ui/suspendScreenSaver", "SuspendScreenSaver",
               _("Suspend screensaver when game is running")},

    /// Sound
    OptionData{"Sound/AudioAPI", "", _("Sound API; if unsupported, default API will be used")},
    OptionData{"Sound/AudioDevice", "", _("Device ID of chosen audio device for chosen driver")},
    OptionData{"Sound/Buffers", "", _("Number of sound buffers")},
    OptionData{"Sound/Enable", "", _("Bit mask of sound channels to enable")},
    OptionData{"Sound/GBAFiltering", "", _("Game Boy Advance sound filtering (%)")},
    OptionData{"Sound/GBAInterpolation", "GBASoundInterpolation",
               _("Game Boy Advance sound interpolation")},
    OptionData{"Sound/GBDeclicking", "GBDeclicking", _("Game Boy sound declicking")},
    OptionData{"Sound/GBEcho", "", _("Game Boy echo effect (%)")},
    OptionData{"Sound/GBEnableEffects", "GBEnhanceSound", _("Enable Game Boy sound effects")},
    OptionData{"Sound/GBStereo", "", _("Game Boy stereo effect (%)")},
    OptionData{"Sound/GBSurround", "GBSurround", _("Game Boy surround sound effect (%)")},
    OptionData{"Sound/Quality", "", _("Sound sample rate (kHz)")},
    OptionData{"Sound/DSoundHWAccel", "DSoundHWAccel", _("Use DirectSound hardware acceleration")},
    OptionData{"Sound/Upmix", "Upmix", _("Upmix stereo to surround")},
    OptionData{"Sound/Volume", "", _("Sound volume (%)")},

    // Last. This should never be used, it actually maps to OptionID::kLast.
    // This is to prevent a memory access violation error in case something
    // attempts to instantiate a OptionID::kLast. It will trigger a check
    // in the Option constructor, but that is after the constructor has
    // accessed this entry.
    OptionData{"", "", ""},
};

nonstd::optional<OptionID> StringToOptionId(const wxString& input) {
    static const std::map<wxString, OptionID> kStringToOptionId([] {
        std::map<wxString, OptionID> string_to_option_id;
        for (size_t i = 0; i < kNbOptions; i++) {
            string_to_option_id.emplace(kAllOptionsData[i].config_name, static_cast<OptionID>(i));
        }
        VBAM_CHECK(string_to_option_id.size() == kNbOptions);
        return string_to_option_id;
    }());

    const auto iter = kStringToOptionId.find(input);
    if (iter == kStringToOptionId.end()) {
        return nonstd::nullopt;
    }
    return iter->second;
}

wxString FilterToString(const Filter& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbFilters);
    return kFilterStrings[size_value];
}

wxString InterframeToString(const Interframe& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbInterframes);
    return kInterframeStrings[size_value];
}

wxString RenderMethodToString(const RenderMethod& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbRenderMethods);
    return kRenderMethodStrings[size_value];
}

wxString AudioApiToString(const AudioApi& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbAudioApis);
    return kAudioApiStrings[size_value];
}

wxString AudioRateToString(const AudioRate& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbSoundRate);
    return kAudioRateStrings[size_value];
}

Filter StringToFilter(const wxString& config_name, const wxString& input) {
    static const std::map<wxString, Filter> kStringToFilter([] {
        std::map<wxString, Filter> string_to_filter;
        for (size_t i = 0; i < kNbFilters; i++) {
            string_to_filter.emplace(kFilterStrings[i], static_cast<Filter>(i));
        }
        VBAM_CHECK(string_to_filter.size() == kNbFilters);
        return string_to_filter;
    }());

    const auto iter = kStringToFilter.find(input);
    if (iter == kStringToFilter.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
                     input, config_name,
                     AllEnumValuesForType(Option::Type::kFilter));
        return Filter::kNone;
    }
    return iter->second;
}

Interframe StringToInterframe(const wxString& config_name, const wxString& input) {
    static const std::map<wxString, Interframe> kStringToInterframe([] {
        std::map<wxString, Interframe> string_to_interframe;
        for (size_t i = 0; i < kNbInterframes; i++) {
            string_to_interframe.emplace(kInterframeStrings[i], static_cast<Interframe>(i));
        }
        VBAM_CHECK(string_to_interframe.size() == kNbInterframes);
        return string_to_interframe;
    }());

    const auto iter = kStringToInterframe.find(input);
    if (iter == kStringToInterframe.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
                     input, config_name,
                     AllEnumValuesForType(Option::Type::kInterframe));
        return Interframe::kNone;
    }
    return iter->second;
}

RenderMethod StringToRenderMethod(const wxString& config_name,
                                  const wxString& input) {
    static const std::map<wxString, RenderMethod> kStringToRenderMethod([] {
        std::map<wxString, RenderMethod> string_to_render_method;
        for (size_t i = 0; i < kNbRenderMethods; i++) {
            string_to_render_method.emplace(kRenderMethodStrings[i], static_cast<RenderMethod>(i));
        }
        VBAM_CHECK(string_to_render_method.size() == kNbRenderMethods);
        return string_to_render_method;
    }());

    const auto iter = kStringToRenderMethod.find(input);
    if (iter == kStringToRenderMethod.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
                     input, config_name,
                     AllEnumValuesForType(Option::Type::kRenderMethod));
        return RenderMethod::kSimple;
    }
    return iter->second;
}

AudioApi StringToAudioApi(const wxString& config_name, const wxString& input) {
    static const std::map<wxString, AudioApi> kStringToAudioApi([] {
        std::map<wxString, AudioApi> string_to_audio_api;
        for (size_t i = 0; i < kNbAudioApis; i++) {
            string_to_audio_api.emplace(kAudioApiStrings[i], static_cast<AudioApi>(i));
        }
        VBAM_CHECK(string_to_audio_api.size() == kNbAudioApis);
        return string_to_audio_api;
    }());

    const auto iter = kStringToAudioApi.find(input);
    if (iter == kStringToAudioApi.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
                     input, config_name,
                     AllEnumValuesForType(Option::Type::kAudioApi));
        return AudioApi::kOpenAL;
    }
    return iter->second;
}

AudioRate StringToSoundQuality(const wxString& config_name, const wxString& input) {
    static const std::map<wxString, AudioRate> kStringToSoundQuality([] {
        std::map<wxString, AudioRate> string_to_sound_quality;
        for (size_t i = 0; i < kNbSoundRate; i++) {
            string_to_sound_quality.emplace(kAudioRateStrings[i], static_cast<AudioRate>(i));
        }
        VBAM_CHECK(string_to_sound_quality.size() == kNbSoundRate);
        return string_to_sound_quality;
    }());

    const auto iter = kStringToSoundQuality.find(input);
    if (iter == kStringToSoundQuality.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"), input, config_name,
                     AllEnumValuesForType(Option::Type::kAudioRate));
        return AudioRate::k44kHz;
    }
    return iter->second;
}

wxString AllEnumValuesForType(Option::Type type) {
    switch (type) {
        case Option::Type::kFilter: {
            static const wxString kAllFilterValues(AllEnumValuesForArray(kFilterStrings));
            return kAllFilterValues;
        }
        case Option::Type::kInterframe: {
            static const wxString kAllInterframeValues(AllEnumValuesForArray(kInterframeStrings));
            return kAllInterframeValues;
        }
        case Option::Type::kRenderMethod: {
            static const wxString kAllRenderValues(AllEnumValuesForArray(kRenderMethodStrings));
            return kAllRenderValues;
        }
        case Option::Type::kAudioApi: {
            static const wxString kAllAudioApiValues(AllEnumValuesForArray(kAudioApiStrings));
            return kAllAudioApiValues;
        }
        case Option::Type::kAudioRate: {
            static const wxString kAllSoundQualityValues(AllEnumValuesForArray(kAudioRateStrings));
            return kAllSoundQualityValues;
        }

        // We don't use default here to explicitly trigger a compiler warning
        // when adding a new value.
        case Option::Type::kNone:
        case Option::Type::kBool:
        case Option::Type::kDouble:
        case Option::Type::kInt:
        case Option::Type::kUnsigned:
        case Option::Type::kString:
        case Option::Type::kGbPalette:
            VBAM_NOTREACHED();
            return wxEmptyString;
    }
    VBAM_NOTREACHED();
    return wxEmptyString;
}

size_t MaxForType(Option::Type type) {
    switch (type) {
        case Option::Type::kFilter:
            return kNbFilters;
        case Option::Type::kInterframe:
            return kNbInterframes;
        case Option::Type::kRenderMethod:
            return kNbRenderMethods;
        case Option::Type::kAudioApi:
            return kNbAudioApis;
        case Option::Type::kAudioRate:
            return kNbSoundRate;

        // We don't use default here to explicitly trigger a compiler warning
        // when adding a new value.
        case Option::Type::kNone:
        case Option::Type::kBool:
        case Option::Type::kDouble:
        case Option::Type::kInt:
        case Option::Type::kUnsigned:
        case Option::Type::kString:
        case Option::Type::kGbPalette:
            VBAM_NOTREACHED();
            return 0;
    }
    VBAM_NOTREACHED();
    return 0;
}

}  // namespace internal
}  // namespace config
