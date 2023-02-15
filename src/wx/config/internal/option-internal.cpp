#include "config/option.h"

// Helper implementation file to define and compile all of these huge constants
// separately. These should not be updated very often, so having these in a
// separate file improves incremental build time.

#include <wx/log.h>
#include <algorithm>
#include <limits>

#include "../common/ConfigManager.h"
#include "../gb/gbGlobals.h"
#include "opts.h"

#define VBAM_OPTION_INTERNAL_INCLUDE
#include "config/internal/option-internal.h"
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
#if defined(__WXMSW__) && !defined(NO_D3D)
    "direct3d",
#elif defined(__WXMAC__)
    "quartz2d",
#endif
};

// This enum must be kept in sync with the one in wxvbam.h
// TODO: These 2 enums should be unified and a validator created for this enum.
// TODO: DirectSound and XAudio2 should only be used on Windows.
enum class AudioApi {
    kSdl = 0,
    kOpenAL,
    kDirectSound,
    kXAudio2,
    kFaudio,

    // Do not add anything under here.
    kLast,
};
constexpr size_t kNbAudioApis = static_cast<size_t>(AudioApi::kLast);

// These MUST follow the same order as the definitions of the enum above.
// Adding an option without adding to this array will result in a compiler
// error since kNbAudioApis is automatically updated.
static const std::array<wxString, kNbAudioApis> kAudioApiStrings = {
    "sdl",
    "openal",
    "directsound",
    "xaudio2",
    "faudio",
};

enum class SoundQuality {
    k48kHz = 0,
    k44kHz,
    k22kHz,
    k11kHz,

    // Do not add anything under here.
    kLast,
};
constexpr size_t kNbSoundQualities = static_cast<size_t>(SoundQuality::kLast);

// These MUST follow the same order as the definitions of the enum above.
// Adding an option without adding to this array will result in a compiler
// error since kNbSoundQualities is automatically updated.
static const std::array<wxString, kNbSoundQualities> kSoundQualityStrings = {
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
#if defined(NO_OGL)
        RenderMethod render_method = RenderMethod::kSimple;
#else
        RenderMethod render_method = RenderMethod::kOpenGL;
#endif
        double video_scale = 3;
        bool retain_aspect = true;

        /// General
        uint32_t ini_version = kIniLatestVersion;

        /// Geometry
        bool window_maximized = false;
        uint32_t window_height = 0;
        uint32_t window_width = 0;
        int32_t window_pos_x = -1;
        int32_t window_pos_y = -1;
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
        Option(OptionID::kDispKeepOnTop, &g_owned_opts.keep_on_top),
        Option(OptionID::kDispMaxThreads, &g_owned_opts.max_threads, 0, 256),
        Option(OptionID::kDispRenderMethod, &g_owned_opts.render_method),
        Option(OptionID::kDispScale, &g_owned_opts.video_scale, 1, 6),
        Option(OptionID::kDispStretch, &g_owned_opts.retain_aspect),

        /// GB
        Option(OptionID::kGBBiosFile, &gopts.gb_bios),
        Option(OptionID::kGBColorOption, &gbColorOption, 0, 1),
        Option(OptionID::kGBColorizerHack, &colorizerHack, 0, 1),
        Option(OptionID::kGBLCDFilter, &gopts.gb_lcd_filter),
        Option(OptionID::kGBGBCBiosFile, &gopts.gbc_bios),
        Option(OptionID::kGBPalette0, systemGbPalette),
        Option(OptionID::kGBPalette1, systemGbPalette + 8),
        Option(OptionID::kGBPalette2, systemGbPalette + 16),
        Option(OptionID::kGBPrintAutoPage, &gopts.print_auto_page),
        Option(OptionID::kGBPrintScreenCap, &gopts.print_screen_cap),
        Option(OptionID::kGBROMDir, &gopts.gb_rom_dir),
        Option(OptionID::kGBGBCROMDir, &gopts.gbc_rom_dir),

        /// GBA
        Option(OptionID::kGBABiosFile, &gopts.gba_bios),
        Option(OptionID::kGBALCDFilter, &gopts.gba_lcd_filter),
#ifndef NO_LINK
        Option(OptionID::kGBALinkAuto, &gopts.link_auto),
        Option(OptionID::kGBALinkFast, &gopts.link_hacks),
        Option(OptionID::kGBALinkHost, &gopts.link_host),
        Option(OptionID::kGBAServerIP, &gopts.server_ip),
        Option(OptionID::kGBALinkPort, &gopts.link_port, 0, 65535),
        Option(OptionID::kGBALinkProto, &gopts.link_proto),
        Option(OptionID::kGBALinkTimeout, &gopts.link_timeout, 0, 9999999),
        Option(OptionID::kGBALinkType, &gopts.gba_link_type, 0, 5),
#endif
        Option(OptionID::kGBAROMDir, &gopts.gba_rom_dir),

        /// General
        Option(OptionID::kGenAutoLoadLastState, &gopts.autoload_state),
        Option(OptionID::kGenBatteryDir, &gopts.battery_dir),
        Option(OptionID::kGenFreezeRecent, &gopts.recent_freeze),
        Option(OptionID::kGenRecordingDir, &gopts.recording_dir),
        Option(OptionID::kGenRewindInterval, &gopts.rewind_interval, 0, 600),
        Option(OptionID::kGenScreenshotDir, &gopts.scrshot_dir),
        Option(OptionID::kGenStateDir, &gopts.state_dir),
        Option(OptionID::kGenStatusBar, &gopts.statusbar),
        Option(OptionID::kGenIniVersion, &g_owned_opts.ini_version, 0, std::numeric_limits<uint32_t>::max()),

        /// Joypad
        Option(OptionID::kJoy),
        Option(OptionID::kJoyAutofireThrottle, &gopts.autofire_rate, 1, 1000),
        Option(OptionID::kJoyDefault, &gopts.default_stick, 1, 4),

        /// Keyboard
        Option(OptionID::kKeyboard),

        /// Core
        Option(OptionID::kPrefAgbPrint, &agbPrint, 0, 1),
        Option(OptionID::kPrefAutoFrameSkip, &autoFrameSkip, 0, 1),
        Option(OptionID::kPrefAutoPatch, &autoPatch, 0, 1),
        Option(OptionID::kPrefAutoSaveLoadCheatList, &gopts.autoload_cheats),
        Option(OptionID::kPrefBorderAutomatic, &gbBorderAutomatic, 0, 1),
        Option(OptionID::kPrefBorderOn, &gbBorderOn, 0, 1),
        Option(OptionID::kPrefCaptureFormat, &captureFormat, 0, 1),
        Option(OptionID::kPrefCheatsEnabled, &coreOptions.cheatsEnabled, 0, 1),
        Option(OptionID::kPrefDisableStatus, &disableStatusMessages, 0, 1),
        Option(OptionID::kPrefEmulatorType, &gbEmulatorType, 0, 5),
        Option(OptionID::kPrefFlashSize, &optFlashSize, 0, 1),
        Option(OptionID::kPrefFrameSkip, &frameSkip, -1, 9),
        Option(OptionID::kPrefGBPaletteOption, &gbPaletteOption, 0, 2),
        Option(OptionID::kPrefGBPrinter, &coreOptions.winGbPrinterEnabled, 0, 1),
        Option(OptionID::kPrefGDBBreakOnLoad, &gopts.gdb_break_on_load),
        Option(OptionID::kPrefGDBPort, &gopts.gdb_port, 0, 65535),
#ifndef NO_LINK
        Option(OptionID::kPrefLinkNumPlayers, &gopts.link_num_players, 2, 4),
#endif
        Option(OptionID::kPrefMaxScale, &gopts.max_scale, 0, 100),
        Option(OptionID::kPrefPauseWhenInactive, &pauseWhenInactive, 0, 1),
        Option(OptionID::kPrefRTCEnabled, &coreOptions.rtcEnabled, 0, 1),
        Option(OptionID::kPrefSaveType, &coreOptions.cpuSaveType, 0, 5),
        Option(OptionID::kPrefShowSpeed, &showSpeed, 0, 2),
        Option(OptionID::kPrefShowSpeedTransparent, &showSpeedTransparent, 0, 1),
        Option(OptionID::kPrefSkipBios, &coreOptions.skipBios, 0, 1),
        Option(OptionID::kPrefSkipSaveGameCheats, &coreOptions.skipSaveGameCheats, 0, 1),
        Option(OptionID::kPrefSkipSaveGameBattery, &coreOptions.skipSaveGameBattery, 0, 1),
        Option(OptionID::kPrefThrottle, &coreOptions.throttle, 0, 450),
        Option(OptionID::kPrefSpeedupThrottle, &coreOptions.speedup_throttle, 0, 3000),
        Option(OptionID::kPrefSpeedupFrameSkip, &coreOptions.speedup_frame_skip, 0, 300),
        Option(OptionID::kPrefSpeedupThrottleFrameSkip, &coreOptions.speedup_throttle_frame_skip),
        Option(OptionID::kPrefUseBiosGB, &gopts.use_bios_file_gb),
        Option(OptionID::kPrefUseBiosGBA, &gopts.use_bios_file_gba),
        Option(OptionID::kPrefUseBiosGBC, &gopts.use_bios_file_gbc),
        Option(OptionID::kPrefVsync, &gopts.vsync),

        /// Geometry
        Option(OptionID::kGeomFullScreen, &fullScreen, 0, 1),
        Option(OptionID::kGeomIsMaximized, &g_owned_opts.window_maximized),
        Option(OptionID::kGeomWindowHeight, &g_owned_opts.window_height, 0, 99999),
        Option(OptionID::kGeomWindowWidth, &g_owned_opts.window_width, 0, 99999),
        Option(OptionID::kGeomWindowX, &g_owned_opts.window_pos_x, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()),
        Option(OptionID::kGeomWindowY, &g_owned_opts.window_pos_y, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()),

        /// UI
        Option(OptionID::kUIAllowKeyboardBackgroundInput, &allowKeyboardBackgroundInput),
        Option(OptionID::kUIAllowJoystickBackgroundInput, &allowJoystickBackgroundInput),
        Option(OptionID::kUIHideMenuBar, &gopts.hide_menu_bar),

        /// Sound
        Option(OptionID::kSoundAudioAPI, &gopts.audio_api),
        Option(OptionID::kSoundAudioDevice, &gopts.audio_dev),
        Option(OptionID::kSoundBuffers, &gopts.audio_buffers, 2, 10),
        Option(OptionID::kSoundEnable, &gopts.sound_en, 0, 0x30f),
        Option(OptionID::kSoundGBAFiltering, &gopts.gba_sound_filter, 0, 100),
        Option(OptionID::kSoundGBAInterpolation, &gopts.soundInterpolation),
        Option(OptionID::kSoundGBDeclicking, &gopts.gb_declick),
        Option(OptionID::kSoundGBEcho, &gopts.gb_echo, 0, 100),
        Option(OptionID::kSoundGBEnableEffects, &gopts.gb_effects_config_enabled),
        Option(OptionID::kSoundGBStereo, &gopts.gb_stereo, 0, 100),
        Option(OptionID::kSoundGBSurround, &gopts.gb_effects_config_surround),
        Option(OptionID::kSoundQuality, &gopts.sound_qual),
        Option(OptionID::kSoundVolume, &gopts.sound_vol, 0, 200),
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
    OptionData{"Display/Bilinear", "Bilinear",
               _("Use bilinear filter with 3d renderer")},
    OptionData{"Display/Filter", "", _("Full-screen filter to apply")},
    OptionData{"Display/FilterPlugin", "", _("Filter plugin library")},
    OptionData{"Display/IFB", "", _("Interframe blending function")},
    OptionData{"Display/KeepOnTop", "KeepOnTop", _("Keep window on top")},
    OptionData{"Display/MaxThreads", "Multithread",
               _("Maximum number of threads to run filters in")},
    OptionData{"Display/RenderMethod", "",
               _("Render method; if unsupported, simple method will be used")},
    OptionData{"Display/Scale", "", _("Default scale factor")},
    OptionData{"Display/Stretch", "RetainAspect",
               _("Retain aspect ratio when resizing")},

    /// GB
    OptionData{"GB/BiosFile", "",
               _("BIOS file to use for Game Boy, if enabled")},
    OptionData{"GB/ColorOption", "GBColorOption",
               _("Game Boy color enhancement, if enabled")},
    OptionData{"GB/ColorizerHack", "ColorizerHack",
               _("Enable DX Colorization Hacks")},
    OptionData{"GB/LCDFilter", "GBLcdFilter",
               _("Apply LCD filter, if enabled")},
    OptionData{"GB/GBCBiosFile", "",
               _("BIOS file to use for Game Boy Color, if enabled")},
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
    OptionData{"GB/GBCROMDir", "",
               _("Directory to look for Game Boy Color ROM files")},

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
    OptionData{"GBA/LinkPort", "",
               _("Default network link port (server and client)")},
    OptionData{"GBA/LinkProto", "LinkProto", _("Default network protocol")},
    OptionData{"GBA/LinkTimeout", "LinkTimeout", _("Link timeout (ms)")},
    OptionData{"GBA/LinkType", "LinkType", _("Link cable type")},
#endif
    OptionData{"GBA/ROMDir", "", _("Directory to look for ROM files")},

    /// General
    OptionData{"General/AutoLoadLastState", "",
               _("Automatically load last saved state")},
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
    OptionData{"Joypad/AutofireThrottle", "",
               _("The autofire toggle period, in frames (1/60 s)")},
    OptionData{"Joypad/Default", "",
               _("The number of the stick to use in single-player mode")},

    /// Keyboard
    OptionData{"Keyboard/*", "",
               _("The parameter Keyboard/<cmd> contains a comma-separated list "
                 "of key names (e.g. Alt-Shift-F1).  When the named key is "
                 "pressed, the command <cmd> is executed.")},

    /// Core
    OptionData{"preferences/agbPrint", "AGBPrinter",
               _("Enable AGB debug print")},
    OptionData{"preferences/autoFrameSkip", "FrameSkipAuto",
               _("Auto skip frames")},
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
    OptionData{"preferences/captureFormat", "",
               _("Screen capture file format")},
    OptionData{"preferences/cheatsEnabled", "", _("Enable cheats")},
    OptionData{"preferences/disableStatus", "NoStatusMsg",
               _("Disable on-screen status messages")},
    OptionData{"preferences/emulatorType", "", _("Type of system to emulate")},
    OptionData{"preferences/flashSize", "",
               _("Flash size 0 = 64 KB 1 = 128 KB")},
    OptionData{"preferences/frameSkip", "FrameSkip",
               _("Skip frames. Values are 0-9 or -1 to skip automatically "
                 "based on time.")},
    OptionData{"preferences/gbPaletteOption", "", _("The palette to use")},
    OptionData{"preferences/gbPrinter", "Printer",
               _("Enable printer emulation")},
    OptionData{"preferences/gdbBreakOnLoad", "DebugGDBBreakOnLoad",
               _("Break into GDB after loading the game.")},
    OptionData{"preferences/gdbPort", "DebugGDBPort",
               _("Port to connect GDB to")},
#ifndef NO_LINK
    OptionData{"preferences/LinkNumPlayers", "",
               _("Number of players in network")},
#endif
    OptionData{"preferences/maxScale", "",
               _("Maximum scale factor (0 = no limit)")},
    OptionData{"preferences/pauseWhenInactive", "PauseWhenInactive",
               _("Pause game when main window loses focus")},
    OptionData{"preferences/rtcEnabled", "RTC",
               _("Enable RTC (vba-over.ini override is rtcEnabled")},
    OptionData{"preferences/saveType", "",
               _("Native save (\"battery\") hardware type")},
    OptionData{"preferences/showSpeed", "", _("Show speed indicator")},
    OptionData{"preferences/showSpeedTransparent", "Transparent",
               _("Draw on-screen messages transparently")},
    OptionData{"preferences/skipBios", "SkipIntro",
               _("Skip BIOS initialization")},
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
    OptionData{"preferences/useBiosGB", "BootRomGB",
               _("Use the specified BIOS file for Game Boy")},
    OptionData{"preferences/useBiosGBA", "BootRomEn",
               _("Use the specified BIOS file")},
    OptionData{"preferences/useBiosGBC", "BootRomGBC",
               _("Use the specified BIOS file for Game Boy Color")},
    OptionData{"preferences/vsync", "VSync", _("Wait for vertical sync")},

    /// Geometry
    OptionData{"geometry/fullScreen", "Fullscreen",
               _("Enter fullscreen mode at startup")},
    OptionData{"geometry/isMaximized", "Maximized", _("Window maximized")},
    OptionData{"geometry/windowHeight", "Height",
               _("Window height at startup")},
    OptionData{"geometry/windowWidth", "Width", _("Window width at startup")},
    OptionData{"geometry/windowX", "X", _("Window axis X position at startup")},
    OptionData{"geometry/windowY", "Y", _("Window axis Y position at startup")},

    /// UI
    OptionData{"ui/allowKeyboardBackgroundInput",
               "AllowKeyboardBackgroundInput",
               _("Capture key events while on background")},
    OptionData{"ui/allowJoystickBackgroundInput",
               "AllowJoystickBackgroundInput",
               _("Capture joy events while on background")},
    OptionData{"ui/hideMenuBar", "", _("Hide menu bar when mouse is inactive")},

    /// Sound
    OptionData{"Sound/AudioAPI", "",
               _("Sound API; if unsupported, default API will be used")},
    OptionData{"Sound/AudioDevice", "",
               _("Device ID of chosen audio device for chosen driver")},
    OptionData{"Sound/Buffers", "", _("Number of sound buffers")},
    OptionData{"Sound/Enable", "", _("Bit mask of sound channels to enable")},
    OptionData{"Sound/GBAFiltering", "",
               _("Game Boy Advance sound filtering (%)")},
    OptionData{"Sound/GBAInterpolation", "GBASoundInterpolation",
               _("Game Boy Advance sound interpolation")},
    OptionData{"Sound/GBDeclicking", "GBDeclicking",
               _("Game Boy sound declicking")},
    OptionData{"Sound/GBEcho", "", _("Game Boy echo effect (%)")},
    OptionData{"Sound/GBEnableEffects", "GBEnhanceSound",
               _("Enable Game Boy sound effects")},
    OptionData{"Sound/GBStereo", "", _("Game Boy stereo effect (%)")},
    OptionData{"Sound/GBSurround", "GBSurround",
               _("Game Boy surround sound effect (%)")},
    OptionData{"Sound/Quality", "", _("Sound sample rate (kHz)")},
    OptionData{"Sound/Volume", "", _("Sound volume (%)")},

    // Last. This should never be used, it actually maps to OptionID::kLast.
    // This is to prevent a memory access violation error in case something
    // attempts to instantiate a OptionID::kLast. It will trigger a check
    // in the Option constructor, but that is after the constructor has
    // accessed this entry.
    OptionData{"", "", ""},
};

nonstd::optional<OptionID> StringToOptionId(const wxString& input) {
    static std::map<wxString, OptionID> kStringToOptionId;
    if (kStringToOptionId.empty()) {
        for (size_t i = 0; i < kNbOptions; i++) {
            kStringToOptionId.emplace(kAllOptionsData[i].config_name,
                                      static_cast<OptionID>(i));
        }
        assert(kStringToOptionId.size() == kNbOptions);
    }

    const auto iter = kStringToOptionId.find(input);
    if (iter == kStringToOptionId.end()) {
        return nonstd::nullopt;
    }
    return iter->second;
}

wxString FilterToString(const Filter& value) {
    const size_t size_value = static_cast<size_t>(value);
    assert(size_value < kNbFilters);
    return kFilterStrings[size_value];
}

wxString InterframeToString(const Interframe& value) {
    const size_t size_value = static_cast<size_t>(value);
    assert(size_value < kNbInterframes);
    return kInterframeStrings[size_value];
}

wxString RenderMethodToString(const RenderMethod& value) {
    const size_t size_value = static_cast<size_t>(value);
    assert(size_value < kNbRenderMethods);
    return kRenderMethodStrings[size_value];
}

wxString AudioApiToString(int value) {
    assert(value >= 0 && static_cast<size_t>(value) < kNbAudioApis);
    return kAudioApiStrings[value];
}

wxString SoundQualityToString(int value) {
    assert(value >= 0 && static_cast<size_t>(value) < kNbSoundQualities);
    return kSoundQualityStrings[value];
}

Filter StringToFilter(const wxString& config_name, const wxString& input) {
    static std::map<wxString, Filter> kStringToFilter;
    if (kStringToFilter.empty()) {
        for (size_t i = 0; i < kNbFilters; i++) {
            kStringToFilter.emplace(kFilterStrings[i], static_cast<Filter>(i));
        }
        assert(kStringToFilter.size() == kNbFilters);
    }

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
    static std::map<wxString, Interframe> kStringToInterframe;
    if (kStringToInterframe.empty()) {
        for (size_t i = 0; i < kNbInterframes; i++) {
            kStringToInterframe.emplace(kInterframeStrings[i],
                                        static_cast<Interframe>(i));
        }
        assert(kStringToInterframe.size() == kNbInterframes);
    }

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
    static std::map<wxString, RenderMethod> kStringToRenderMethod;
    if (kStringToRenderMethod.empty()) {
        for (size_t i = 0; i < kNbRenderMethods; i++) {
            kStringToRenderMethod.emplace(kRenderMethodStrings[i],
                                          static_cast<RenderMethod>(i));
        }
        assert(kStringToRenderMethod.size() == kNbRenderMethods);
    }

    const auto iter = kStringToRenderMethod.find(input);
    if (iter == kStringToRenderMethod.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
                     input, config_name,
                     AllEnumValuesForType(Option::Type::kRenderMethod));
        return RenderMethod::kSimple;
    }
    return iter->second;
}

int StringToAudioApi(const wxString& config_name, const wxString& input) {
    static std::map<wxString, AudioApi> kStringToAudioApi;
    if (kStringToAudioApi.empty()) {
        for (size_t i = 0; i < kNbAudioApis; i++) {
            kStringToAudioApi.emplace(kAudioApiStrings[i],
                                      static_cast<AudioApi>(i));
        }
        assert(kStringToAudioApi.size() == kNbAudioApis);
    }

    const auto iter = kStringToAudioApi.find(input);
    if (iter == kStringToAudioApi.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
                     input, config_name,
                     AllEnumValuesForType(Option::Type::kAudioApi));
        return 0;
    }
    return static_cast<int>(iter->second);
}

int StringToSoundQuality(const wxString& config_name, const wxString& input) {
    static std::map<wxString, SoundQuality> kStringToSoundQuality;
    if (kStringToSoundQuality.empty()) {
        for (size_t i = 0; i < kNbSoundQualities; i++) {
            kStringToSoundQuality.emplace(kSoundQualityStrings[i],
                                          static_cast<SoundQuality>(i));
        }
        assert(kStringToSoundQuality.size() == kNbSoundQualities);
    }

    const auto iter = kStringToSoundQuality.find(input);
    if (iter == kStringToSoundQuality.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
                     input, config_name,
                     AllEnumValuesForType(Option::Type::kSoundQuality));
        return 0;
    }
    return static_cast<int>(iter->second);
}

wxString AllEnumValuesForType(Option::Type type) {
    switch (type) {
        case Option::Type::kFilter: {
            static const wxString kAllFilterValues =
                AllEnumValuesForArray(kFilterStrings);
            return kAllFilterValues;
        }
        case Option::Type::kInterframe: {
            static const wxString kAllInterframeValues =
                AllEnumValuesForArray(kInterframeStrings);
            return kAllInterframeValues;
        }
        case Option::Type::kRenderMethod: {
            static const wxString kAllRenderValues =
                AllEnumValuesForArray(kRenderMethodStrings);
            return kAllRenderValues;
        }
        case Option::Type::kAudioApi: {
            static const wxString kAllAudioApiValues =
                AllEnumValuesForArray(kAudioApiStrings);
            return kAllAudioApiValues;
        }
        case Option::Type::kSoundQuality: {
            static const wxString kAllSoundQualityValues =
                AllEnumValuesForArray(kSoundQualityStrings);
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
            assert(false);
            return wxEmptyString;
    }
    assert(false);
    return wxEmptyString;
}

int MaxForType(Option::Type type) {
    switch (type) {
        case Option::Type::kFilter:
            return kNbFilters;
        case Option::Type::kInterframe:
            return kNbInterframes;
        case Option::Type::kRenderMethod:
            return kNbRenderMethods;
        case Option::Type::kAudioApi:
            return kNbAudioApis;
        case Option::Type::kSoundQuality:
            return kNbSoundQualities;

        // We don't use default here to explicitly trigger a compiler warning
        // when adding a new value.
        case Option::Type::kNone:
        case Option::Type::kBool:
        case Option::Type::kDouble:
        case Option::Type::kInt:
        case Option::Type::kUnsigned:
        case Option::Type::kString:
        case Option::Type::kGbPalette:
            assert(false);
            return 0;
    }
    assert(false);
    return 0;
}

}  // namespace internal
}  // namespace config
