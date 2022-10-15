#ifndef VBAM_WX_CONFIG_OPTIONS_H_
#define VBAM_WX_CONFIG_OPTIONS_H_

#include "nonstd/variant.hpp"

#include <array>

#include <wx/string.h>

namespace config {

enum class OptionID {
    // Display
    kDisplayBilinear = 0,
    kDisplayFilter,
    kDisplayFilterPlugin,
    kDisplayIFB,
    kDisplayKeepOnTop,
    kDisplayMaxThreads,
    kDisplayRenderMethod,
    kDisplayScale,
    kDisplayStretch,

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
    kGeneralAutoLoadLastState,
    kGeneralBatteryDir,
    kGeneralFreezeRecent,
    kGeneralRecordingDir,
    kGeneralRewindInterval,
    kGeneralScreenshotDir,
    kGeneralStateDir,
    kGeneralStatusBar,

    /// Joypad
    kJoypad,
    kJoypadAutofireThrottle,
    kJoypadDefault,

    /// Keyboard
    kKeyboard,

    // Core
    kpreferencesagbPrint,
    kpreferencesautoFrameSkip,
    kpreferencesautoPatch,
    kpreferencesautoSaveLoadCheatList,
    kpreferencesborderAutomatic,
    kpreferencesborderOn,
    kpreferencescaptureFormat,
    kpreferencescheatsEnabled,

#ifdef MMX
    kpreferencesenableMMX,
#endif
    kpreferencesdisableStatus,
    kpreferencesemulatorType,
    kpreferencesflashSize,
    kpreferencesframeSkip,
    kpreferencesfsColorDepth,
    kpreferencesfsFrequency,
    kpreferencesfsHeight,
    kpreferencesfsWidth,
    kpreferencesgbPaletteOption,
    kpreferencesgbPrinter,
    kpreferencesgdbBreakOnLoad,
    kpreferencesgdbPort,
#ifndef NO_LINK
    kpreferencesLinkNumPlayers,
#endif
    kpreferencesmaxScale,
    kpreferencespauseWhenInactive,
    kpreferencesrtcEnabled,
    kpreferencessaveType,
    kpreferencesshowSpeed,
    kpreferencesshowSpeedTransparent,
    kpreferencesskipBios,
    kpreferencesskipSaveGameCheats,
    kpreferencesskipSaveGameBattery,
    kpreferencesthrottle,
    kpreferencesspeedupThrottle,
    kpreferencesspeedupFrameSkip,
    kpreferencesspeedupThrottleFrameSkip,
    kpreferencesuseBiosGB,
    kpreferencesuseBiosGBA,
    kpreferencesuseBiosGBC,
    kpreferencesvsync,

    /// Geometry
    kgeometryfullScreen,
    kgeometryisMaximized,
    kgeometrywindowHeight,
    kgeometrywindowWidth,
    kgeometrywindowX,
    kgeometrywindowY,

    /// UI
    kuiallowKeyboardBackgroundInput,
    kuiallowJoystickBackgroundInput,
    kuihideMenuBar,

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

constexpr size_t kNbOptions = static_cast<size_t>(OptionID::Last);

// Represents a single option saved in the INI file. Option does not own the
// individual option, but keeps a pointer to where the data is actually saved.
//
// Ideally, options in the UI code should only be accessed and set via this
// class, which should also take care of updating the INI file when
// Option::Set*() is called. This should also handle keyboard and joystick
// configuration so option parsing can be done in a uniform manner. If we ever
// get to that point, we would be able to remove most update_opts() calls and
// have individual UI elements access the option via
// Option::FindByID().
//
// The implementation for this class is largely inspired by base::Value in
// Chromium.
// https://source.chromium.org/chromium/chromium/src/+/main:base/values.h
class Option {
public:
    enum class Type {
        kNone = 0,
        kBool,
        kDouble,
        kInt,
        kUnsigned,
        kString,
        kFilter,
        kInterframe,
        kRenderMethod,
        kAudioApi,
        kSoundQuality,
        kGbPalette,
    };

    static std::array<Option, kNbOptions>& AllOptions();

    // O(log(kNbOptions))
    static Option const* FindByName(const wxString& config_name);

    // O(1)
    static Option& FindByID(OptionID id);

    ~Option();

    // Accessors.
    const wxString& config_name() const { return config_name_; }
    const wxString& command() const { return command_; }
    const wxString& ux_helper() const { return ux_helper_; }

    // Returns the type of the value stored by the current object.
    Type type() const { return type_; }

    // Returns true if the current object represents a given type.
    bool is_none() const { return type() == Type::kNone; }
    bool is_bool() const { return type() == Type::kBool; }
    bool is_double() const { return type() == Type::kDouble; }
    bool is_int() const { return type() == Type::kInt; }
    bool is_unsigned() const { return type() == Type::kUnsigned; }
    bool is_string() const { return type() == Type::kString; }
    bool is_filter() const { return type() == Type::kFilter; }
    bool is_interframe() const { return type() == Type::kInterframe; }
    bool is_render_method() const { return type() == Type::kRenderMethod; }
    bool is_audio_api() const { return type() == Type::kAudioApi; }
    bool is_sound_quality() const { return type() == Type::kSoundQuality; }
    bool is_gb_palette() const { return type() == Type::kGbPalette; }

    // Returns a reference to the stored data. Will assert on type mismatch.
    // All enum types go through GetEnumString().
    bool GetBool() const;
    double GetDouble() const;
    int32_t GetInt() const;
    uint32_t GetUnsigned() const;
    const wxString GetString() const;
    wxString GetEnumString() const;
    wxString GetGbPaletteString() const;

    // Sets the value. Will assert on type mismatch.
    // All enum types go through SetEnumString() and SetEnumInt().
    void SetBool(bool value) const;
    void SetDouble(double value) const;
    void SetInt(int32_t value) const;
    void SetUnsigned(uint32_t value) const;
    void SetString(const wxString& value) const;
    void SetEnumString(const wxString& value) const;
    void SetEnumInt(int value) const;
    void SetGbPalette(const wxString& value) const;

    // Command-line helper string.
    wxString ToHelperString() const;

private:
    // Disable copy and assignment. Every individual option is unique.
    Option(const Option&) = delete;
    Option& operator=(const Option&) = delete;

    Option(OptionID id);
    Option(OptionID id, bool* option);
    Option(OptionID id, double* option, double min, double max);
    Option(OptionID id, int32_t* option, int32_t min, int32_t max);
    Option(OptionID id, uint32_t* option, uint32_t min, uint32_t max);
    Option(OptionID id, wxString* option);
    Option(OptionID id, int* option);
    Option(OptionID id, uint16_t* option);

    const OptionID id_;

    const wxString config_name_;
    const wxString command_;
    const wxString ux_helper_;

    const Type type_;
    const nonstd::variant<nonstd::monostate,
                          bool*,
                          double*,
                          int32_t*,
                          uint32_t*,
                          wxString*,
                          uint16_t*>
        value_;

    const nonstd::variant<nonstd::monostate, double, int32_t, uint32_t> min_;
    const nonstd::variant<nonstd::monostate, double, int32_t, uint32_t> max_;
};

}  // namespace config

#endif  // VBAM_WX_CONFIG_OPTIONS_H_
