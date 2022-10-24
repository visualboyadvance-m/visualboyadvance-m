#ifndef VBAM_WX_CONFIG_OPTION_ID_H_
#define VBAM_WX_CONFIG_OPTION_ID_H_

namespace config {

enum class OptionID {
    // Display
    kDispBilinear = 0,
    kDispFilter,
    kDispFilterPlugin,
    kDispIFB,
    kDispKeepOnTop,
    kDispMaxThreads,
    kDispRenderMethod,
    kDispScale,
    kDispStretch,

    /// GB
    kGBBiosFile,
    kGBColorOption,
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

    /// General
    kGenAutoLoadLastState,
    kGenBatteryDir,
    kGenFreezeRecent,
    kGenRecordingDir,
    kGenRewindInterval,
    kGenScreenshotDir,
    kGenStateDir,
    kGenStatusBar,

    /// Joypad
    kJoy,
    kJoyAutofireThrottle,
    kJoyDefault,

    /// Keyboard
    kKeyboard,

    // Core
    kPrefAgbPrint,
    kPrefAutoFrameSkip,
    kPrefAutoPatch,
    kPrefAutoSaveLoadCheatList,
    kPrefBorderAutomatic,
    kPrefBorderOn,
    kPrefCaptureFormat,
    kPrefCheatsEnabled,

#ifdef MMX
    kPrefEnableMMX,
#endif
    kPrefDisableStatus,
    kPrefEmulatorType,
    kPrefFlashSize,
    kPrefFrameSkip,
    kPrefFsColorDepth,
    kPrefFsFrequency,
    kPrefFsHeight,
    kPrefFsWidth,
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
    kSoundQuality,
    kSoundVolume,

    // Do not add anything under here.
    Last,
};
static constexpr size_t kNbOptions = static_cast<size_t>(OptionID::Last);

}  // namespace config

#endif  // VBAM_WX_CONFIG_OPTION_ID_H_
