#ifndef VBAM_WX_CONFIG_OPTION_ID_H_
#define VBAM_WX_CONFIG_OPTION_ID_H_

#include <cstddef>

namespace config {

enum class OptionID {
    /// Display
    kDispBilinear = 0,
    kDispSDLPixelArt,
    kDispFilter,
    kDispFilterPlugin,
    kDispIFB,
    kBitDepth,
    kDispKeepOnTop,
    kDispMaxThreads,
    kDispRenderMethod,
    kDispScale,
    kDispStretch,
    kSDLRenderer,
    kDispColorCorrectionProfile,

    /// GB
    kGBBiosFile,
    kGBColorizerHack,
    kGBLCDFilter,
    kGBGBCBiosFile,
    kGBPalette0,
    kGBPalette1,
    kGBPalette2,
    kGBPrintAutoPage,
    kGBPrintScreenCap,
    kGBROMDir,
    kGBGBCROMDir,
    kGBLighten,

    /// GBA
    kGBABiosFile,
    kGBALCDFilter,
#ifndef NO_LINK
    kGBALinkAuto,
    kGBALinkFast,
    kGBALinkHost,
    kGBAServerIP,
    kGBALinkPort,
    kGBALinkProto,
    kGBALinkTimeout,
    kGBALinkType,
#endif
    kGBAROMDir,
    kGBADarken,

    /// General
    kGenAutoLoadLastState,
    kGenBatteryDir,
    kGenFreezeRecent,
    kGenRecordingDir,
    kGenRewindInterval,
    kGenScreenshotDir,
    kGenStateDir,
    kGenStatusBar,
    kGenIniVersion,

    /// Joypad
    kJoy,
    kJoyAutofireThrottle,
    kJoyDefault,
    kSDLGameControllerMode,

    /// Keyboard
    kKeyboard,

    /// Core
    kPrefAgbPrint,
    kPrefAutoFrameSkip,
    kPrefAutoPatch,
    kPrefAutoSaveLoadCheatList,
    kPrefBorderAutomatic,
    kPrefBorderOn,
    kPrefCaptureFormat,
    kPrefCheatsEnabled,
    kPrefDisableStatus,
    kPrefEmulatorType,
    kPrefFlashSize,
    kPrefFrameSkip,
    kPrefGBPaletteOption,
    kPrefGBPrinter,
    kPrefGDBBreakOnLoad,
    kPrefGDBPort,
#ifndef NO_LINK
    kPrefLinkNumPlayers,
#endif
    kPrefMaxScale,
    kPrefPauseWhenInactive,
    kPrefRTCEnabled,
    kPrefSaveType,
    kPrefShowSpeed,
    kPrefShowSpeedTransparent,
    kPrefSkipBios,
    kPrefSkipSaveGameCheats,
    kPrefSkipSaveGameBattery,
    kPrefThrottle,
    kPrefSpeedupThrottle,
    kPrefSpeedupFrameSkip,
    kPrefSpeedupThrottleFrameSkip,
    kPrefSpeedupMute,
    kPrefUseBiosGB,
    kPrefUseBiosGBA,
    kPrefUseBiosGBC,
    kPrefVsync,

    /// Geometry
    kGeomFullScreen,
    kGeomIsMaximized,
    kGeomWindowHeight,
    kGeomWindowWidth,
    kGeomWindowX,
    kGeomWindowY,

    /// UI
    kUIAllowKeyboardBackgroundInput,
    kUIAllowJoystickBackgroundInput,
    kUIHideMenuBar,
    kUISuspendScreenSaver,

    /// Sound
    kSoundAudioAPI,
    kSoundAudioDevice,
    kSoundBuffers,
    kSoundEnable,
    kSoundGBAFiltering,
    kSoundGBAInterpolation,
    kSoundGBDeclicking,
    kSoundGBEcho,
    kSoundGBEnableEffects,
    kSoundGBStereo,
    kSoundGBSurround,
    kSoundAudioRate,
    kSoundDSoundHWAccel,
    kSoundUpmix,
    kSoundVolume,
    kLocale,
    kExternalTranslations,

    // Do not add anything under here.
    Last,
};
static constexpr size_t kNbOptions = static_cast<size_t>(OptionID::Last);

}  // namespace config

#endif  // VBAM_WX_CONFIG_OPTION_ID_H_
