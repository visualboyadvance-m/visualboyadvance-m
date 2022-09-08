#ifndef VBAM_OPTIONS_H
#define VBAM_OPTIONS_H

#include <array>
#include <variant>

#include <wx/string.h>

enum class VbamOptionID {
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

constexpr size_t kNbOptions = static_cast<size_t>(VbamOptionID::Last);

// Represents a single option saved in the INI file. VbamOption does not own the
// individual option, but keeps a pointer to where the data is actually saved.
//
// Ideally, options in the UI code should only be accessed and set via this
// class, which should also take care of updating the INI file when
// VbamOption::Set*() is called. This should also handle keyboard and joystick
// configuration so option parsing can be done in a uniform manner. If we ever
// get to that point, we would be able to remove most update_opts() calls and
// have individual UI elements access the option via
// VbamOption::FindOptionByID().
//
// The implementation for this class is largely inspired by base::Value in
// Chromium.
// https://source.chromium.org/chromium/chromium/src/+/main:base/values.h
class VbamOption {
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

    static std::array<VbamOption, kNbOptions>& AllOptions();

    // O(log(kNbOptions))
    static VbamOption const* FindOptionByName(const wxString& config_name);

    // O(1)
    static VbamOption& FindOptionByID(VbamOptionID id);

    ~VbamOption();

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
    VbamOption(const VbamOption&) = delete;
    VbamOption& operator=(const VbamOption&) = delete;

    VbamOption(VbamOptionID id);
    VbamOption(VbamOptionID id, bool* option);
    VbamOption(VbamOptionID id, double* option, double min, double max);
    VbamOption(VbamOptionID id, int32_t* option, int32_t min, int32_t max);
    VbamOption(VbamOptionID id, uint32_t* option, uint32_t min, uint32_t max);
    VbamOption(VbamOptionID id, wxString* option);
    VbamOption(VbamOptionID id, int* option);
    VbamOption(VbamOptionID id, uint16_t* option);

    const VbamOptionID id_;

    const wxString config_name_;
    const wxString command_;
    const wxString ux_helper_;

    const Type type_;
    const std::variant<
            std::monostate,
            bool*,
            double*,
            int32_t*,
            uint32_t*,
            wxString*,
            uint16_t*>
        value_;

    const std::variant<std::monostate, double, int32_t, uint32_t> min_;
    const std::variant<std::monostate, double, int32_t, uint32_t> max_;
};

#endif /* VBAM_OPTIONS_H */
