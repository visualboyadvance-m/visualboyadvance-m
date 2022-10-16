#include "config/option.h"

// Helper implementation file to define and compile all of these huge constants
// separately. These should not be updated very often, so having these in a
// separate file improves incremental build time.

#include <wx/log.h>
#include <algorithm>

#include "../common/ConfigManager.h"
#include "../gb/gbGlobals.h"
#include "opts.h"

#define VBAM_OPTION_INTERNAL_INCLUDE
#include "config/internal/option-internal.h"
#undef VBAM_OPTION_INTERNAL_INCLUDE

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
        double video_scale = 3;
        wxString filter_plugin = wxEmptyString;
        Filter filter = Filter::kNone;
        Interframe interframe = Interframe::kNone;
#if defined(NO_OGL)
        RenderMethod render_method = RenderMethod::kSimple;
#else
        RenderMethod render_method = RenderMethod::kOpenGL;
#endif
    };
    static OwnedOptions g_owned_opts;

    // These MUST follow the same order as the definitions in OptionID.
    // Adding an option without adding to this array will result in a compiler
    // error since kNbOptions is automatically updated.
    // This will be initialized on the first call, in load_opts(), ensuring the
    // translation initialization has already happened.
    static std::array<Option, kNbOptions> g_all_opts = {
        /// Display
        Option(OptionID::kDisplayBilinear, &gopts.bilinear),
        Option(OptionID::kDisplayFilter, &g_owned_opts.filter),
        Option(OptionID::kDisplayFilterPlugin, &g_owned_opts.filter_plugin),
        Option(OptionID::kDisplayIFB, &g_owned_opts.interframe),
        Option(OptionID::kDisplayKeepOnTop, &gopts.keep_on_top),
        Option(OptionID::kDisplayMaxThreads, &gopts.max_threads, 1, 256),
        Option(OptionID::kDisplayRenderMethod, &g_owned_opts.render_method),
        Option(OptionID::kDisplayScale, &g_owned_opts.video_scale, 1, 6),
        Option(OptionID::kDisplayStretch, &gopts.retain_aspect),

        /// GB
        Option(OptionID::kGBBiosFile, &gopts.gb_bios),
        Option(OptionID::kGBColorOption, &gbColorOption, 0, 1),
        Option(OptionID::kGBColorizerHack, &colorizerHack, 0, 1),
        Option(OptionID::kGBLCDFilter, &gbLcdFilter),
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
        Option(OptionID::kGBALCDFilter, &gbaLcdFilter),
#ifndef NO_LINK
        Option(OptionID::kGBALinkAuto, &gopts.link_auto),
        Option(OptionID::kGBALinkFast, &linkHacks, 0, 1),
        Option(OptionID::kGBALinkHost, &gopts.link_host),
        Option(OptionID::kGBAServerIP, &gopts.server_ip),
        Option(OptionID::kGBALinkPort, &gopts.link_port, 0, 65535),
        Option(OptionID::kGBALinkProto, &gopts.link_proto, 0, 1),
        Option(OptionID::kGBALinkTimeout, &linkTimeout, 0, 9999999),
        Option(OptionID::kGBALinkType, &gopts.gba_link_type, 0, 5),
#endif
        Option(OptionID::kGBAROMDir, &gopts.gba_rom_dir),

        /// General
        Option(OptionID::kGeneralAutoLoadLastState, &gopts.autoload_state),
        Option(OptionID::kGeneralBatteryDir, &gopts.battery_dir),
        Option(OptionID::kGeneralFreezeRecent, &gopts.recent_freeze),
        Option(OptionID::kGeneralRecordingDir, &gopts.recording_dir),
        Option(OptionID::kGeneralRewindInterval, &gopts.rewind_interval, 0,
               600),
        Option(OptionID::kGeneralScreenshotDir, &gopts.scrshot_dir),
        Option(OptionID::kGeneralStateDir, &gopts.state_dir),
        Option(OptionID::kGeneralStatusBar, &gopts.statusbar, 0, 1),

        /// Joypad
        Option(OptionID::kJoypad),
        Option(OptionID::kJoypadAutofireThrottle, &gopts.autofire_rate, 1,
               1000),
        Option(OptionID::kJoypadDefault, &gopts.default_stick, 1, 4),

        /// Keyboard
        Option(OptionID::kKeyboard),

        // Core
        Option(OptionID::kpreferencesagbPrint, &agbPrint, 0, 1),
        Option(OptionID::kpreferencesautoFrameSkip, &autoFrameSkip, 0, 1),
        Option(OptionID::kpreferencesautoPatch, &autoPatch, 0, 1),
        Option(OptionID::kpreferencesautoSaveLoadCheatList,
               &gopts.autoload_cheats),
        Option(OptionID::kpreferencesborderAutomatic, &gbBorderAutomatic, 0, 1),
        Option(OptionID::kpreferencesborderOn, &gbBorderOn, 0, 1),
        Option(OptionID::kpreferencescaptureFormat, &captureFormat, 0, 1),
        Option(OptionID::kpreferencescheatsEnabled, &cheatsEnabled, 0, 1),
#ifdef MMX
        Option(OptionID::kpreferencesenableMMX, &enableMMX, 0, 1),
#endif
        Option(OptionID::kpreferencesdisableStatus, &disableStatusMessages, 0,
               1),
        Option(OptionID::kpreferencesemulatorType, &gbEmulatorType, 0, 5),
        Option(OptionID::kpreferencesflashSize, &optFlashSize, 0, 1),
        Option(OptionID::kpreferencesframeSkip, &frameSkip, -1, 9),
        Option(OptionID::kpreferencesfsColorDepth, &fsColorDepth, 0, 999),
        Option(OptionID::kpreferencesfsFrequency, &fsFrequency, 0, 999),
        Option(OptionID::kpreferencesfsHeight, &fsHeight, 0, 99999),
        Option(OptionID::kpreferencesfsWidth, &fsWidth, 0, 99999),
        Option(OptionID::kpreferencesgbPaletteOption, &gbPaletteOption, 0, 2),
        Option(OptionID::kpreferencesgbPrinter, &winGbPrinterEnabled, 0, 1),
        Option(OptionID::kpreferencesgdbBreakOnLoad, &gdbBreakOnLoad, 0, 1),
        Option(OptionID::kpreferencesgdbPort, &gdbPort, 0, 65535),
#ifndef NO_LINK
        Option(OptionID::kpreferencesLinkNumPlayers, &linkNumPlayers, 2, 4),
#endif
        Option(OptionID::kpreferencesmaxScale, &maxScale, 0, 100),
        Option(OptionID::kpreferencespauseWhenInactive, &pauseWhenInactive, 0,
               1),
        Option(OptionID::kpreferencesrtcEnabled, &rtcEnabled, 0, 1),
        Option(OptionID::kpreferencessaveType, &cpuSaveType, 0, 5),
        Option(OptionID::kpreferencesshowSpeed, &showSpeed, 0, 2),
        Option(OptionID::kpreferencesshowSpeedTransparent,
               &showSpeedTransparent, 0, 1),
        Option(OptionID::kpreferencesskipBios, &skipBios, 0, 1),
        Option(OptionID::kpreferencesskipSaveGameCheats, &skipSaveGameCheats, 0,
               1),
        Option(OptionID::kpreferencesskipSaveGameBattery, &skipSaveGameBattery,
               0, 1),
        Option(OptionID::kpreferencesthrottle, &throttle, 0, 450),
        Option(OptionID::kpreferencesspeedupThrottle, &speedup_throttle, 0,
               3000),
        Option(OptionID::kpreferencesspeedupFrameSkip, &speedup_frame_skip, 0,
               300),
        Option(OptionID::kpreferencesspeedupThrottleFrameSkip,
               &speedup_throttle_frame_skip),
        Option(OptionID::kpreferencesuseBiosGB, &useBiosFileGB, 0, 1),
        Option(OptionID::kpreferencesuseBiosGBA, &useBiosFileGBA, 0, 1),
        Option(OptionID::kpreferencesuseBiosGBC, &useBiosFileGBC, 0, 1),
        Option(OptionID::kpreferencesvsync, &vsync, 0, 1),

        /// Geometry
        Option(OptionID::kgeometryfullScreen, &fullScreen, 0, 1),
        Option(OptionID::kgeometryisMaximized, &windowMaximized, 0, 1),
        Option(OptionID::kgeometrywindowHeight, &windowHeight, 0, 99999),
        Option(OptionID::kgeometrywindowWidth, &windowWidth, 0, 99999),
        Option(OptionID::kgeometrywindowX, &windowPositionX, -1, 99999),
        Option(OptionID::kgeometrywindowY, &windowPositionY, -1, 99999),

        /// UI
        Option(OptionID::kuiallowKeyboardBackgroundInput,
               &allowKeyboardBackgroundInput),
        Option(OptionID::kuiallowJoystickBackgroundInput,
               &allowJoystickBackgroundInput),
        Option(OptionID::kuihideMenuBar, &gopts.hide_menu_bar),

        /// Sound
        Option(OptionID::kSoundAudioAPI, &gopts.audio_api),
        Option(OptionID::kSoundAudioDevice, &gopts.audio_dev),
        Option(OptionID::kSoundBuffers, &gopts.audio_buffers, 2, 10),
        Option(OptionID::kSoundEnable, &gopts.sound_en, 0, 0x30f),
        Option(OptionID::kSoundGBAFiltering, &gopts.gba_sound_filter, 0, 100),
        Option(OptionID::kSoundGBAInterpolation, &gopts.soundInterpolation),
        Option(OptionID::kSoundGBDeclicking, &gopts.gb_declick),
        Option(OptionID::kSoundGBEcho, &gopts.gb_echo, 0, 100),
        Option(OptionID::kSoundGBEnableEffects,
               &gopts.gb_effects_config_enabled),
        Option(OptionID::kSoundGBStereo, &gopts.gb_stereo, 0, 100),
        Option(OptionID::kSoundGBSurround, &gopts.gb_effects_config_surround),
        Option(OptionID::kSoundQuality, &gopts.sound_qual),
        Option(OptionID::kSoundVolume, &gopts.sound_vol, 0, 200),
    };
    return g_all_opts;
}

namespace internal {

// These MUST follow the same order as the definitions in OptionID.
// Adding an option without adding to this array will result in a compiler
// error since kNbOptions is automatically updated.
const std::array<OptionData, kNbOptions + 1> kAllOptionsData = {
    /// Display
    OptionData{"Display/Bilinear", "Bilinear",
               _("Use bilinear filter with 3d renderer"), Option::Type::kBool},
    OptionData{"Display/Filter", "", _("Full-screen filter to apply"),
               Option::Type::kFilter},
    OptionData{"Display/FilterPlugin", "", _("Filter plugin library"),
               Option::Type::kString},
    OptionData{"Display/IFB", "", _("Interframe blending function"),
               Option::Type::kInterframe},
    OptionData{"Display/KeepOnTop", "KeepOnTop", _("Keep window on top"),
               Option::Type::kBool},
    OptionData{"Display/MaxThreads", "Multithread",
               _("Maximum number of threads to run filters in"),
               Option::Type::kInt},
    OptionData{"Display/RenderMethod", "",
               _("Render method; if unsupported, simple method will be used"),
               Option::Type::kRenderMethod},
    OptionData{"Display/Scale", "", _("Default scale factor"),
               Option::Type::kDouble},
    OptionData{"Display/Stretch", "RetainAspect",
               _("Retain aspect ratio when resizing"), Option::Type::kBool},

    /// GB
    OptionData{"GB/BiosFile", "", _("BIOS file to use for GB, if enabled"),
               Option::Type::kString},
    OptionData{"GB/ColorOption", "GBColorOption",
               _("GB color enhancement, if enabled"), Option::Type::kInt},
    OptionData{"GB/ColorizerHack", "ColorizerHack",
               _("Enable DX Colorization Hacks"), Option::Type::kInt},
    OptionData{"GB/LCDFilter", "GBLcdFilter", _("Apply LCD filter, if enabled"),
               Option::Type::kBool},
    OptionData{"GB/GBCBiosFile", "", _("BIOS file to use for GBC, if enabled"),
               Option::Type::kString},
    OptionData{"GB/Palette0", "",
               _("The default palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555)."),
               Option::Type::kGbPalette},
    OptionData{"GB/Palette1", "",
               _("The first user palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555)."),
               Option::Type::kGbPalette},
    OptionData{"GB/Palette2", "",
               _("The second user palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555)."),
               Option::Type::kGbPalette},
    OptionData{"GB/PrintAutoPage", "PrintGather",
               _("Automatically gather a full page before printing"),
               Option::Type::kBool},
    OptionData{
        "GB/PrintScreenCap", "PrintSnap",
        _("Automatically save printouts as screen captures with -print suffix"),
        Option::Type::kBool},
    OptionData{"GB/ROMDir", "", _("Directory to look for ROM files"),
               Option::Type::kString},
    OptionData{"GB/GBCROMDir", "", _("Directory to look for GBC ROM files"),
               Option::Type::kString},

    /// GBA
    OptionData{"GBA/BiosFile", "", _("BIOS file to use, if enabled"),
               Option::Type::kString},
    OptionData{"GBA/LCDFilter", "GBALcdFilter",
               _("Apply LCD filter, if enabled"), Option::Type::kBool},
#ifndef NO_LINK
    OptionData{"GBA/LinkAuto", "LinkAuto", _("Enable link at boot"),
               Option::Type::kBool},
    OptionData{"GBA/LinkFast", "SpeedOn",
               _("Enable faster network protocol by default"),
               Option::Type::kInt},
    OptionData{"GBA/LinkHost", "", _("Default network link client host"),
               Option::Type::kString},
    OptionData{"GBA/ServerIP", "", _("Default network link server IP to bind"),
               Option::Type::kString},
    OptionData{"GBA/LinkPort", "",
               _("Default network link port (server and client)"),
               Option::Type::kUnsigned},
    OptionData{"GBA/LinkProto", "LinkProto", _("Default network protocol"),
               Option::Type::kInt},
    OptionData{"GBA/LinkTimeout", "LinkTimeout", _("Link timeout (ms)"),
               Option::Type::kInt},
    OptionData{"GBA/LinkType", "LinkType", _("Link cable type"),
               Option::Type::kInt},
#endif
    OptionData{"GBA/ROMDir", "", _("Directory to look for ROM files"),
               Option::Type::kString},

    /// General
    OptionData{"General/AutoLoadLastState", "",
               _("Automatically load last saved state"), Option::Type::kBool},
    OptionData{"General/BatteryDir", "",
               _("Directory to store game save files (relative paths are "
                 "relative to ROM; blank is config dir)"),
               Option::Type::kString},
    OptionData{"General/FreezeRecent", "", _("Freeze recent load list"),
               Option::Type::kBool},
    OptionData{"General/RecordingDir", "",
               _("Directory to store A/V and game recordings (relative paths "
                 "are relative to ROM)"),
               Option::Type::kString},
    OptionData{"General/RewindInterval", "",
               _("Number of seconds between rewind snapshots (0 to disable)"),
               Option::Type::kInt},
    OptionData{"General/ScreenshotDir", "",
               _("Directory to store screenshots (relative paths are relative "
                 "to ROM)"),
               Option::Type::kString},
    OptionData{"General/StateDir", "",
               _("Directory to store saved state files (relative paths are "
                 "relative to BatteryDir)"),
               Option::Type::kString},
    OptionData{"General/StatusBar", "StatusBar", _("Enable status bar"),
               Option::Type::kInt},

    /// Joypad
    OptionData{"Joypad/*/*", "",
               _("The parameter Joypad/<n>/<button> contains a comma-separated "
                 "list of key names which map to joypad #<n> button <button>.  "
                 "Button is one of Up, Down, Left, Right, A, B, L, R, Select, "
                 "Start, MotionUp, MotionDown, MotionLeft, MotionRight, AutoA, "
                 "AutoB, Speed, Capture, GS"),
               Option::Type::kNone},
    OptionData{"Joypad/AutofireThrottle", "",
               _("The autofire toggle period, in frames (1/60 s)"),
               Option::Type::kInt},
    OptionData{"Joypad/Default", "",
               _("The number of the stick to use in single-player mode"),
               Option::Type::kInt},

    /// Keyboard
    OptionData{"Keyboard/*", "",
               _("The parameter Keyboard/<cmd> contains a comma-separated list "
                 "of key names (e.g. Alt-Shift-F1).  When the named key is "
                 "pressed, the command <cmd> is executed."),
               Option::Type::kNone},

    // Core
    OptionData{"preferences/agbPrint", "AGBPrinter",
               _("Enable AGB debug print"), Option::Type::kInt},
    OptionData{"preferences/autoFrameSkip", "FrameSkipAuto",
               _("Auto skip frames."), Option::Type::kInt},
    OptionData{"preferences/autoPatch", "ApplyPatches",
               _("Apply IPS/UPS/IPF patches if found"), Option::Type::kInt},
    OptionData{"preferences/autoSaveLoadCheatList", "",
               _("Automatically save and load cheat list"),
               Option::Type::kBool},
    OptionData{"preferences/borderAutomatic", "",
               _("Automatically enable border for Super GameBoy games"),
               Option::Type::kInt},
    OptionData{"preferences/borderOn", "", _("Always enable border"),
               Option::Type::kInt},
    OptionData{"preferences/captureFormat", "", _("Screen capture file format"),
               Option::Type::kInt},
    OptionData{"preferences/cheatsEnabled", "", _("Enable cheats"),
               Option::Type::kInt},
#ifdef MMX
    OptionData{"preferences/enableMMX", "MMX", _("Enable MMX"),
               Option::Type::kInt},
#endif
    OptionData{"preferences/disableStatus", "NoStatusMsg",
               _("Disable on-screen status messages"), Option::Type::kInt},
    OptionData{"preferences/emulatorType", "", _("Type of system to emulate"),
               Option::Type::kInt},
    OptionData{"preferences/flashSize", "", _("Flash size 0 = 64KB 1 = 128KB"),
               Option::Type::kInt},
    OptionData{"preferences/frameSkip", "FrameSkip",
               _("Skip frames.  Values are 0-9 or -1 to skip automatically "
                 "based on time."),
               Option::Type::kInt},
    OptionData{"preferences/fsColorDepth", "",
               _("Fullscreen mode color depth (0 = any)"), Option::Type::kInt},
    OptionData{"preferences/fsFrequency", "",
               _("Fullscreen mode frequency (0 = any)"), Option::Type::kInt},
    OptionData{"preferences/fsHeight", "",
               _("Fullscreen mode height (0 = desktop)"), Option::Type::kInt},
    OptionData{"preferences/fsWidth", "",
               _("Fullscreen mode width (0 = desktop)"), Option::Type::kInt},
    OptionData{"preferences/gbPaletteOption", "", _("The palette to use"),
               Option::Type::kInt},
    OptionData{"preferences/gbPrinter", "Printer",
               _("Enable printer emulation"), Option::Type::kInt},
    OptionData{"preferences/gdbBreakOnLoad", "DebugGDBBreakOnLoad",
               _("Break into GDB after loading the game."), Option::Type::kInt},
    OptionData{"preferences/gdbPort", "DebugGDBPort",
               _("Port to connect GDB to."), Option::Type::kInt},
#ifndef NO_LINK
    OptionData{"preferences/LinkNumPlayers", "",
               _("Number of players in network"), Option::Type::kInt},
#endif
    OptionData{"preferences/maxScale", "",
               _("Maximum scale factor (0 = no limit)"), Option::Type::kInt},
    OptionData{"preferences/pauseWhenInactive", "PauseWhenInactive",
               _("Pause game when main window loses focus"),
               Option::Type::kInt},
    OptionData{"preferences/rtcEnabled", "RTC",
               _("Enable RTC (vba-over.ini override is rtcEnabled"),
               Option::Type::kInt},
    OptionData{"preferences/saveType", "",
               _("Native save (\"battery\") hardware type"),
               Option::Type::kInt},
    OptionData{"preferences/showSpeed", "", _("Show speed indicator"),
               Option::Type::kInt},
    OptionData{"preferences/showSpeedTransparent", "Transparent",
               _("Draw on-screen messages transparently"), Option::Type::kInt},
    OptionData{"preferences/skipBios", "SkipIntro",
               _("Skip BIOS initialization"), Option::Type::kInt},
    OptionData{"preferences/skipSaveGameCheats", "",
               _("Do not overwrite cheat list when loading state"),
               Option::Type::kInt},
    OptionData{"preferences/skipSaveGameBattery", "",
               _("Do not overwrite native (battery) save when loading state"),
               Option::Type::kInt},
    OptionData{"preferences/throttle", "",
               _("Throttle game speed, even when accelerated (0-450%, 0 = no "
                 "throttle)"),
               Option::Type::kUnsigned},
    OptionData{"preferences/speedupThrottle", "",
               _("Set throttle for speedup key (0-3000%, 0 = no throttle)"),
               Option::Type::kUnsigned},
    OptionData{"preferences/speedupFrameSkip", "",
               _("Number of frames to skip with speedup (instead of speedup "
                 "throttle)"),
               Option::Type::kUnsigned},
    OptionData{"preferences/speedupThrottleFrameSkip", "",
               _("Use frame skip for speedup throttle"), Option::Type::kBool},
    OptionData{"preferences/useBiosGB", "BootRomGB",
               _("Use the specified BIOS file for GB"), Option::Type::kInt},
    OptionData{"preferences/useBiosGBA", "BootRomEn",
               _("Use the specified BIOS file"), Option::Type::kInt},
    OptionData{"preferences/useBiosGBC", "BootRomGBC",
               _("Use the specified BIOS file for GBC"), Option::Type::kInt},
    OptionData{"preferences/vsync", "VSync", _("Wait for vertical sync"),
               Option::Type::kInt},

    /// Geometry
    OptionData{"geometry/fullScreen", "Fullscreen",
               _("Enter fullscreen mode at startup"), Option::Type::kInt},
    OptionData{"geometry/isMaximized", "Maximized", _("Window maximized"),
               Option::Type::kInt},
    OptionData{"geometry/windowHeight", "Height", _("Window height at startup"),
               Option::Type::kUnsigned},
    OptionData{"geometry/windowWidth", "Width", _("Window width at startup"),
               Option::Type::kUnsigned},
    OptionData{"geometry/windowX", "X", _("Window axis X position at startup"),
               Option::Type::kInt},
    OptionData{"geometry/windowY", "Y", _("Window axis Y position at startup"),
               Option::Type::kInt},

    /// UI
    OptionData{
        "ui/allowKeyboardBackgroundInput", "AllowKeyboardBackgroundInput",
        _("Capture key events while on background"), Option::Type::kBool},
    OptionData{
        "ui/allowJoystickBackgroundInput", "AllowJoystickBackgroundInput",
        _("Capture joy events while on background"), Option::Type::kBool},
    OptionData{"ui/hideMenuBar", "", _("Hide menu bar when mouse is inactive"),
               Option::Type::kBool},

    /// Sound
    OptionData{"Sound/AudioAPI", "",
               _("Sound API; if unsupported, default API will be used"),
               Option::Type::kAudioApi},
    OptionData{"Sound/AudioDevice", "",
               _("Device ID of chosen audio device for chosen driver"),
               Option::Type::kString},
    OptionData{"Sound/Buffers", "", _("Number of sound buffers"),
               Option::Type::kInt},
    OptionData{"Sound/Enable", "", _("Bit mask of sound channels to enable"),
               Option::Type::kInt},
    OptionData{"Sound/GBAFiltering", "", _("GBA sound filtering (%)"),
               Option::Type::kInt},
    OptionData{"Sound/GBAInterpolation", "GBASoundInterpolation",
               _("GBA sound interpolation"), Option::Type::kBool},
    OptionData{"Sound/GBDeclicking", "GBDeclicking", _("GB sound declicking"),
               Option::Type::kBool},
    OptionData{"Sound/GBEcho", "", _("GB echo effect (%)"), Option::Type::kInt},
    OptionData{"Sound/GBEnableEffects", "GBEnhanceSound",
               _("Enable GB sound effects"), Option::Type::kBool},
    OptionData{"Sound/GBStereo", "", _("GB stereo effect (%)"),
               Option::Type::kInt},
    OptionData{"Sound/GBSurround", "GBSurround",
               _("GB surround sound effect (%)"), Option::Type::kBool},
    OptionData{"Sound/Quality", "", _("Sound sample rate (kHz)"),
               Option::Type::kSoundQuality},
    OptionData{"Sound/Volume", "", _("Sound volume (%)"), Option::Type::kInt},

    // Last. This should never be used, it actually maps to OptionID::kLast.
    // This is to prevent a memory access violation error in case something
    // attempts to instantiate a OptionID::kLast. It will trigger a check
    // in the Option constructor, but that is after the constructor has
    // accessed this entry.
    OptionData{"", "", "", Option::Type::kNone},
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
    assert(size_value >= 0 && size_value < kNbFilters);
    return kFilterStrings[size_value];
}

wxString InterframeToString(const Interframe& value) {
    const size_t size_value = static_cast<size_t>(value);
    assert(size_value >= 0 && size_value < kNbInterframes);
    return kInterframeStrings[size_value];
}

wxString RenderMethodToString(const RenderMethod& value) {
    const size_t size_value = static_cast<size_t>(value);
    assert(size_value >= 0 && size_value < kNbRenderMethods);
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
            kStringToFilter.emplace(kFilterStrings[i],
                                    static_cast<Filter>(i));
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
