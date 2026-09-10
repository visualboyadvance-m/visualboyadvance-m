#include "qt/config/option.h"

// Helper implementation file to define and compile all of these huge constants
// separately. These should not be updated very often, so having these in a
// separate file improves incremental build time.

#include <algorithm>
#include <limits>
#include <map>

#include <QCoreApplication>

#include "core/base/check.h"
#include "core/base/system.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaSound.h"
#include "qt/log.h"
#include "qt/opts.h"

#define VBAM_OPTION_INTERNAL_INCLUDE
#include "qt/config/internal/option-internal.h"

#include "components/filters_agb/filters_agb.h"
#include "components/filters_cgb/filters_cgb.h"
#undef VBAM_OPTION_INTERNAL_INCLUDE

struct CoreOptions coreOptions;

namespace config {

namespace {

// clang-format off
// These MUST follow the same order as the definitions of the enum.
// Adding an option without adding to this array will result in a compiler
// error since kNbFilters is automatically updated.
static const std::array<QString, kNbFilters> kFilterStrings = {
    QStringLiteral("none"),
    QStringLiteral("super2xsai"),
    QStringLiteral("supereagle"),
    QStringLiteral("pixelate"),
    QStringLiteral("advmame"),
    QStringLiteral("bilinearplus"),
    QStringLiteral("scanlines"),
    QStringLiteral("tvmode"),
    QStringLiteral("hq4x"),
    QStringLiteral("lq2x"),
    QStringLiteral("simple4x"),
    QStringLiteral("xbrz2x"),
    QStringLiteral("xbrz6x"),
    QStringLiteral("xbrz9x"),
    QStringLiteral("scalefx3x"),
    QStringLiteral("scalefx9x"),
    QStringLiteral("plugin"),
};

// These MUST follow the same order as the definitions of the enum.
static const std::array<QString, kNbInterframes> kInterframeStrings = {
    QStringLiteral("none"),
    QStringLiteral("smart"),
    QStringLiteral("motionblur"),
};

// These MUST follow the same order as the definitions of the enum. The
// strings match the wx port's so an INI file can be shared where the render
// method exists in both.
static const std::array<QString, kNbRenderMethods> kRenderMethodStrings = {
    QStringLiteral("simple"),
    QStringLiteral("opengl"),
    QStringLiteral("sdl_video"),
#if defined(_WIN32)
#if !defined(NO_D3D12)
    QStringLiteral("direct3d12"),
#endif
#if !defined(NO_D3D)
    QStringLiteral("direct3d"),
#endif
#elif defined(__APPLE__)
    QStringLiteral("quartz2d"),
#ifndef NO_METAL
    QStringLiteral("metal"),
#endif
#endif
#ifndef NO_VULKAN
    QStringLiteral("vulkan"),
#endif
};

// These MUST follow the same order as the definitions of the enum.
static const std::array<QString, kNbColorCorrectionProfiles> kColorCorrectionProfileStrings = {
    QStringLiteral("srgb"),
    QStringLiteral("dci"),
    QStringLiteral("rec2020"),
};

// These MUST follow the same order as the definitions of the enum above.
static const std::array<QString, kNbAudioApis> kAudioApiStrings = {
#if defined(VBAM_ENABLE_OPENAL)
    QStringLiteral("openal"),
#endif
    QStringLiteral("sdl_audio"),
#if defined(_WIN32)
    QStringLiteral("directsound"),
#endif
#if defined(VBAM_ENABLE_XAUDIO2)
    QStringLiteral("xaudio2"),
#endif
#if defined(VBAM_ENABLE_FAUDIO)
    QStringLiteral("faudio"),
#endif
#if defined(__APPLE__)
    QStringLiteral("coreaudio"),
#endif
#if defined(VBAM_ENABLE_AAUDIO)
    QStringLiteral("aaudio"),
#endif
    QStringLiteral("null"),
};

// These MUST follow the same order as the definitions of the enum above.
static const std::array<QString, kNbSoundRate> kAudioRateStrings = {
    QStringLiteral("48"),
    QStringLiteral("44"),
    QStringLiteral("22"),
    QStringLiteral("11"),
};
// clang-format on

// Builds the "all enum values" string for a given array defined above.
template <std::size_t SIZE>
QString AllEnumValuesForArray(const std::array<QString, SIZE>& input) {
    QString all_options;
    std::for_each(input.cbegin(), input.cend(), [&all_options](const auto& elt) {
        all_options.append(elt);
        all_options.append('|');
    });
    // Remove last value
    all_options.chop(1);
    return all_options;
}

QString InvalidEnumWarning(const QString& input, const QString& config_name, Option::Type type) {
    return QCoreApplication::translate("vbam",
                                       "Invalid value %1 for option %2; valid values are %3")
        .arg(input)
        .arg(config_name)
        .arg(internal::AllEnumValuesForType(type));
}

}  // namespace

// static
std::array<Option, kNbOptions>& Option::All() {
    struct OwnedOptions {
        /// Display
        bool bilinear = false;
        bool sdl_pixel_art = false;
        // First launch runs a runtime probe (GameArea::StepFilterProbe) that
        // measures real frame rate once a ROM loads and picks the best filter,
        // persisting it; this stored default is only the pre-probe placeholder.
        Filter filter = Filter::kXbrz2x;
        QString filter_plugin;
        QString plugin_dir;
        Interframe interframe = Interframe::kNone;
        bool keep_on_top = false;
        int32_t max_threads = 0;

#if !defined(NO_OGL)
        RenderMethod render_method = RenderMethod::kOpenGL;
#else
        RenderMethod render_method = RenderMethod::kSimple;
#endif

        ColorCorrectionProfile color_correction_profile = ColorCorrectionProfile::kSRGB;
        bool color_correction_auto = true;

        // HDR / deep color are kept for INI compatibility with the wx port; the
        // Qt renderers present SDR only, so these are inert here.
        bool hdr = true;
        uint32_t hdr_reference_white = 220;
        uint32_t hdr_peak_brightness = 10000;
        uint32_t hdr_highlight_knee = 50;
        uint32_t hdr_shadow_contrast = 170;
        bool deep_color = true;

        double video_scale = 3;
        bool retain_aspect = true;

        /// GB
        QString gb_bios;
        bool colorizer_hack = false;
        bool gb_lcd_filter = true;
        // Index into GbcFilterVariant.
        uint32_t gb_lcd_filter_variant = 0;
        QString gbc_bios;
        bool print_auto_page = true;
        bool print_screen_cap = false;
        QString gb_rom_dir;
        QString gbc_rom_dir;
        uint32_t gb_lighten = 0;

        /// GBA
        bool gba_lcd_filter = true;
        // Index into GbaFilterVariant.
        uint32_t gba_lcd_filter_variant = 0;
#ifndef NO_LINK
        bool link_auto = false;
        bool link_hacks = true;
        bool link_proto = false;
#endif
        QString gba_rom_dir;
        uint32_t gba_darken = 37;

        /// Core
        bool agb_print = false;
        bool auto_frame_skip = false;
        bool auto_patch = true;
        bool autoload_cheats = false;
        uint32_t capture_format = 0;
        bool disable_status_messages = false;
        uint32_t flash_size = 0;
        int32_t frame_skip = 0;
        bool gdb_break_on_load = false;
        bool pause_when_inactive = false;
        uint32_t show_speed = 0;
        bool use_bios_file_gb = false;
        bool use_bios_file_gba = false;
        bool use_bios_file_gbc = false;
        bool vsync = false;

        /// General
        bool autoload_state = false;
        QString battery_dir;
        bool recent_freeze = false;
        QString recording_dir;
        QString screenshot_dir;
        QString state_dir;
        bool statusbar = true;
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
#if defined(VBAM_ENABLE_AAUDIO)
        AudioApi audio_api = AudioApi::kAAudio;
#elif defined(__APPLE__) && !defined(NO_COREAUDIO_DEFAULT)
        AudioApi audio_api = AudioApi::kCoreAudio;
#elif defined(_WIN32) && !defined(NO_DIRECTAUDIO_DEFAULT)
        AudioApi audio_api = AudioApi::kDirectSound;
#elif defined(VBAM_ENABLE_FAUDIO) && !defined(NO_FAUDIO_DEFAULT)
        AudioApi audio_api = AudioApi::kFAudio;
#elif defined(VBAM_ENABLE_XAUDIO2) && !defined(NO_XAUDIO2_DEFAULT)
        AudioApi audio_api = AudioApi::kXAudio2;
#elif defined(VBAM_ENABLE_OPENAL) && !defined(NO_OPENAL_DEFAULT)
        AudioApi audio_api = AudioApi::kOpenAL;
#else
        AudioApi audio_api = AudioApi::kSDL;
#endif
        QString audio_dev;
        // Buffer count is latency: each one is a frame of audio, so this is
        // 50ms at 3.
        int32_t audio_buffers = 3;
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
        QString sdlrenderer = QStringLiteral("default");
        // 0 is "system default" (the wx port's wxLANGUAGE_DEFAULT); other
        // values are wxLanguage codes kept for INI compatibility.
        int locale = 0;
        bool exttrans = false;
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
        Option(OptionID::kDispSDLPixelArt, &g_owned_opts.sdl_pixel_art),
        Option(OptionID::kDispFilter, &g_owned_opts.filter),
        Option(OptionID::kDispFilterPlugin, &g_owned_opts.filter_plugin),
        Option(OptionID::kDispPluginDir, &g_owned_opts.plugin_dir),
        Option(OptionID::kDispIFB, &g_owned_opts.interframe),
        Option(OptionID::kBitDepth, &g_owned_opts.bitdepth, 0, 3),
        Option(OptionID::kDispKeepOnTop, &g_owned_opts.keep_on_top),
        Option(OptionID::kDispMaxThreads, &g_owned_opts.max_threads, 0, 256),
        Option(OptionID::kDispRenderMethod, &g_owned_opts.render_method),
        Option(OptionID::kDispScale, &g_owned_opts.video_scale, 1, 6),
        Option(OptionID::kDispStretch, &g_owned_opts.retain_aspect),
        Option(OptionID::kSDLRenderer, &g_owned_opts.sdlrenderer),
        Option(OptionID::kDispColorCorrectionProfile, &g_owned_opts.color_correction_profile),
        Option(OptionID::kDispColorCorrectionAuto, &g_owned_opts.color_correction_auto),
        Option(OptionID::kDispHDR, &g_owned_opts.hdr),
        Option(OptionID::kDispHDRReferenceWhite, &g_owned_opts.hdr_reference_white, 80, 400),
        Option(OptionID::kDispHDRPeakBrightness, &g_owned_opts.hdr_peak_brightness, 100, 10000),
        Option(OptionID::kDispHDRHighlightKnee, &g_owned_opts.hdr_highlight_knee, 0, 100),
        Option(OptionID::kDispHDRShadowContrast, &g_owned_opts.hdr_shadow_contrast, 100, 300),
        Option(OptionID::kDispDeepColor, &g_owned_opts.deep_color),

        /// GB
        Option(OptionID::kGBBiosFile, &g_owned_opts.gb_bios),
        Option(OptionID::kGBColorizerHack, &g_owned_opts.colorizer_hack),
        Option(OptionID::kGBLCDFilter, &g_owned_opts.gb_lcd_filter),
        Option(OptionID::kGBLCDFilterVariant, &g_owned_opts.gb_lcd_filter_variant, 0,
               kGbcFilterVariantCount - 1),
        Option(OptionID::kGBGBCBiosFile, &g_owned_opts.gbc_bios),
        Option(OptionID::kGBPalette0, systemGbPalette),
        Option(OptionID::kGBPalette1, systemGbPalette + 8),
        Option(OptionID::kGBPalette2, systemGbPalette + 16),
        Option(OptionID::kGBPrintAutoPage, &g_owned_opts.print_auto_page),
        Option(OptionID::kGBPrintScreenCap, &g_owned_opts.print_screen_cap),
        Option(OptionID::kGBROMDir, &g_owned_opts.gb_rom_dir),
        Option(OptionID::kGBGBCROMDir, &g_owned_opts.gbc_rom_dir),
        Option(OptionID::kGBLighten, &g_owned_opts.gb_lighten, 0, 100),

        /// GBA
        Option(OptionID::kGBABiosFile, &gopts.gba_bios),
        Option(OptionID::kGBALCDFilter, &g_owned_opts.gba_lcd_filter),
        Option(OptionID::kGBALCDFilterVariant, &g_owned_opts.gba_lcd_filter_variant, 0,
               kGbaFilterVariantCount - 1),
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
        Option(OptionID::kGBADarken, &g_owned_opts.gba_darken, 0, 100),

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
        Option(OptionID::kPrefSkipBios, &coreOptions.skipBios),
        Option(OptionID::kPrefSkipSaveGameCheats, &coreOptions.skipSaveGameCheats, 0, 1),
        Option(OptionID::kPrefSkipSaveGameBattery, &coreOptions.skipSaveGameBattery, 0, 1),
        Option(OptionID::kPrefThrottle, &coreOptions.throttle, 0, kMaxThrottlePercent),
        Option(OptionID::kPrefSpeedupThrottle, &coreOptions.speedup_throttle, 0, kMaxThrottlePercent),
        Option(OptionID::kPrefSpeedupFrameSkip, &coreOptions.speedup_frame_skip, 0, 60),
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
        Option(OptionID::kUIShowOnScreenController, &gopts.show_onscreen_controller),
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
        Option(OptionID::kLocale, &g_owned_opts.locale, 0, 911),
        Option(OptionID::kExternalTranslations, &g_owned_opts.exttrans)
    };
    // clang-format on
    return g_all_opts;
}

namespace internal {

// Helper strings are translated lazily (see Option::UxHelper); QT_TRANSLATE_NOOP
// marks them for lupdate under the "vbam" context.
#define H(s) QT_TRANSLATE_NOOP("vbam", s)

// These MUST follow the same order as the definitions in OptionID.
// Adding an option without adding to this array will result in a compiler
// error since kNbOptions is automatically updated.
const std::array<OptionData, kNbOptions + 1> kAllOptionsData = {
    /// Display
    OptionData{"Display/Bilinear", "Bilinear", H("Use bilinear filter with 3d renderer")},
    OptionData{"Display/SDLPixelArt", "SDLPixelArt", H("Use the SDL pixel art filter with an SDL renderer")},
    OptionData{"Display/Filter", "", H("Full-screen filter to apply")},
    OptionData{"Display/FilterPlugin", "", H("Filter plugin library")},
    OptionData{"Display/PluginDir", "", H("Directory containing RPI filter plugins")},
    OptionData{"Display/IFB", "", H("Interframe blending function")},
    OptionData{"Display/BitDepth", "BitDepth", H("Bit depth")},
    OptionData{"Display/KeepOnTop", "KeepOnTop", H("Keep window on top")},
    OptionData{"Display/MaxThreads", "Multithread",
               H("Maximum number of threads to run filters in")},
    OptionData{"Display/RenderMethod", "",
               H("Render method; if unsupported, simple method will be used")},
    OptionData{"Display/Scale", "", H("Default scale factor")},
    OptionData{"Display/Stretch", "RetainAspect", H("Retain aspect ratio when resizing")},
    OptionData{"Display/SDLRenderer", "", H("SDL renderer")},
    OptionData{"Display/ColorCorrectionProfile", "", H("Color correction profile")},
    OptionData{"Display/ColorCorrectionAuto", "",
               H("Auto-select color correction profile (sRGB for SDR, Rec2020 for HDR)")},
    OptionData{"Display/HDR", "", H("Output HDR on supported displays and renderers")},
    OptionData{"Display/HDRReferenceWhite", "",
               H("HDR diffuse (paper) white luminance, in nits")},
    OptionData{"Display/HDRPeakBrightness", "",
               H("HDR peak/highlight luminance, in nits")},
    OptionData{"Display/HDRHighlightKnee", "",
               H("Input level percentage above which HDR highlights ramp to peak")},
    OptionData{"Display/HDRShadowContrast", "",
               H("HDR shadow contrast as a percentage gamma (100 = neutral; "
                 "higher deepens blacks)")},
    OptionData{"Display/DeepColor", "",
               H("Use a 10-bit deep color SDR visual for OpenGL (X11; reduces banding)")},

    /// GB
    OptionData{"GB/BiosFile", "", H("BIOS file to use for Game Boy, if enabled")},
    OptionData{"GB/ColorizerHack", "ColorizerHack", H("Enable DX Colorization Hacks")},
    OptionData{"GB/LCDFilter", "GBLcdFilter", H("Apply LCD filter, if enabled")},
    OptionData{"GB/LCDFilterVariant", "",
               H("LCD panel the GBC color correction emulates "
                 "(0 = GBC, 1 = NSO)")},
    OptionData{"GB/GBCBiosFile", "", H("BIOS file to use for Game Boy Color, if enabled")},
    OptionData{"GB/Palette0", "",
               H("The default palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555).")},
    OptionData{"GB/Palette1", "",
               H("The first user palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555).")},
    OptionData{"GB/Palette2", "",
               H("The second user palette, as 8 comma-separated 4-digit hex "
                 "integers (rgb555).")},
    OptionData{"GB/PrintAutoPage", "PrintGather",
               H("Automatically gather a full page before printing")},
    OptionData{"GB/PrintScreenCap", "PrintSnap",
               H("Automatically save printouts as screen captures with -print "
                 "suffix")},
    OptionData{"GB/ROMDir", "", H("Directory to look for ROM files")},
    OptionData{"GB/GBCROMDir", "", H("Directory to look for Game Boy Color ROM files")},
    OptionData{"GB/GBCLighten", "", H("Color lightness factor")},

    /// GBA
    OptionData{"GBA/BiosFile", "", H("BIOS file to use, if enabled")},
    OptionData{"GBA/LCDFilter", "GBALcdFilter", H("Apply LCD filter, if enabled")},
    OptionData{"GBA/LCDFilterVariant", "",
               H("LCD panel the GBA color correction emulates (0 = GBA, "
                 "1 = GBASP Backlit, 2 = Micro, 3 = DS, 4 = DS-Lite, 5 = NSO)")},
#ifndef NO_LINK
    OptionData{"GBA/LinkAuto", "LinkAuto", H("Enable link at boot")},
    OptionData{"GBA/LinkFast", "SpeedOn", H("Enable faster network protocol by default")},
    OptionData{"GBA/LinkHost", "", H("Default network link client host")},
    OptionData{"GBA/ServerIP", "", H("Default network link server IP to bind")},
    OptionData{"GBA/LinkPort", "", H("Default network link port (server and client)")},
    OptionData{"GBA/LinkProto", "LinkProto", H("Default network protocol")},
    OptionData{"GBA/LinkTimeout", "LinkTimeout", H("Link timeout (ms)")},
    OptionData{"GBA/LinkType", "LinkType", H("Link cable type")},
#endif
    OptionData{"GBA/ROMDir", "", H("Directory to look for ROM files")},
    OptionData{"GBA/GBADarken", "", H("Color darkness factor")},

    /// General
    OptionData{"General/AutoLoadLastState", "", H("Automatically load last saved state")},
    OptionData{"General/BatteryDir", "",
               H("Directory to store game save files (relative paths are "
                 "relative to the executable; blank is ROM dir)")},
    OptionData{"General/FreezeRecent", "", H("Freeze recent load list")},
    OptionData{"General/RecordingDir", "",
               H("Directory to store A / V and game recordings (relative paths "
                 "paths are relative to the executable; blank is ROM dir)")},
    OptionData{"General/RewindInterval", "",
               H("Number of seconds between rewind snapshots (0 to disable)")},
    OptionData{"General/ScreenshotDir", "",
               H("Directory to store screenshots (relative paths are relative "
                 "to the executable; blank is ROM dir)")},
    OptionData{"General/StateDir", "",
               H("Directory to store saved state files (relative paths are "
                 "relative to the executable; blank is ROM dir)")},
    OptionData{"General/StatusBar", "StatusBar", H("Enable status bar")},
    OptionData{"General/IniVersion", "", H("INI file version (DO NOT MODIFY)")},

    /// Joypad
    OptionData{"Joypad/*/*", "",
               H("The parameter Joypad/<n>/<button> contains a comma-separated "
                 "list of key names which map to joypad #<n> button <button>. "
                 "Button is one of Up, Down, Left, Right, A, B, L, R, Select, "
                 "Start, MotionUp, MotionDown, MotionLeft, MotionRight, AutoA, "
                 "AutoB, Speed, Capture, GS")},
    OptionData{"Joypad/AutofireThrottle", "", H("The autofire toggle period, in frames (1/60 s)")},
    OptionData{"Joypad/Default", "", H("The number of the stick to use in single-player mode")},
    OptionData{"Joypad/SDLGameControllerMode", "SDLGameControllerMode",
               H("Whether to enable SDL GameController mode")},

    /// Keyboard
    OptionData{"Keyboard/*", "",
               H("The parameter Keyboard/<cmd> contains a comma-separated list "
                 "of key names (e.g. Alt-Shift-F1).  When the named key is "
                 "pressed, the command <cmd> is executed.")},

    /// Core
    OptionData{"preferences/agbPrint", "AGBPrinter", H("Enable AGB debug print")},
    OptionData{"preferences/autoFrameSkip", "FrameSkipAuto", H("Auto skip frames")},
    OptionData{"preferences/autoPatch", "ApplyPatches",
               H("Apply IPS / UPS / IPF patches if found")},
    OptionData{"preferences/autoSaveLoadCheatList", "",
               H("Automatically save and load cheat list")},
    OptionData{"preferences/borderAutomatic", "",
               H("Automatically enable border for Super Game Boy games")},
    OptionData{"preferences/borderOn", "", H("Always enable border")},
    OptionData{"preferences/captureFormat", "", H("Screen capture file format")},
    OptionData{"preferences/cheatsEnabled", "", H("Enable cheats")},
    OptionData{"preferences/disableStatus", "NoStatusMsg", H("Disable on-screen status messages")},
    OptionData{"preferences/emulatorType", "", H("Type of system to emulate")},
    OptionData{"preferences/flashSize", "", H("Flash size 0 = 64 KB 1 = 128 KB")},
    OptionData{"preferences/frameSkip", "FrameSkip",
               H("Skip frames. Values are 0-9 or -1 to skip automatically "
                 "based on time.")},
    OptionData{"preferences/gbPaletteOption", "", H("The palette to use")},
    OptionData{"preferences/gbPrinter", "Printer", H("Enable printer emulation")},
    OptionData{"preferences/gdbBreakOnLoad", "DebugGDBBreakOnLoad",
               H("Break into GDB after loading the game.")},
    OptionData{"preferences/gdbPort", "DebugGDBPort", H("Port to connect GDB to")},
#ifndef NO_LINK
    OptionData{"preferences/LinkNumPlayers", "", H("Number of players in network")},
#endif
    OptionData{"preferences/maxScale", "", H("Maximum scale factor (0 = no limit)")},
    OptionData{"preferences/pauseWhenInactive", "PauseWhenInactive",
               H("Pause game when main window loses focus")},
    OptionData{"preferences/rtcEnabled", "RTC",
               H("Enable RTC (vba-over.ini override is rtcEnabled")},
    OptionData{"preferences/saveType", "", H("Native save (\"battery\") hardware type")},
    OptionData{"preferences/showSpeed", "", H("Show speed indicator")},
    OptionData{"preferences/skipBios", "SkipIntro", H("Skip BIOS initialization")},
    OptionData{"preferences/skipSaveGameCheats", "",
               H("Do not overwrite cheat list when loading state")},
    OptionData{"preferences/skipSaveGameBattery", "",
               H("Do not overwrite native (battery) save when loading state")},
    OptionData{"preferences/throttle", "",
               H("Throttle game speed, even when accelerated (0-450 %, 0 = no "
                 "throttle)")},
    OptionData{"preferences/speedupThrottle", "",
               H("Set throttle for speedup key (0-3000 %, 0 = no throttle)")},
    OptionData{"preferences/speedupFrameSkip", "",
               H("Number of frames to skip with speedup (instead of speedup "
                 "throttle)")},
    OptionData{"preferences/speedupThrottleFrameSkip", "",
               H("Use frame skip for speedup throttle")},
    OptionData{"preferences/speedupMute", "", H("Mute sound during speedup")},
    OptionData{"preferences/useBiosGB", "BootRomGB", H("Use the specified BIOS file for Game Boy")},
    OptionData{"preferences/useBiosGBA", "BootRomEn", H("Use the specified BIOS file")},
    OptionData{"preferences/useBiosGBC", "BootRomGBC",
               H("Use the specified BIOS file for Game Boy Color")},
    OptionData{"preferences/vsync", "VSync", H("Wait for vertical sync")},

    /// Geometry
    OptionData{"geometry/fullScreen", "Fullscreen", H("Enter fullscreen mode at startup")},
    OptionData{"geometry/isMaximized", "Maximized", H("Window maximized")},
    OptionData{"geometry/windowHeight", "Height", H("Window height at startup")},
    OptionData{"geometry/windowWidth", "Width", H("Window width at startup")},
    OptionData{"geometry/windowX", "X", H("Window axis X position at startup")},
    OptionData{"geometry/windowY", "Y", H("Window axis Y position at startup")},

    /// UI
    OptionData{"ui/allowKeyboardBackgroundInput", "AllowKeyboardBackgroundInput",
               H("Capture key events while on background")},
    OptionData{"ui/allowJoystickBackgroundInput", "AllowJoystickBackgroundInput",
               H("Capture joy events while on background")},
    OptionData{"ui/hideMenuBar", "HideMenuBar", H("Hide menu bar when mouse is inactive")},
    OptionData{"ui/onScreenController", "OnScreenController",
               H("Show the on-screen touch controller")},
    OptionData{"ui/suspendScreenSaver", "SuspendScreenSaver",
               H("Suspend screensaver when game is running")},

    /// Sound
    OptionData{"Sound/AudioAPI", "", H("Sound API; if unsupported, default API will be used")},
    OptionData{"Sound/AudioDevice", "", H("Device ID of chosen audio device for chosen driver")},
    OptionData{"Sound/Buffers", "", H("Number of sound buffers")},
    OptionData{"Sound/Enable", "", H("Bit mask of sound channels to enable")},
    OptionData{"Sound/GBAFiltering", "", H("Game Boy Advance sound filtering (%)")},
    OptionData{"Sound/GBAInterpolation", "GBASoundInterpolation",
               H("Game Boy Advance sound interpolation")},
    OptionData{"Sound/GBDeclicking", "GBDeclicking", H("Game Boy sound declicking")},
    OptionData{"Sound/GBEcho", "", H("Game Boy echo effect (%)")},
    OptionData{"Sound/GBEnableEffects", "GBEnhanceSound", H("Enable Game Boy sound effects")},
    OptionData{"Sound/GBStereo", "", H("Game Boy stereo effect (%)")},
    OptionData{"Sound/GBSurround", "GBSurround", H("Game Boy surround sound effect (%)")},
    OptionData{"Sound/Quality", "", H("Sound sample rate (kHz)")},
    OptionData{"Sound/DSoundHWAccel", "DSoundHWAccel", H("Use DirectSound hardware acceleration")},
    OptionData{"Sound/Upmix", "Upmix", H("Upmix stereo to surround")},
    OptionData{"Sound/Volume", "", H("Sound volume (%)")},
    OptionData{"Language/Locale", "", H("Language")},
    OptionData{"Language/ExternalTranslations", "", H("External translations")},

    // Last. This should never be used, it actually maps to OptionID::kLast.
    // This is to prevent a memory access violation error in case something
    // attempts to instantiate a OptionID::kLast. It will trigger a check
    // in the Option constructor, but that is after the constructor has
    // accessed this entry.
    OptionData{"", "", ""},
};

#undef H

nonstd::optional<OptionID> StringToOptionId(const QString& input) {
    static const std::map<QString, OptionID> kStringToOptionId([] {
        std::map<QString, OptionID> string_to_option_id;
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

QString FilterToString(const Filter& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbFilters);
    return kFilterStrings[size_value];
}

QString InterframeToString(const Interframe& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbInterframes);
    return kInterframeStrings[size_value];
}

QString RenderMethodToString(const RenderMethod& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbRenderMethods);
    return kRenderMethodStrings[size_value];
}

QString ColorCorrectionProfileToString(const ColorCorrectionProfile& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbColorCorrectionProfiles);
    return kColorCorrectionProfileStrings[size_value];
}

QString AudioApiToString(const AudioApi& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbAudioApis);
    return kAudioApiStrings[size_value];
}

QString AudioRateToString(const AudioRate& value) {
    const size_t size_value = static_cast<size_t>(value);
    VBAM_CHECK(size_value < kNbSoundRate);
    return kAudioRateStrings[size_value];
}

namespace {

template <typename Enum, std::size_t SIZE>
const std::map<QString, Enum>& StringToEnumMap(const std::array<QString, SIZE>& strings) {
    static const std::map<QString, Enum> kMap([&strings] {
        std::map<QString, Enum> string_to_enum;
        for (size_t i = 0; i < SIZE; i++) {
            string_to_enum.emplace(strings[i], static_cast<Enum>(i));
        }
        VBAM_CHECK(string_to_enum.size() == SIZE);
        return string_to_enum;
    }());
    return kMap;
}

}  // namespace

Filter StringToFilter(const QString& config_name, const QString& input) {
    const auto& map = StringToEnumMap<Filter>(kFilterStrings);
    const auto iter = map.find(input);
    if (iter == map.end()) {
        vbam::LogWarning(InvalidEnumWarning(input, config_name, Option::Type::kFilter));
        return Filter::kNone;
    }
    return iter->second;
}

Interframe StringToInterframe(const QString& config_name, const QString& input) {
    const auto& map = StringToEnumMap<Interframe>(kInterframeStrings);
    const auto iter = map.find(input);
    if (iter == map.end()) {
        vbam::LogWarning(InvalidEnumWarning(input, config_name, Option::Type::kInterframe));
        return Interframe::kNone;
    }
    return iter->second;
}

RenderMethod StringToRenderMethod(const QString& config_name, const QString& input) {
    const auto& map = StringToEnumMap<RenderMethod>(kRenderMethodStrings);
    const auto iter = map.find(input);
    if (iter == map.end()) {
        // Render methods of the wx port that do not exist in this build (a
        // renderer compiled out, or one of another platform, or the Android
        // GLES / macOS Quartz2D paths) silently map to the best one available,
        // so a shared INI does not warn on every start.
        static const std::array<QString, 7> kForeignRenderMethods = {
            QStringLiteral("sdl_video"), QStringLiteral("gles"),      QStringLiteral("direct3d12"),
            QStringLiteral("direct3d"),  QStringLiteral("quartz2d"),  QStringLiteral("metal"),
            QStringLiteral("vulkan"),
        };
        if (std::find(kForeignRenderMethods.begin(), kForeignRenderMethods.end(), input) !=
            kForeignRenderMethods.end()) {
#if !defined(NO_OGL)
            return RenderMethod::kOpenGL;
#else
            return RenderMethod::kSimple;
#endif
        }
        vbam::LogWarning(InvalidEnumWarning(input, config_name, Option::Type::kRenderMethod));
        return RenderMethod::kSimple;
    }
    return iter->second;
}

ColorCorrectionProfile StringToColorCorrectionProfile(const QString& config_name,
                                                      const QString& input) {
    const auto& map = StringToEnumMap<ColorCorrectionProfile>(kColorCorrectionProfileStrings);
    const auto iter = map.find(input);
    if (iter == map.end()) {
        vbam::LogWarning(
            InvalidEnumWarning(input, config_name, Option::Type::kColorCorrectionProfile));
        return ColorCorrectionProfile::kSRGB;
    }
    return iter->second;
}

AudioApi StringToAudioApi(const QString& config_name, const QString& input) {
    const auto& map = StringToEnumMap<AudioApi>(kAudioApiStrings);
    const auto iter = map.find(input);
    if (iter == map.end()) {
        vbam::LogWarning(InvalidEnumWarning(input, config_name, Option::Type::kAudioApi));
        return AudioApi::kSDL;
    }
    return iter->second;
}

AudioRate StringToSoundQuality(const QString& config_name, const QString& input) {
    const auto& map = StringToEnumMap<AudioRate>(kAudioRateStrings);
    const auto iter = map.find(input);
    if (iter == map.end()) {
        vbam::LogWarning(InvalidEnumWarning(input, config_name, Option::Type::kAudioRate));
        return AudioRate::k44kHz;
    }
    return iter->second;
}

QString AllEnumValuesForType(Option::Type type) {
    switch (type) {
        case Option::Type::kFilter: {
            static const QString kAllFilterValues(AllEnumValuesForArray(kFilterStrings));
            return kAllFilterValues;
        }
        case Option::Type::kInterframe: {
            static const QString kAllInterframeValues(AllEnumValuesForArray(kInterframeStrings));
            return kAllInterframeValues;
        }
        case Option::Type::kRenderMethod: {
            static const QString kAllRenderValues(AllEnumValuesForArray(kRenderMethodStrings));
            return kAllRenderValues;
        }
        case Option::Type::kColorCorrectionProfile: {
            static const QString kAllColorCorrectionValues(
                AllEnumValuesForArray(kColorCorrectionProfileStrings));
            return kAllColorCorrectionValues;
        }
        case Option::Type::kAudioApi: {
            static const QString kAllAudioApiValues(AllEnumValuesForArray(kAudioApiStrings));
            return kAllAudioApiValues;
        }
        case Option::Type::kAudioRate: {
            static const QString kAllSoundQualityValues(AllEnumValuesForArray(kAudioRateStrings));
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
            VBAM_NOTREACHED_RETURN(QString());
    }
    VBAM_NOTREACHED_RETURN(QString());
}

size_t MaxForType(Option::Type type) {
    switch (type) {
        case Option::Type::kFilter:
            return kNbFilters;
        case Option::Type::kInterframe:
            return kNbInterframes;
        case Option::Type::kRenderMethod:
            return kNbRenderMethods;
        case Option::Type::kColorCorrectionProfile:
            return kNbColorCorrectionProfiles;
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
            VBAM_NOTREACHED_RETURN(0);
    }
    VBAM_NOTREACHED_RETURN(0);
}

}  // namespace internal
}  // namespace config
