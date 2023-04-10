#ifndef VBAM_WX_CONFIG_OPTION_PROXY_H_
#define VBAM_WX_CONFIG_OPTION_PROXY_H_

#include <array>
#include <type_traits>

#include "config/option-id.h"
#include "config/option.h"

namespace config {

static constexpr std::array<Option::Type, kNbOptions> kOptionsTypes = {
    /// Display
    /*kDispBilinear*/ Option::Type::kBool,
    /*kDispFilter*/ Option::Type::kFilter,
    /*kDispFilterPlugin*/ Option::Type::kString,
    /*kDispIFB*/ Option::Type::kInterframe,
    /*kDispKeepOnTop*/ Option::Type::kBool,
    /*kDispMaxThreads*/ Option::Type::kInt,
    /*kDispRenderMethod*/ Option::Type::kRenderMethod,
    /*kDispScale*/ Option::Type::kDouble,
    /*kDispStretch*/ Option::Type::kBool,

    /// GB
    /*kGBBiosFile*/ Option::Type::kString,
    /*kGBColorOption*/ Option::Type::kBool,
    /*kGBColorizerHack*/ Option::Type::kBool,
    /*kGBLCDFilter*/ Option::Type::kBool,
    /*kGBGBCBiosFile*/ Option::Type::kString,
    /*kGBPalette0*/ Option::Type::kGbPalette,
    /*kGBPalette1*/ Option::Type::kGbPalette,
    /*kGBPalette2*/ Option::Type::kGbPalette,
    /*kGBPrintAutoPage*/ Option::Type::kBool,
    /*kGBPrintScreenCap*/ Option::Type::kBool,
    /*kGBROMDir*/ Option::Type::kString,
    /*kGBGBCROMDir*/ Option::Type::kString,

    /// GBA
    /*kGBABiosFile*/ Option::Type::kString,
    /*kGBALCDFilter*/ Option::Type::kBool,
#ifndef NO_LINK
    /*kGBALinkAuto*/ Option::Type::kBool,
    /*kGBALinkFast*/ Option::Type::kBool,
    /*kGBALinkHost*/ Option::Type::kString,
    /*kGBAServerIP*/ Option::Type::kString,
    /*kGBALinkPort*/ Option::Type::kInt,
    /*kGBAServerPort*/ Option::Type::kInt,
    /*kGBALinkProto*/ Option::Type::kBool,
    /*kGBALinkTimeout*/ Option::Type::kInt,
    /*kGBALinkType*/ Option::Type::kInt,
#endif
    /*kGBAROMDir*/ Option::Type::kString,

    /// General
    /*kGenAutoLoadLastState*/ Option::Type::kBool,
    /*kGenBatteryDir*/ Option::Type::kString,
    /*kGenFreezeRecent*/ Option::Type::kBool,
    /*kGenRecordingDir*/ Option::Type::kString,
    /*kGenRewindInterval*/ Option::Type::kInt,
    /*kGenScreenshotDir*/ Option::Type::kString,
    /*kGenStateDir*/ Option::Type::kString,
    /*kGenStatusBar*/ Option::Type::kBool,
    /*kGenIniVersion*/ Option::Type::kUnsigned,

    /// Joypad
    /*kJoy*/ Option::Type::kNone,
    /*kJoyAutofireThrottle*/ Option::Type::kInt,
    /*kJoyDefault*/ Option::Type::kInt,

    /// Keyboard
    /*kKeyboard*/ Option::Type::kNone,

    /// Core
    /*kPrefAgbPrint*/ Option::Type::kBool,
    /*kPrefAutoFrameSkip*/ Option::Type::kBool,
    /*kPrefAutoPatch*/ Option::Type::kBool,
    /*kPrefAutoSaveLoadCheatList*/ Option::Type::kBool,
    /*kPrefBorderAutomatic*/ Option::Type::kBool,
    /*kPrefBorderOn*/ Option::Type::kBool,
    /*kPrefCaptureFormat*/ Option::Type::kUnsigned,
    /*kPrefCheatsEnabled*/ Option::Type::kInt,
    /*kPrefDisableStatus*/ Option::Type::kBool,
    /*kPrefEmulatorType*/ Option::Type::kUnsigned,
    /*kPrefFlashSize*/ Option::Type::kUnsigned,
    /*kPrefFrameSkip*/ Option::Type::kInt,
    /*kPrefGBPaletteOption*/ Option::Type::kUnsigned,
    /*kPrefGBPrinter*/ Option::Type::kInt,
    /*kPrefGDBBreakOnLoad*/ Option::Type::kBool,
    /*kPrefGDBPort*/ Option::Type::kInt,
#ifndef NO_LINK
    /*kPrefLinkNumPlayers*/ Option::Type::kInt,
#endif
    /*kPrefMaxScale*/ Option::Type::kInt,
    /*kPrefPauseWhenInactive*/ Option::Type::kBool,
    /*kPrefRTCEnabled*/ Option::Type::kInt,
    /*kPrefSaveType*/ Option::Type::kInt,
    /*kPrefShowSpeed*/ Option::Type::kUnsigned,
    /*kPrefShowSpeedTransparent*/ Option::Type::kBool,
    /*kPrefSkipBios*/ Option::Type::kBool,
    /*kPrefSkipSaveGameCheats*/ Option::Type::kInt,
    /*kPrefSkipSaveGameBattery*/ Option::Type::kInt,
    /*kPrefThrottle*/ Option::Type::kUnsigned,
    /*kPrefSpeedupThrottle*/ Option::Type::kUnsigned,
    /*kPrefSpeedupFrameSkip*/ Option::Type::kUnsigned,
    /*kPrefSpeedupThrottleFrameSkip*/ Option::Type::kBool,
    /*kPrefUseBiosGB*/ Option::Type::kBool,
    /*kPrefUseBiosGBA*/ Option::Type::kBool,
    /*kPrefUseBiosGBC*/ Option::Type::kBool,
    /*kPrefVsync*/ Option::Type::kBool,

    /// Geometry
    /*kGeomFullScreen*/ Option::Type::kBool,
    /*kGeomIsMaximized*/ Option::Type::kBool,
    /*kGeomWindowHeight*/ Option::Type::kUnsigned,
    /*kGeomWindowWidth*/ Option::Type::kUnsigned,
    /*kGeomWindowX*/ Option::Type::kInt,
    /*kGeomWindowY*/ Option::Type::kInt,

    /// UI
    /*kUIAllowKeyboardBackgroundInput*/ Option::Type::kBool,
    /*kUIAllowJoystickBackgroundInput*/ Option::Type::kBool,
    /*kUIHideMenuBar*/ Option::Type::kBool,
    /*kUISuspendScreenSaver*/ Option::Type::kBool,

    /// Sound
    /*kSoundAudioAPI*/ Option::Type::kAudioApi,
    /*kSoundAudioDevice*/ Option::Type::kString,
    /*kSoundBuffers*/ Option::Type::kInt,
    /*kSoundEnable*/ Option::Type::kInt,
    /*kSoundGBAFiltering*/ Option::Type::kInt,
    /*kSoundGBAInterpolation*/ Option::Type::kBool,
    /*kSoundGBDeclicking*/ Option::Type::kBool,
    /*kSoundGBEcho*/ Option::Type::kInt,
    /*kSoundGBEnableEffects*/ Option::Type::kBool,
    /*kSoundGBStereo*/ Option::Type::kInt,
    /*kSoundGBSurround*/ Option::Type::kBool,
    /*kSoundQuality*/ Option::Type::kSoundQuality,
    /*kSoundVolume*/ Option::Type::kInt,
};

// Less verbose accessor for a specific OptionID with compile-time type checks.
//
// Sample usage:
//
// if (OPTION(kDispBilinear)) {
//     // Do something if bilinear filter is on.
// }
//
// // Set this Option to false.
// OPTION(kDispBilinear) = false;
#define OPTION(option_id) ::config::OptionProxy<::config::OptionID::option_id>()

template <OptionID ID, typename = void>
class OptionProxy {};

template <OptionID ID>
class OptionProxy<
    ID,
    typename std::enable_if<kOptionsTypes[static_cast<size_t>(ID)] ==
                            Option::Type::kBool>::type> {
public:
    OptionProxy() : option_(Option::ByID(ID)) {}
    ~OptionProxy() = default;

    bool Get() const { return option_->GetBool(); }
    bool Set(bool value) { return option_->SetBool(value); }

    bool operator=(bool value) { return Set(value); }
    operator bool() const { return Get(); }

private:
    Option* option_;
};

template <OptionID ID>
class OptionProxy<
    ID,
    typename std::enable_if<kOptionsTypes[static_cast<size_t>(ID)] ==
                            Option::Type::kDouble>::type> {
public:
    OptionProxy() : option_(Option::ByID(ID)) {}
    ~OptionProxy() = default;

    double Get() const { return option_->GetDouble(); }
    bool Set(double value) { return option_->SetDouble(value); }
    double Min() const { return option_->GetDoubleMin(); }
    double Max() const { return option_->GetDoubleMax(); }

    bool operator=(double value) { return Set(value); }
    operator double() const { return Get(); }

private:
    Option* option_;
};

template <OptionID ID>
class OptionProxy<
    ID,
    typename std::enable_if<kOptionsTypes[static_cast<size_t>(ID)] ==
                            Option::Type::kInt>::type> {
public:
    OptionProxy() : option_(Option::ByID(ID)) {}
    ~OptionProxy() = default;

    int32_t Get() const { return option_->GetInt(); }
    bool Set(int32_t value) { return option_->SetInt(value); }
    int32_t Min() const { return option_->GetIntMin(); }
    int32_t Max() const { return option_->GetIntMax(); }

    bool operator=(int32_t value) { return Set(value); }
    operator int32_t() const { return Get(); }

private:
    Option* option_;
};

template <OptionID ID>
class OptionProxy<
    ID,
    typename std::enable_if<kOptionsTypes[static_cast<size_t>(ID)] ==
                            Option::Type::kUnsigned>::type> {
public:
    OptionProxy() : option_(Option::ByID(ID)) {}
    ~OptionProxy() = default;

    uint32_t Get() const { return option_->GetUnsigned(); }
    bool Set(uint32_t value) { return option_->SetUnsigned(value); }
    uint32_t Min() const { return option_->GetUnsignedMin(); }
    uint32_t Max() const { return option_->GetUnsignedMax(); }

    bool operator=(int32_t value) { return Set(value); }
    operator int32_t() const { return Get(); }

private:
    Option* option_;
};

template <OptionID ID>
class OptionProxy<
    ID,
    typename std::enable_if<kOptionsTypes[static_cast<size_t>(ID)] ==
                            Option::Type::kString>::type> {
public:
    OptionProxy() : option_(Option::ByID(ID)) {}
    ~OptionProxy() = default;

    const wxString& Get() const { return option_->GetString(); }
    bool Set(const wxString& value) { return option_->SetString(value); }

    bool operator=(wxString value) { return Set(value); }
    operator const wxString&() const { return Get(); }

private:
    Option* option_;
};

template <OptionID ID>
class OptionProxy<
    ID,
    typename std::enable_if<kOptionsTypes[static_cast<size_t>(ID)] ==
                            Option::Type::kFilter>::type> {
public:
    OptionProxy() : option_(Option::ByID(ID)) {}
    ~OptionProxy() = default;

    Filter Get() const { return option_->GetFilter(); }
    bool Set(Filter value) { return option_->SetFilter(value); }
    void Next() { option_->NextFilter(); }

    bool operator=(Filter value) { return Set(value); }
    operator Filter() const { return Get(); }

private:
    Option* option_;
};

template <OptionID ID>
class OptionProxy<
    ID,
    typename std::enable_if<kOptionsTypes[static_cast<size_t>(ID)] ==
                            Option::Type::kInterframe>::type> {
public:
    OptionProxy() : option_(Option::ByID(ID)) {}
    ~OptionProxy() = default;

    Interframe Get() const { return option_->GetInterframe(); }
    bool Set(Interframe value) { return option_->SetInterframe(value); }
    void Next() { option_->NextInterframe(); }

    bool operator=(Interframe value) { return Set(value); }
    operator Interframe() const { return Get(); }

private:
    Option* option_;
};

template <OptionID ID>
class OptionProxy<
    ID,
    typename std::enable_if<kOptionsTypes[static_cast<size_t>(ID)] ==
                            Option::Type::kRenderMethod>::type> {
public:
    OptionProxy() : option_(Option::ByID(ID)) {}
    ~OptionProxy() = default;

    RenderMethod Get() const { return option_->GetRenderMethod(); }
    bool Set(RenderMethod value) { return option_->SetRenderMethod(value); }

    bool operator=(RenderMethod value) { return Set(value); }
    operator RenderMethod() const { return Get(); }

private:
    Option* option_;
};

}  // namespace config

#endif  // VBAM_WX_CONFIG_OPTION_PROXY_H_
