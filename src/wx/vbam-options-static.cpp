#include "vbam-options.h"

// Helper implementation file to define and compile all of these huge constants
// separately. These should not be updated very often, so having these in a
// separate file improves incremental build time.

#include <algorithm>
#include <wx/log.h>

#include "../common/ConfigManager.h"
#include "../gb/gbGlobals.h"
#include "opts.h"

#define VBAM_OPTIONS_INTERNAL_INCLUDE
#include "vbam-options-internal.h"
#undef VBAM_OPTIONS_INTERNAL_INCLUDE

namespace {

// This enum must be kept in sync with the one in wxvbam.h
// TODO: These 2 enums should be unified and a validator created for this enum.
enum class FilterFunction {
    kNone,
    k2xsai,
    kSuper2xsai,
    kSupereagle,
    kPixelate,
    kAdvmame,
    kBilinear,
    kBilinearplus,
    kScanlines,
    kTvmode,
    kHQ2x,
    kLQ2x,
    kSimple2x,
    kSimple3x,
    kHQ3x,
    kSimple4x,
    kHQ4x,
    kXbrz2x,
    kXbrz3x,
    kXbrz4x,
    kXbrz5x,
    kXbrz6x,
    kPlugin, // This must always be last.

    // Do not add anything under here.
    kLast,
};
constexpr size_t kNbFilterFunctions = static_cast<size_t>(FilterFunction::kLast);

// These MUST follow the same order as the definitions of the enum above.
// Adding an option without adding to this array will result in a compiler
// error since kNbFilterFunctions is automatically updated.
static const std::array<wxString, kNbFilterFunctions> kFilterStrings = {
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

// This enum must be kept in sync with the one in wxvbam.h
// TODO: These 2 enums should be unified and a validator created for this enum.
enum class Interframe {
    kNone = 0,
    kSmart,
    kMotionBlur,

    // Do not add anything under here.
    kLast,
};
constexpr size_t kNbInterframes = static_cast<size_t>(Interframe::kLast);

// These MUST follow the same order as the definitions of the enum above.
// Adding an option without adding to this array will result in a compiler
// error since kNbInterframes is automatically updated.
static const std::array<wxString, kNbInterframes> kInterframeStrings = {
    "none",
    "smart",
    "motionblur",
};

// This enum must be kept in sync with the one in wxvbam.h
// TODO: These 2 enums should be unified and a validator created for this enum.
enum class RenderMethod {
    kSimple = 0,
    kOpenGL,
#ifdef __WXMSW__
    kDirect3d,
#elif defined(__WXMAC__)
    kQuartz2d,
#endif

    // Do not add anything under here.
    kLast,
};
constexpr size_t kNbRenderMethods = static_cast<size_t>(RenderMethod::kLast);

// These MUST follow the same order as the definitions of the enum above.
// Adding an option without adding to this array will result in a compiler
// error since kNbRenderMethods is automatically updated.
static const std::array<wxString, kNbRenderMethods> kRenderMethodStrings = {
    "simple",
    "opengl",
#ifdef __WXMSW__
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

// Builds the "all enum values" string for a given array defined above.
template<std::size_t SIZE>
wxString AllEnumValuesForArray(const std::array<wxString, SIZE>& input) {
    // 15 characters per option is a reasonable higher bound. Probably.
    static constexpr size_t kMaxOptionLength = 15u;

    wxString all_options;
    all_options.reserve(kMaxOptionLength * SIZE);

    std::for_each(input.cbegin(), input.cend(), [&all_options](const auto& elt) {
        all_options.append(elt);
        all_options.append('|');
    });
    // Remove last value
    all_options.RemoveLast(1u);
    return all_options;
}

}  // namespace

// static
std::array<VbamOption, kNbOptions>& VbamOption::AllOptions() {
    // These MUST follow the same order as the definitions in VbamOptionID.
    // Adding an option without adding to this array will result in a compiler
    // error since kNbOptions is automatically updated.
    // This will be initialized on the first call, in load_opts(), ensuring the
    // translation initialization has already happened.
    static std::array<VbamOption, kNbOptions> g_all_opts = {
        /// Display
        VbamOption(VbamOptionID::kDisplayBilinear, &gopts.bilinear),
        VbamOption(VbamOptionID::kDisplayFilter, &gopts.filter),
        VbamOption(VbamOptionID::kDisplayFilterPlugin, &gopts.filter_plugin),
        VbamOption(VbamOptionID::kDisplayIFB, &gopts.ifb),
        VbamOption(VbamOptionID::kDisplayKeepOnTop, &gopts.keep_on_top),
        VbamOption(VbamOptionID::kDisplayMaxThreads, &gopts.max_threads, 1, 256),
        VbamOption(VbamOptionID::kDisplayRenderMethod, &gopts.render_method),
        VbamOption(VbamOptionID::kDisplayScale, &gopts.video_scale, 1, 6),
        VbamOption(VbamOptionID::kDisplayStretch, &gopts.retain_aspect),

        /// GB
        VbamOption(VbamOptionID::kGBBiosFile, &gopts.gb_bios),
        VbamOption(VbamOptionID::kGBColorOption, &gbColorOption, 0, 1),
        VbamOption(VbamOptionID::kGBColorizerHack, &colorizerHack, 0, 1),
        VbamOption(VbamOptionID::kGBLCDFilter, &gbLcdFilter),
        VbamOption(VbamOptionID::kGBGBCBiosFile, &gopts.gbc_bios),
        VbamOption(VbamOptionID::kGBPalette0, systemGbPalette),
        VbamOption(VbamOptionID::kGBPalette1, systemGbPalette + 8),
        VbamOption(VbamOptionID::kGBPalette2, systemGbPalette + 16),
        VbamOption(VbamOptionID::kGBPrintAutoPage, &gopts.print_auto_page),
        VbamOption(VbamOptionID::kGBPrintScreenCap, &gopts.print_screen_cap),
        VbamOption(VbamOptionID::kGBROMDir, &gopts.gb_rom_dir),
        VbamOption(VbamOptionID::kGBGBCROMDir, &gopts.gbc_rom_dir),

        /// GBA
        VbamOption(VbamOptionID::kGBABiosFile, &gopts.gba_bios),
        VbamOption(VbamOptionID::kGBALCDFilter, &gbaLcdFilter),
    #ifndef NO_LINK
        VbamOption(VbamOptionID::kGBALinkAuto, &gopts.link_auto),
        VbamOption(VbamOptionID::kGBALinkFast, &linkHacks, 0, 1),
        VbamOption(VbamOptionID::kGBALinkHost, &gopts.link_host),
        VbamOption(VbamOptionID::kGBAServerIP, &gopts.server_ip),
        VbamOption(VbamOptionID::kGBALinkPort, &gopts.link_port, 0, 65535),
        VbamOption(VbamOptionID::kGBALinkProto, &gopts.link_proto, 0, 1),
        VbamOption(VbamOptionID::kGBALinkTimeout, &linkTimeout, 0, 9999999),
        VbamOption(VbamOptionID::kGBALinkType, &gopts.gba_link_type, 0, 5),
    #endif
        VbamOption(VbamOptionID::kGBAROMDir, &gopts.gba_rom_dir),

        /// General
        VbamOption(VbamOptionID::kGeneralAutoLoadLastState, &gopts.autoload_state),
        VbamOption(VbamOptionID::kGeneralBatteryDir, &gopts.battery_dir),
        VbamOption(VbamOptionID::kGeneralFreezeRecent, &gopts.recent_freeze),
        VbamOption(VbamOptionID::kGeneralRecordingDir, &gopts.recording_dir),
        VbamOption(VbamOptionID::kGeneralRewindInterval, &gopts.rewind_interval, 0, 600),
        VbamOption(VbamOptionID::kGeneralScreenshotDir, &gopts.scrshot_dir),
        VbamOption(VbamOptionID::kGeneralStateDir, &gopts.state_dir),
        VbamOption(VbamOptionID::kGeneralStatusBar, &gopts.statusbar, 0, 1),

        /// Joypad
        VbamOption(VbamOptionID::kJoypad),
        VbamOption(VbamOptionID::kJoypadAutofireThrottle, &gopts.autofire_rate, 1, 1000),
        VbamOption(VbamOptionID::kJoypadDefault, &gopts.default_stick, 1, 4),

        /// Keyboard
        VbamOption(VbamOptionID::kKeyboard),

        // Core
        VbamOption(VbamOptionID::kpreferencesagbPrint, &agbPrint, 0, 1),
        VbamOption(VbamOptionID::kpreferencesautoFrameSkip, &autoFrameSkip, 0, 1),
        VbamOption(VbamOptionID::kpreferencesautoPatch, &autoPatch, 0, 1),
        VbamOption(VbamOptionID::kpreferencesautoSaveLoadCheatList, &gopts.autoload_cheats),
        VbamOption(VbamOptionID::kpreferencesborderAutomatic, &gbBorderAutomatic, 0, 1),
        VbamOption(VbamOptionID::kpreferencesborderOn, &gbBorderOn, 0, 1),
        VbamOption(VbamOptionID::kpreferencescaptureFormat, &captureFormat, 0, 1),
        VbamOption(VbamOptionID::kpreferencescheatsEnabled, &cheatsEnabled, 0, 1),
    #ifdef MMX
        VbamOption(VbamOptionID::kpreferencesenableMMX, &enableMMX, 0, 1),
    #endif
        VbamOption(VbamOptionID::kpreferencesdisableStatus, &disableStatusMessages, 0, 1),
        VbamOption(VbamOptionID::kpreferencesemulatorType, &gbEmulatorType, 0, 5),
        VbamOption(VbamOptionID::kpreferencesflashSize, &optFlashSize, 0, 1),
        VbamOption(VbamOptionID::kpreferencesframeSkip, &frameSkip, -1, 9),
        VbamOption(VbamOptionID::kpreferencesfsColorDepth, &fsColorDepth, 0, 999),
        VbamOption(VbamOptionID::kpreferencesfsFrequency, &fsFrequency, 0, 999),
        VbamOption(VbamOptionID::kpreferencesfsHeight, &fsHeight, 0, 99999),
        VbamOption(VbamOptionID::kpreferencesfsWidth, &fsWidth, 0, 99999),
        VbamOption(VbamOptionID::kpreferencesgbPaletteOption, &gbPaletteOption, 0, 2),
        VbamOption(VbamOptionID::kpreferencesgbPrinter, &winGbPrinterEnabled, 0, 1),
        VbamOption(VbamOptionID::kpreferencesgdbBreakOnLoad, &gdbBreakOnLoad, 0, 1),
        VbamOption(VbamOptionID::kpreferencesgdbPort, &gdbPort, 0, 65535),
    #ifndef NO_LINK
        VbamOption(VbamOptionID::kpreferencesLinkNumPlayers, &linkNumPlayers, 2, 4),
    #endif
        VbamOption(VbamOptionID::kpreferencesmaxScale, &maxScale, 0, 100),
        VbamOption(VbamOptionID::kpreferencespauseWhenInactive, &pauseWhenInactive, 0, 1),
        VbamOption(VbamOptionID::kpreferencesrtcEnabled, &rtcEnabled, 0, 1),
        VbamOption(VbamOptionID::kpreferencessaveType, &cpuSaveType, 0, 5),
        VbamOption(VbamOptionID::kpreferencesshowSpeed, &showSpeed, 0, 2),
        VbamOption(VbamOptionID::kpreferencesshowSpeedTransparent, &showSpeedTransparent, 0, 1),
        VbamOption(VbamOptionID::kpreferencesskipBios, &skipBios, 0, 1),
        VbamOption(VbamOptionID::kpreferencesskipSaveGameCheats, &skipSaveGameCheats, 0, 1),
        VbamOption(VbamOptionID::kpreferencesskipSaveGameBattery, &skipSaveGameBattery, 0, 1),
        VbamOption(VbamOptionID::kpreferencesthrottle, &throttle, 0, 450),
        VbamOption(VbamOptionID::kpreferencesspeedupThrottle, &speedup_throttle, 0, 3000),
        VbamOption(VbamOptionID::kpreferencesspeedupFrameSkip, &speedup_frame_skip, 0, 300),
        VbamOption(VbamOptionID::kpreferencesspeedupThrottleFrameSkip, &speedup_throttle_frame_skip),
        VbamOption(VbamOptionID::kpreferencesuseBiosGB, &useBiosFileGB, 0, 1),
        VbamOption(VbamOptionID::kpreferencesuseBiosGBA, &useBiosFileGBA, 0, 1),
        VbamOption(VbamOptionID::kpreferencesuseBiosGBC, &useBiosFileGBC, 0, 1),
        VbamOption(VbamOptionID::kpreferencesvsync, &vsync, 0, 1),

        /// Geometry
        VbamOption(VbamOptionID::kgeometryfullScreen, &fullScreen, 0, 1),
        VbamOption(VbamOptionID::kgeometryisMaximized, &windowMaximized, 0, 1),
        VbamOption(VbamOptionID::kgeometrywindowHeight, &windowHeight, 0, 99999),
        VbamOption(VbamOptionID::kgeometrywindowWidth, &windowWidth, 0, 99999),
        VbamOption(VbamOptionID::kgeometrywindowX, &windowPositionX, -1, 99999),
        VbamOption(VbamOptionID::kgeometrywindowY, &windowPositionY, -1, 99999),

        /// UI
        VbamOption(VbamOptionID::kuiallowKeyboardBackgroundInput, &allowKeyboardBackgroundInput),
        VbamOption(VbamOptionID::kuiallowJoystickBackgroundInput, &allowJoystickBackgroundInput),
        VbamOption(VbamOptionID::kuihideMenuBar, &gopts.hide_menu_bar),

        /// Sound
        VbamOption(VbamOptionID::kSoundAudioAPI, &gopts.audio_api),
        VbamOption(VbamOptionID::kSoundAudioDevice, &gopts.audio_dev),
        VbamOption(VbamOptionID::kSoundBuffers, &gopts.audio_buffers, 2, 10),
        VbamOption(VbamOptionID::kSoundEnable, &gopts.sound_en, 0, 0x30f),
        VbamOption(VbamOptionID::kSoundGBAFiltering, &gopts.gba_sound_filter, 0, 100),
        VbamOption(VbamOptionID::kSoundGBAInterpolation, &gopts.soundInterpolation),
        VbamOption(VbamOptionID::kSoundGBDeclicking, &gopts.gb_declick),
        VbamOption(VbamOptionID::kSoundGBEcho, &gopts.gb_echo, 0, 100),
        VbamOption(VbamOptionID::kSoundGBEnableEffects, &gopts.gb_effects_config_enabled),
        VbamOption(VbamOptionID::kSoundGBStereo, &gopts.gb_stereo, 0, 100),
        VbamOption(VbamOptionID::kSoundGBSurround, &gopts.gb_effects_config_surround),
        VbamOption(VbamOptionID::kSoundQuality, &gopts.sound_qual),
        VbamOption(VbamOptionID::kSoundVolume, &gopts.sound_vol, 0, 200),
    };
    return g_all_opts;
}

namespace internal {

// These MUST follow the same order as the definitions in VbamOptionID.
// Adding an option without adding to this array will result in a compiler
// error since kNbOptions is automatically updated.
const std::array<VbamOptionData, kNbOptions + 1> kAllOptionsData = {
    /// Display
    VbamOptionData { "Display/Bilinear", "Bilinear", _("Use bilinear filter with 3d renderer"), VbamOption::Type::kBool },
    VbamOptionData { "Display/Filter", "", _("Full-screen filter to apply"), VbamOption::Type::kFilter },
    VbamOptionData { "Display/FilterPlugin", "", _("Filter plugin library"), VbamOption::Type::kString },
    VbamOptionData { "Display/IFB", "", _("Interframe blending function"), VbamOption::Type::kInterframe },
    VbamOptionData { "Display/KeepOnTop", "KeepOnTop", _("Keep window on top"), VbamOption::Type::kBool },
    VbamOptionData { "Display/MaxThreads", "Multithread", _("Maximum number of threads to run filters in"), VbamOption::Type::kInt },
    VbamOptionData { "Display/RenderMethod", "", _("Render method; if unsupported, simple method will be used"), VbamOption::Type::kRenderMethod },
    VbamOptionData { "Display/Scale", "", _("Default scale factor"), VbamOption::Type::kDouble },
    VbamOptionData { "Display/Stretch", "RetainAspect", _("Retain aspect ratio when resizing"), VbamOption::Type::kBool },

    /// GB
    VbamOptionData { "GB/BiosFile", "", _("BIOS file to use for GB, if enabled"), VbamOption::Type::kString },
    VbamOptionData { "GB/ColorOption", "GBColorOption", _("GB color enhancement, if enabled"), VbamOption::Type::kInt },
    VbamOptionData { "GB/ColorizerHack", "ColorizerHack", _("Enable DX Colorization Hacks"), VbamOption::Type::kInt },
    VbamOptionData { "GB/LCDFilter", "GBLcdFilter", _("Apply LCD filter, if enabled"), VbamOption::Type::kBool },
    VbamOptionData { "GB/GBCBiosFile", "", _("BIOS file to use for GBC, if enabled"), VbamOption::Type::kString },
    VbamOptionData { "GB/Palette0", "", _("The default palette, as 8 comma-separated 4-digit hex integers (rgb555)."), VbamOption::Type::kGbPalette },
    VbamOptionData { "GB/Palette1", "", _("The first user palette, as 8 comma-separated 4-digit hex integers (rgb555)."), VbamOption::Type::kGbPalette },
    VbamOptionData { "GB/Palette2", "", _("The second user palette, as 8 comma-separated 4-digit hex integers (rgb555)."), VbamOption::Type::kGbPalette },
    VbamOptionData { "GB/PrintAutoPage", "PrintGather", _("Automatically gather a full page before printing"), VbamOption::Type::kBool },
    VbamOptionData { "GB/PrintScreenCap", "PrintSnap", _("Automatically save printouts as screen captures with -print suffix"), VbamOption::Type::kBool },
    VbamOptionData { "GB/ROMDir", "", _("Directory to look for ROM files"), VbamOption::Type::kString },
    VbamOptionData { "GB/GBCROMDir", "", _("Directory to look for GBC ROM files"), VbamOption::Type::kString },

    /// GBA
    VbamOptionData { "GBA/BiosFile", "", _("BIOS file to use, if enabled"), VbamOption::Type::kString },
    VbamOptionData { "GBA/LCDFilter", "GBALcdFilter", _("Apply LCD filter, if enabled"), VbamOption::Type::kBool },
#ifndef NO_LINK
    VbamOptionData { "GBA/LinkAuto", "LinkAuto", _("Enable link at boot"), VbamOption::Type::kBool },
    VbamOptionData { "GBA/LinkFast", "SpeedOn", _("Enable faster network protocol by default"), VbamOption::Type::kInt },
    VbamOptionData { "GBA/LinkHost", "", _("Default network link client host"), VbamOption::Type::kString },
    VbamOptionData { "GBA/ServerIP", "", _("Default network link server IP to bind"), VbamOption::Type::kString },
    VbamOptionData { "GBA/LinkPort", "", _("Default network link port (server and client)"), VbamOption::Type::kUnsigned },
    VbamOptionData { "GBA/LinkProto", "LinkProto", _("Default network protocol"), VbamOption::Type::kInt },
    VbamOptionData { "GBA/LinkTimeout", "LinkTimeout", _("Link timeout (ms)"), VbamOption::Type::kInt },
    VbamOptionData { "GBA/LinkType", "LinkType", _("Link cable type"), VbamOption::Type::kInt },
#endif
    VbamOptionData { "GBA/ROMDir", "", _("Directory to look for ROM files"), VbamOption::Type::kString },

    /// General
    VbamOptionData { "General/AutoLoadLastState", "", _("Automatically load last saved state"), VbamOption::Type::kBool },
    VbamOptionData { "General/BatteryDir", "", _("Directory to store game save files (relative paths are relative to ROM; blank is config dir)"), VbamOption::Type::kString },
    VbamOptionData { "General/FreezeRecent", "", _("Freeze recent load list"), VbamOption::Type::kBool },
    VbamOptionData { "General/RecordingDir", "", _("Directory to store A/V and game recordings (relative paths are relative to ROM)"), VbamOption::Type::kString },
    VbamOptionData { "General/RewindInterval", "", _("Number of seconds between rewind snapshots (0 to disable)"), VbamOption::Type::kInt },
    VbamOptionData { "General/ScreenshotDir", "", _("Directory to store screenshots (relative paths are relative to ROM)"), VbamOption::Type::kString },
    VbamOptionData { "General/StateDir", "", _("Directory to store saved state files (relative paths are relative to BatteryDir)"), VbamOption::Type::kString },
    VbamOptionData { "General/StatusBar", "StatusBar", _("Enable status bar"), VbamOption::Type::kInt },

    /// Joypad
    VbamOptionData { "Joypad/*/*", "", _("The parameter Joypad/<n>/<button> contains a comma-separated list of key names which map to joypad #<n> button <button>.  Button is one of Up, Down, Left, Right, A, B, L, R, Select, Start, MotionUp, MotionDown, MotionLeft, MotionRight, AutoA, AutoB, Speed, Capture, GS"), VbamOption::Type::kNone },
    VbamOptionData { "Joypad/AutofireThrottle", "", _("The autofire toggle period, in frames (1/60 s)"), VbamOption::Type::kInt },
    VbamOptionData { "Joypad/Default", "", _("The number of the stick to use in single-player mode"), VbamOption::Type::kInt },

    /// Keyboard
    VbamOptionData { "Keyboard/*", "", _("The parameter Keyboard/<cmd> contains a comma-separated list of key names (e.g. Alt-Shift-F1).  When the named key is pressed, the command <cmd> is executed."), VbamOption::Type::kNone },

    // Core
    VbamOptionData { "preferences/agbPrint", "AGBPrinter", _("Enable AGB debug print"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/autoFrameSkip", "FrameSkipAuto", _("Auto skip frames."), VbamOption::Type::kInt },
    VbamOptionData { "preferences/autoPatch", "ApplyPatches", _("Apply IPS/UPS/IPF patches if found"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/autoSaveLoadCheatList", "", _("Automatically save and load cheat list"), VbamOption::Type::kBool },
    VbamOptionData { "preferences/borderAutomatic", "", _("Automatically enable border for Super GameBoy games"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/borderOn", "", _("Always enable border"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/captureFormat", "", _("Screen capture file format"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/cheatsEnabled", "", _("Enable cheats"), VbamOption::Type::kInt },
#ifdef MMX
    VbamOptionData { "preferences/enableMMX", "MMX", _("Enable MMX"), VbamOption::Type::kInt },
#endif
    VbamOptionData { "preferences/disableStatus", "NoStatusMsg", _("Disable on-screen status messages"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/emulatorType", "", _("Type of system to emulate"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/flashSize", "", _("Flash size 0 = 64KB 1 = 128KB"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/frameSkip", "FrameSkip", _("Skip frames.  Values are 0-9 or -1 to skip automatically based on time."), VbamOption::Type::kInt },
    VbamOptionData { "preferences/fsColorDepth", "", _("Fullscreen mode color depth (0 = any)"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/fsFrequency", "", _("Fullscreen mode frequency (0 = any)"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/fsHeight", "", _("Fullscreen mode height (0 = desktop)"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/fsWidth", "", _("Fullscreen mode width (0 = desktop)"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/gbPaletteOption", "", _("The palette to use"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/gbPrinter", "Printer", _("Enable printer emulation"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/gdbBreakOnLoad", "DebugGDBBreakOnLoad", _("Break into GDB after loading the game."), VbamOption::Type::kInt },
    VbamOptionData { "preferences/gdbPort", "DebugGDBPort", _("Port to connect GDB to."), VbamOption::Type::kInt },
#ifndef NO_LINK
    VbamOptionData { "preferences/LinkNumPlayers", "", _("Number of players in network"), VbamOption::Type::kInt },
#endif
    VbamOptionData { "preferences/maxScale", "", _("Maximum scale factor (0 = no limit)"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/pauseWhenInactive", "PauseWhenInactive", _("Pause game when main window loses focus"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/rtcEnabled", "RTC", _("Enable RTC (vba-over.ini override is rtcEnabled"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/saveType", "", _("Native save (\"battery\") hardware type"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/showSpeed", "", _("Show speed indicator"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/showSpeedTransparent", "Transparent", _("Draw on-screen messages transparently"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/skipBios", "SkipIntro", _("Skip BIOS initialization"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/skipSaveGameCheats", "", _("Do not overwrite cheat list when loading state"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/skipSaveGameBattery", "", _("Do not overwrite native (battery) save when loading state"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/throttle", "", _("Throttle game speed, even when accelerated (0-450%, 0 = no throttle)"), VbamOption::Type::kUnsigned },
    VbamOptionData { "preferences/speedupThrottle", "", _("Set throttle for speedup key (0-3000%, 0 = no throttle)"), VbamOption::Type::kUnsigned },
    VbamOptionData { "preferences/speedupFrameSkip", "", _("Number of frames to skip with speedup (instead of speedup throttle)"), VbamOption::Type::kUnsigned },
    VbamOptionData { "preferences/speedupThrottleFrameSkip", "", _("Use frame skip for speedup throttle"), VbamOption::Type::kBool },
    VbamOptionData { "preferences/useBiosGB", "BootRomGB", _("Use the specified BIOS file for GB"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/useBiosGBA", "BootRomEn", _("Use the specified BIOS file"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/useBiosGBC", "BootRomGBC", _("Use the specified BIOS file for GBC"), VbamOption::Type::kInt },
    VbamOptionData { "preferences/vsync", "VSync", _("Wait for vertical sync"), VbamOption::Type::kInt },

    /// Geometry
    VbamOptionData { "geometry/fullScreen", "Fullscreen", _("Enter fullscreen mode at startup"), VbamOption::Type::kInt },
    VbamOptionData { "geometry/isMaximized", "Maximized", _("Window maximized"), VbamOption::Type::kInt },
    VbamOptionData { "geometry/windowHeight", "Height", _("Window height at startup"), VbamOption::Type::kUnsigned },
    VbamOptionData { "geometry/windowWidth", "Width", _("Window width at startup"), VbamOption::Type::kUnsigned },
    VbamOptionData { "geometry/windowX", "X", _("Window axis X position at startup"), VbamOption::Type::kInt },
    VbamOptionData { "geometry/windowY", "Y", _("Window axis Y position at startup"), VbamOption::Type::kInt },

    /// UI
    VbamOptionData { "ui/allowKeyboardBackgroundInput", "AllowKeyboardBackgroundInput", _("Capture key events while on background"), VbamOption::Type::kBool },
    VbamOptionData { "ui/allowJoystickBackgroundInput", "AllowJoystickBackgroundInput", _("Capture joy events while on background"), VbamOption::Type::kBool },
    VbamOptionData { "ui/hideMenuBar", "", _("Hide menu bar when mouse is inactive"), VbamOption::Type::kBool },

    /// Sound
    VbamOptionData { "Sound/AudioAPI", "", _("Sound API; if unsupported, default API will be used"), VbamOption::Type::kAudioApi },
    VbamOptionData { "Sound/AudioDevice", "", _("Device ID of chosen audio device for chosen driver"), VbamOption::Type::kString },
    VbamOptionData { "Sound/Buffers", "", _("Number of sound buffers"), VbamOption::Type::kInt },
    VbamOptionData { "Sound/Enable", "", _("Bit mask of sound channels to enable"), VbamOption::Type::kInt },
    VbamOptionData { "Sound/GBAFiltering", "", _("GBA sound filtering (%)"), VbamOption::Type::kInt },
    VbamOptionData { "Sound/GBAInterpolation", "GBASoundInterpolation", _("GBA sound interpolation"), VbamOption::Type::kBool },
    VbamOptionData { "Sound/GBDeclicking", "GBDeclicking", _("GB sound declicking"), VbamOption::Type::kBool },
    VbamOptionData { "Sound/GBEcho", "", _("GB echo effect (%)"), VbamOption::Type::kInt },
    VbamOptionData { "Sound/GBEnableEffects", "GBEnhanceSound", _("Enable GB sound effects"), VbamOption::Type::kBool },
    VbamOptionData { "Sound/GBStereo", "", _("GB stereo effect (%)"), VbamOption::Type::kInt },
    VbamOptionData { "Sound/GBSurround", "GBSurround", _("GB surround sound effect (%)"), VbamOption::Type::kBool },
    VbamOptionData { "Sound/Quality", "", _("Sound sample rate (kHz)"), VbamOption::Type::kSoundQuality },
    VbamOptionData { "Sound/Volume", "", _("Sound volume (%)"), VbamOption::Type::kInt },

    // Last. This should never be used, it actually maps to VbamOptionID::kLast.
    // This is to prevent a memory access violation error in case something
    // attempts to instantiate a VbamOptionID::kLast. It will trigger a check
    // in the VbamOption constructor, but that is after the constructor has
    // accessed this entry.
    VbamOptionData { "", "", "", VbamOption::Type::kNone },
};

nonstd::optional<VbamOptionID> StringToOptionId(const wxString& input) {
    static std::map<wxString, VbamOptionID> kStringToOptionId;
    if (kStringToOptionId.empty()) {
        for (size_t i = 0; i < kNbOptions; i++) {
            kStringToOptionId.emplace(
                kAllOptionsData[i].config_name, static_cast<VbamOptionID>(i));
        }
        assert(kStringToOptionId.size() == kNbOptions);
    }

    const auto iter = kStringToOptionId.find(input);
    if (iter == kStringToOptionId.end()) {
        return nonstd::nullopt;
    }
    return iter->second;
}

wxString FilterToString(int value) {
    assert(value >= 0 && static_cast<size_t>(value) < kNbFilterFunctions);
    return kFilterStrings[value];
}

wxString InterframeToString(int value) {
    assert(value >= 0 && static_cast<size_t>(value) < kNbInterframes);
    return kInterframeStrings[value];
}

wxString RenderMethodToString(int value) {
    assert(value >= 0 && static_cast<size_t>(value) < kNbRenderMethods);
    return kRenderMethodStrings[value];
}

wxString AudioApiToString(int value) {
    assert(value >= 0 && static_cast<size_t>(value) < kNbAudioApis);
    return kAudioApiStrings[value];
}

wxString SoundQualityToString(int value) {
    assert(value >= 0 && static_cast<size_t>(value) < kNbSoundQualities);
    return kSoundQualityStrings[value];
}

int StringToFilter(const wxString& config_name, const wxString& input) {
    static std::map<wxString, FilterFunction> kStringToFilter;
    if (kStringToFilter.empty()) {
        for (size_t i = 0; i < kNbFilterFunctions; i++) {
            kStringToFilter.emplace(
                kFilterStrings[i], static_cast<FilterFunction>(i));
        }
        assert(kStringToFilter.size() == kNbFilterFunctions);
    }

    const auto iter = kStringToFilter.find(input);
    if (iter == kStringToFilter.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
             input,
             config_name,
             AllEnumValuesForType(VbamOption::Type::kFilter));
        return 0;
    }
    return static_cast<int>(iter->second);
}

int StringToInterframe(const wxString& config_name, const wxString& input) {
    static std::map<wxString, Interframe> kStringToInterframe;
    if (kStringToInterframe.empty()) {
        for (size_t i = 0; i < kNbInterframes; i++) {
            kStringToInterframe.emplace(
                kInterframeStrings[i], static_cast<Interframe>(i));
        }
        assert(kStringToInterframe.size() == kNbInterframes);
    }

    const auto iter = kStringToInterframe.find(input);
    if (iter == kStringToInterframe.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
             input,
             config_name,
             AllEnumValuesForType(VbamOption::Type::kInterframe));
        return 0;
    }
    return static_cast<int>(iter->second);
}

int StringToRenderMethod(const wxString& config_name, const wxString& input) {
    static std::map<wxString, RenderMethod> kStringToRenderMethod;
    if (kStringToRenderMethod.empty()) {
        for (size_t i = 0; i < kNbRenderMethods; i++) {
            kStringToRenderMethod.emplace(kRenderMethodStrings[i], static_cast<RenderMethod>(i));
        }
        assert(kStringToRenderMethod.size() == kNbRenderMethods);
    }

    const auto iter = kStringToRenderMethod.find(input);
    if (iter == kStringToRenderMethod.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
             input,
             config_name,
             AllEnumValuesForType(VbamOption::Type::kRenderMethod));
        return 0;
    }
    return static_cast<int>(iter->second);
}

int StringToAudioApi(const wxString& config_name, const wxString& input) {
    static std::map<wxString, AudioApi> kStringToAudioApi;
    if (kStringToAudioApi.empty()) {
        for (size_t i = 0; i < kNbAudioApis; i++) {
            kStringToAudioApi.emplace(kAudioApiStrings[i], static_cast<AudioApi>(i));
        }
        assert(kStringToAudioApi.size() == kNbAudioApis);
    }

    const auto iter = kStringToAudioApi.find(input);
    if (iter == kStringToAudioApi.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
             input,
             config_name,
             AllEnumValuesForType(VbamOption::Type::kAudioApi));
        return 0;
    }
    return static_cast<int>(iter->second);
}

int StringToSoundQuality(const wxString& config_name, const wxString& input) {
    static std::map<wxString, SoundQuality> kStringToSoundQuality;
    if (kStringToSoundQuality.empty()) {
        for (size_t i = 0; i < kNbSoundQualities; i++) {
            kStringToSoundQuality.emplace(kSoundQualityStrings[i], static_cast<SoundQuality>(i));
        }
        assert(kStringToSoundQuality.size() == kNbSoundQualities);
    }

    const auto iter = kStringToSoundQuality.find(input);
    if (iter == kStringToSoundQuality.end()) {
        wxLogWarning(_("Invalid value %s for option %s; valid values are %s"),
             input,
             config_name,
             AllEnumValuesForType(VbamOption::Type::kSoundQuality));
        return 0;
    }
    return static_cast<int>(iter->second);

}

wxString AllEnumValuesForType(VbamOption::Type type) {
    switch (type) {
    case VbamOption::Type::kFilter: {
        static const wxString kAllFilterValues = AllEnumValuesForArray(kFilterStrings);
        return kAllFilterValues;
    }
    case VbamOption::Type::kInterframe: {
        static const wxString kAllInterframeValues = AllEnumValuesForArray(kInterframeStrings);
        return kAllInterframeValues;
    }
    case VbamOption::Type::kRenderMethod: {
        static const wxString kAllRenderValues = AllEnumValuesForArray(kRenderMethodStrings);
        return kAllRenderValues;
    }
    case VbamOption::Type::kAudioApi: {
        static const wxString kAllAudioApiValues = AllEnumValuesForArray(kAudioApiStrings);
        return kAllAudioApiValues;
    }
    case VbamOption::Type::kSoundQuality: {
        static const wxString kAllSoundQualityValues = AllEnumValuesForArray(kSoundQualityStrings);
        return kAllSoundQualityValues;
    }

    // We don't use default here to explicitly trigger a compiler warning when
    // adding a new value.
    case VbamOption::Type::kNone:
    case VbamOption::Type::kBool:
    case VbamOption::Type::kDouble:
    case VbamOption::Type::kInt:
    case VbamOption::Type::kUnsigned:
    case VbamOption::Type::kString:
    case VbamOption::Type::kGbPalette:
        assert(false);
        return wxEmptyString;
    }
    assert(false);
    return wxEmptyString;
}

int MaxForType(VbamOption::Type type) {
    switch (type) {
    case VbamOption::Type::kFilter:
        return kNbFilterFunctions;
    case VbamOption::Type::kInterframe:
        return kNbInterframes;
    case VbamOption::Type::kRenderMethod:
        return kNbRenderMethods;
    case VbamOption::Type::kAudioApi:
        return kNbAudioApis;
    case VbamOption::Type::kSoundQuality:
        return kNbSoundQualities;

    // We don't use default here to explicitly trigger a compiler warning when
    // adding a new value.
    case VbamOption::Type::kNone:
    case VbamOption::Type::kBool:
    case VbamOption::Type::kDouble:
    case VbamOption::Type::kInt:
    case VbamOption::Type::kUnsigned:
    case VbamOption::Type::kString:
    case VbamOption::Type::kGbPalette:
        assert(false);
        return 0;
    }
    assert(false);
    return 0;
}

}  // namespace internal
