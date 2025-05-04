#ifndef VBAM_WX_CONFIG_OPTIONS_H_
#define VBAM_WX_CONFIG_OPTIONS_H_

#include <array>
#include <cstdint>
#include <unordered_set>

#include "variant.hpp"

#include <wx/string.h>

#include "wx/config/option-id.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::int8_t;
using std::int16_t;
using std::int32_t;

namespace config {

// Values for kDispFilter.
enum class Filter {
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
    kPlugin,  // This must always be last.

    // Do not add anything under here.
    kLast,
};
static constexpr size_t kNbFilters = static_cast<size_t>(Filter::kLast);

// Values for kDispIFB.
enum class Interframe {
    kNone = 0,
    kSmart,
    kMotionBlur,

    // Do not add anything under here.
    kLast,
};
static constexpr size_t kNbInterframes = static_cast<size_t>(Interframe::kLast);

// Values for kDispRenderMethod.
enum class RenderMethod {
    kSimple = 0,
    kOpenGL,
    kSDL,
#if defined(__WXMSW__) && !defined(NO_D3D)
    kDirect3d,
#elif defined(__WXMAC__)
    kQuartz2d,
#ifndef NO_METAL
    kMetal,
#endif
#endif

    // Do not add anything under here.
    kLast,
};
static constexpr size_t kNbRenderMethods = static_cast<size_t>(RenderMethod::kLast);

// Values for kAudioApi.
enum class AudioApi {
    kOpenAL,
    kSDL,
#if defined(__WXMSW__)
    kDirectSound,
#endif  // __WXMSW__
#if defined(VBAM_ENABLE_XAUDIO2)
    kXAudio2,
#endif  // VBAM_ENABLE_XAUDIO2
#if defined(VBAM_ENABLE_FAUDIO)
    kFAudio,
#endif  // VBAM_ENABLE_FAUDIO

    // Do not add anything under here.
    kLast,
};
static constexpr size_t kNbAudioApis = static_cast<size_t>(AudioApi::kLast);

enum class AudioRate {
    k48kHz = 0,
    k44kHz,
    k22kHz,
    k11kHz,

    // Do not add anything under here.
    kLast,
};
static constexpr size_t kNbSoundRate = static_cast<size_t>(AudioRate::kLast);

// This is incremented whenever we want to change a default value between
// release versions. The option update code is in load_opts.
static constexpr uint32_t kIniLatestVersion = 1;

// Represents a single option saved in the INI file. Option does not own the
// individual option, but keeps a pointer to where the data is actually saved.
//
// Ideally, options in the UI code should only be accessed and set via this
// class, which should also take care of updating the INI file when
// Option::Set*() is called. This should also handle keyboard and joystick
// configuration so option parsing can be done in a uniform manner. If we ever
// get to that point, we would be able to remove most update_opts() calls and
// have individual UI elements access the option via Option::ByID().
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
        kAudioRate,
        kGbPalette,
    };

    // Observer for an option. OnValueChanged() will be called when the value
    // has changed. Implementers should take care of not modifying option()
    // in the OnValueChanged() handler.
    class Observer {
    public:
        explicit Observer(config::OptionID option_id);
        virtual ~Observer();

        // Class is move-only.
        Observer(const Observer&) = delete;
        Observer& operator=(const Observer&) = delete;
        Observer(Observer&& other) = default;
        Observer& operator=(Observer&& other) = default;

        virtual void OnValueChanged() = 0;

    protected:
        Option* option() const { return option_; }

    private:
        Option* option_;
    };

    static std::array<Option, kNbOptions>& All();

    // O(log(kNbOptions))
    static Option* ByName(const wxString& config_name);

    // O(1)
    static Option* ByID(OptionID id);

    ~Option();

    // Accessors.
    const wxString& config_name() const { return config_name_; }
    const wxString& command() const { return command_; }
    const wxString& ux_helper() const { return ux_helper_; }
    const OptionID& id() const { return id_; }

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
    bool is_audio_rate() const { return type() == Type::kAudioRate; }
    bool is_gb_palette() const { return type() == Type::kGbPalette; }

    // Returns a reference to the stored data. Will assert on type mismatch.
    // Only enum types can use GetEnumString().
    bool GetBool() const;
    double GetDouble() const;
    int32_t GetInt() const;
    uint32_t GetUnsigned() const;
    const wxString& GetString() const;
    Filter GetFilter() const;
    Interframe GetInterframe() const;
    RenderMethod GetRenderMethod() const;
    AudioApi GetAudioApi() const;
    AudioRate GetAudioRate() const;
    wxString GetEnumString() const;
    std::array<uint16_t, 8> GetGbPalette() const;
    wxString GetGbPaletteString() const;

    // Sets the value. Will assert on type mismatch.
    // Only enum types can use SetEnumString().
    // Returns true on success. On failure, the value will not be modified.
    bool SetBool(bool value);
    bool SetDouble(double value);
    bool SetInt(int32_t value);
    bool SetUnsigned(uint32_t value);
    bool SetString(const wxString& value);
    bool SetFilter(const Filter& value);
    bool SetInterframe(const Interframe& value);
    bool SetRenderMethod(const RenderMethod& value);
    bool SetAudioApi(const AudioApi& value);
    bool SetAudioRate(const AudioRate& value);
    bool SetEnumString(const wxString& value);
    bool SetGbPalette(const std::array<uint16_t, 8>& value);
    bool SetGbPaletteString(const wxString& value);

    // Min/Max accessors.
    double GetDoubleMin() const;
    double GetDoubleMax() const;
    int32_t GetIntMin() const;
    int32_t GetIntMax() const;
    uint32_t GetUnsignedMin() const;
    uint32_t GetUnsignedMax() const;
    size_t GetEnumMax() const;

    // Special convenience modifiers.
    void NextFilter();
    void NextInterframe();

    // Command-line helper string.
    wxString ToHelperString() const;

private:
    // Disable copy and assignment. Every individual option is unique.
    Option(const Option&) = delete;
    Option& operator=(const Option&) = delete;

    explicit Option(OptionID id);
    Option(OptionID id, bool* option);
    Option(OptionID id, double* option, double min, double max);
    Option(OptionID id, int32_t* option, int32_t min, int32_t max);
    Option(OptionID id, uint32_t* option, uint32_t min, uint32_t max);
    Option(OptionID id, wxString* option);
    Option(OptionID id, Filter* option);
    Option(OptionID id, Interframe* option);
    Option(OptionID id, RenderMethod* option);
    Option(OptionID id, AudioApi* option);
    Option(OptionID id, AudioRate* option);
    Option(OptionID id, int* option);
    Option(OptionID id, uint16_t* option);

    // Observer.
    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);
    void CallObservers();
    std::unordered_set<Observer*> observers_;

    // Set to true when the observers are being called. This will fire an assert
    // to prevent modifying the object again, which would trigger an infinite
    // call stack.
    bool calling_observers_ = false;

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
                          Filter*,
                          Interframe*,
                          RenderMethod*,
                          AudioApi*,
                          AudioRate*,
                          uint16_t*>
        value_;

    // Technically, `uint64_t` is only needed for 64 bits targets, as `size_t`.
    // However, `size_t` is the same as `uint32_t` on 32 bits targets, resulting
    // in a compiler error if we use `size_t` here.
    const nonstd::variant<nonstd::monostate, double, int32_t, uint32_t> min_;
    const nonstd::variant<nonstd::monostate, double, int32_t, uint32_t, uint64_t> max_;
};

}  // namespace config

#endif  // VBAM_WX_CONFIG_OPTIONS_H_
