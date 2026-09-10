#include "qt/config/option.h"

#include <cstring>

#include <variant.hpp>

#include <QCoreApplication>

#define VBAM_OPTION_INTERNAL_INCLUDE
#include "qt/config/internal/option-internal.h"
#undef VBAM_OPTION_INTERNAL_INCLUDE

#include "core/base/check.h"
#include "qt/config/option-proxy.h"
#include "qt/log.h"

namespace config {

namespace {

const QString& ConfigName(OptionID id) {
    return internal::kAllOptionsData[static_cast<size_t>(id)].config_name;
}

const QString& CommandName(OptionID id) {
    return internal::kAllOptionsData[static_cast<size_t>(id)].command;
}

QString UxHelper(OptionID id) {
    const char* helper = internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper;
    if (!helper || !*helper) {
        return QString();
    }
    return QCoreApplication::translate("vbam", helper);
}

}  // namespace

// static
Option* Option::ByName(const QString& config_name) {
    nonstd::optional<OptionID> option_id = internal::StringToOptionId(config_name);
    if (!option_id) {
        return nullptr;
    }
    return ByID(option_id.value());
}

// static
Option* Option::ByID(OptionID id) {
    VBAM_CHECK(id != OptionID::Last);
    return &All()[static_cast<size_t>(id)];
}

Option::~Option() = default;

Option::Observer::Observer(OptionID option_id) : option_(Option::ByID(option_id)) {
    VBAM_CHECK(option_);
    option_->AddObserver(this);
}
Option::Observer::~Observer() {
    option_->RemoveObserver(this);
}

Option::Option(OptionID id)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(),
      min_(),
      max_() {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_none());
}

Option::Option(OptionID id, bool* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_() {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_bool());
}

Option::Option(OptionID id, double* option, double min, double max)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(min),
      max_(max) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_double());

    // Validate the initial value.
    SetDouble(*option);
}

Option::Option(OptionID id, int32_t* option, int32_t min, int32_t max)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(min),
      max_(max) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_int());

    // Validate the initial value.
    SetInt(*option);

    numeric_default_ = *option;
}

Option::Option(OptionID id, uint32_t* option, uint32_t min, uint32_t max)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(min),
      max_(max) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_unsigned());

    // Validate the initial value.
    SetUnsigned(*option);

    numeric_default_ = *option;
}

Option::Option(OptionID id, QString* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_() {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_string());
}

Option::Option(OptionID id, Filter* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_(nonstd::in_place_type<uint64_t>, internal::MaxForType(type_)) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_filter());
}

Option::Option(OptionID id, Interframe* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_(nonstd::in_place_type<uint64_t>, internal::MaxForType(type_)) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_interframe());
}

Option::Option(OptionID id, RenderMethod* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_(nonstd::in_place_type<uint64_t>, internal::MaxForType(type_)) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_render_method());
}

Option::Option(OptionID id, ColorCorrectionProfile* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_(nonstd::in_place_type<uint64_t>, internal::MaxForType(type_)) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_color_correction_profile());
}

Option::Option(OptionID id, AudioApi* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_(nonstd::in_place_type<uint64_t>, internal::MaxForType(type_)) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_audio_api());
}

Option::Option(OptionID id, AudioRate* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_(nonstd::in_place_type<uint64_t>, internal::MaxForType(type_)) {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_audio_rate());
}

Option::Option(OptionID id, uint16_t* option)
    : id_(id),
      config_name_(ConfigName(id)),
      command_(CommandName(id)),
      ux_helper_(UxHelper(id)),
      type_(kOptionsTypes[static_cast<size_t>(id)]),
      value_(option),
      min_(),
      max_() {
    VBAM_CHECK(id != OptionID::Last);
    VBAM_CHECK(is_gb_palette());
}

bool Option::GetBool() const {
    VBAM_CHECK(is_bool());
    return *(nonstd::get<bool*>(value_));
}

double Option::GetDouble() const {
    VBAM_CHECK(is_double());
    return *(nonstd::get<double*>(value_));
}

int32_t Option::GetInt() const {
    VBAM_CHECK(is_int());
    return *(nonstd::get<int32_t*>(value_));
}

uint32_t Option::GetUnsigned() const {
    VBAM_CHECK(is_unsigned());
    return *(nonstd::get<uint32_t*>(value_));
}

const QString& Option::GetString() const {
    VBAM_CHECK(is_string());
    return *(nonstd::get<QString*>(value_));
}

Filter Option::GetFilter() const {
    VBAM_CHECK(is_filter());
    return *(nonstd::get<Filter*>(value_));
}

Interframe Option::GetInterframe() const {
    VBAM_CHECK(is_interframe());
    return *(nonstd::get<Interframe*>(value_));
}

RenderMethod Option::GetRenderMethod() const {
    VBAM_CHECK(is_render_method());
    return *(nonstd::get<RenderMethod*>(value_));
}

ColorCorrectionProfile Option::GetColorCorrectionProfile() const {
    VBAM_CHECK(is_color_correction_profile());
    return *(nonstd::get<ColorCorrectionProfile*>(value_));
}

AudioApi Option::GetAudioApi() const {
    VBAM_CHECK(is_audio_api());
    return *(nonstd::get<AudioApi*>(value_));
}

AudioRate Option::GetAudioRate() const {
    VBAM_CHECK(is_audio_rate());
    return *(nonstd::get<AudioRate*>(value_));
}

QString Option::GetEnumString() const {
    switch (type_) {
        case Option::Type::kFilter:
            return internal::FilterToString(GetFilter());
        case Option::Type::kInterframe:
            return internal::InterframeToString(GetInterframe());
        case Option::Type::kRenderMethod:
            return internal::RenderMethodToString(GetRenderMethod());
        case Option::Type::kColorCorrectionProfile:
            return internal::ColorCorrectionProfileToString(GetColorCorrectionProfile());
        case Option::Type::kAudioApi:
            return internal::AudioApiToString(GetAudioApi());
        case Option::Type::kAudioRate:
            return internal::AudioRateToString(GetAudioRate());

        // We don't use default here to explicitly trigger a compiler warning
        // when adding a new value.
        case Option::Type::kNone:
        case Option::Type::kBool:
        case Option::Type::kDouble:
        case Option::Type::kInt:
        case Option::Type::kUnsigned:
        case Option::Type::kString:
        case Option::Type::kGbPalette:
            VBAM_CHECK(false);
            return QString();
    }
    VBAM_NOTREACHED_RETURN(QString());
}

std::array<uint16_t, 8> Option::GetGbPalette() const {
    VBAM_CHECK(is_gb_palette());

    const uint16_t* raw_palette = (nonstd::get<uint16_t*>(value_));
    std::array<uint16_t, 8> palette;
    std::memcpy(palette.data(), raw_palette, sizeof(palette));
    return palette;
}

QString Option::GetGbPaletteString() const {
    VBAM_CHECK(is_gb_palette());

    uint16_t const* value = nonstd::get<uint16_t*>(value_);
    QString palette_string;
    for (size_t i = 0; i < 8; i++) {
        if (i) {
            palette_string += ',';
        }
        palette_string += QString("%1").arg(value[i], 4, 16, QChar('0')).toUpper();
    }
    return palette_string;
}

bool Option::SetBool(bool value) {
    VBAM_CHECK(is_bool());
    bool old_value = GetBool();
    *nonstd::get<bool*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetDouble(double value) {
    VBAM_CHECK(is_double());
    double old_value = GetDouble();
    if (value < nonstd::get<double>(min_) || value > nonstd::get<double>(max_)) {
        vbam::LogWarning(
            QCoreApplication::translate("vbam",
                                        "Invalid value %1 for option %2; valid values are %3 - %4")
                .arg(value)
                .arg(config_name_)
                .arg(nonstd::get<double>(min_))
                .arg(nonstd::get<double>(max_)));
        return false;
    }
    *nonstd::get<double*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetInt(int32_t value) {
    VBAM_CHECK(is_int());
    int old_value = GetInt();
    if (value < nonstd::get<int32_t>(min_) || value > nonstd::get<int32_t>(max_)) {
        vbam::LogWarning(
            QCoreApplication::translate("vbam",
                                        "Invalid value %1 for option %2; valid values are %3 - %4")
                .arg(value)
                .arg(config_name_)
                .arg(nonstd::get<int32_t>(min_))
                .arg(nonstd::get<int32_t>(max_)));
        return false;
    }
    *nonstd::get<int32_t*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetUnsigned(uint32_t value) {
    VBAM_CHECK(is_unsigned());
    uint32_t old_value = GetUnsigned();
    if (value < nonstd::get<uint32_t>(min_) || value > nonstd::get<uint32_t>(max_)) {
        vbam::LogWarning(
            QCoreApplication::translate("vbam",
                                        "Invalid value %1 for option %2; valid values are %3 - %4")
                .arg(value)
                .arg(config_name_)
                .arg(nonstd::get<uint32_t>(min_))
                .arg(nonstd::get<uint32_t>(max_)));
        return false;
    }
    *nonstd::get<uint32_t*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetString(const QString& value) {
    VBAM_CHECK(is_string());
    const QString old_value = GetString();
    *nonstd::get<QString*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetFilter(const Filter& value) {
    VBAM_CHECK(is_filter());
    VBAM_CHECK(value < Filter::kLast);
    const Filter old_value = GetFilter();
    *nonstd::get<Filter*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetInterframe(const Interframe& value) {
    VBAM_CHECK(is_interframe());
    VBAM_CHECK(value < Interframe::kLast);
    const Interframe old_value = GetInterframe();
    *nonstd::get<Interframe*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetRenderMethod(const RenderMethod& value) {
    VBAM_CHECK(is_render_method());
    VBAM_CHECK(value < RenderMethod::kLast);
    const RenderMethod old_value = GetRenderMethod();
    *nonstd::get<RenderMethod*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetColorCorrectionProfile(const ColorCorrectionProfile& value) {
    VBAM_CHECK(is_color_correction_profile());
    VBAM_CHECK(value < ColorCorrectionProfile::kLast);
    const ColorCorrectionProfile old_value = GetColorCorrectionProfile();
    *nonstd::get<ColorCorrectionProfile*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetAudioApi(const AudioApi& value) {
    VBAM_CHECK(is_audio_api());
    VBAM_CHECK(value < AudioApi::kLast);
    const AudioApi old_value = GetAudioApi();
    *nonstd::get<AudioApi*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetAudioRate(const AudioRate& value) {
    VBAM_CHECK(is_audio_rate());
    VBAM_CHECK(value < AudioRate::kLast);
    const AudioRate old_value = GetAudioRate();
    *nonstd::get<AudioRate*>(value_) = value;
    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetEnumString(const QString& value) {
    switch (type_) {
        case Option::Type::kFilter:
            return SetFilter(internal::StringToFilter(config_name_, value));
        case Option::Type::kInterframe:
            return SetInterframe(internal::StringToInterframe(config_name_, value));
        case Option::Type::kRenderMethod:
            return SetRenderMethod(internal::StringToRenderMethod(config_name_, value));
        case Option::Type::kColorCorrectionProfile:
            return SetColorCorrectionProfile(
                internal::StringToColorCorrectionProfile(config_name_, value));
        case Option::Type::kAudioApi:
            return SetAudioApi(internal::StringToAudioApi(config_name_, value));
        case Option::Type::kAudioRate:
            return SetAudioRate(internal::StringToSoundQuality(config_name_, value));

        // We don't use default here to explicitly trigger a compiler warning
        // when adding a new value.
        case Option::Type::kNone:
        case Option::Type::kBool:
        case Option::Type::kDouble:
        case Option::Type::kInt:
        case Option::Type::kUnsigned:
        case Option::Type::kString:
        case Option::Type::kGbPalette:
            VBAM_CHECK(false);
            return false;
    }
    VBAM_NOTREACHED_RETURN(false);
}

bool Option::SetGbPalette(const std::array<uint16_t, 8>& value) {
    VBAM_CHECK(is_gb_palette());

    uint16_t* dest = nonstd::get<uint16_t*>(value_);

    // Keep a copy of the current value.
    std::array<uint16_t, 8> old_value;
    std::copy(dest, dest + 8, old_value.data());

    // Set the new value.
    std::copy(value.begin(), value.end(), dest);

    if (old_value != value) {
        CallObservers();
    }
    return true;
}

bool Option::SetGbPaletteString(const QString& value) {
    VBAM_CHECK(is_gb_palette());

    // 8 values of 4 chars and 7 commas.
    static constexpr int kPaletteStringSize = 8 * 4 + 7;

    if (value.size() != kPaletteStringSize) {
        vbam::LogWarning(QCoreApplication::translate("vbam", "Invalid value %1 for option %2")
                             .arg(value)
                             .arg(config_name_));
        return false;
    }

    std::array<uint16_t, 8> new_value;
    for (int i = 0; i < 8; i++) {
        const QString number = value.mid(i * 5, 4);
        bool ok = false;
        const uint temp = number.toUInt(&ok, 16);
        if (ok) {
            new_value[i] = static_cast<uint16_t>(temp);
        } else {
            vbam::LogWarning(QCoreApplication::translate("vbam", "Invalid value %1 for option %2")
                                 .arg(value)
                                 .arg(config_name_));
            return false;
        }
    }

    return SetGbPalette(new_value);
}

double Option::GetDoubleMin() const {
    VBAM_CHECK(is_double());
    return nonstd::get<double>(min_);
}

double Option::GetDoubleMax() const {
    VBAM_CHECK(is_double());
    return nonstd::get<double>(max_);
}

int32_t Option::GetIntMin() const {
    VBAM_CHECK(is_int());
    return nonstd::get<int32_t>(min_);
}

int32_t Option::GetIntMax() const {
    VBAM_CHECK(is_int());
    return nonstd::get<int32_t>(max_);
}

int32_t Option::GetIntDefault() const {
    VBAM_CHECK(is_int());
    return static_cast<int32_t>(numeric_default_);
}

uint32_t Option::GetUnsignedDefault() const {
    VBAM_CHECK(is_unsigned());
    return static_cast<uint32_t>(numeric_default_);
}

bool Option::ResetToDefault() {
    switch (type_) {
        case Option::Type::kInt:
            return SetInt(static_cast<int32_t>(numeric_default_));
        case Option::Type::kUnsigned:
            return SetUnsigned(static_cast<uint32_t>(numeric_default_));
        default:
            return false;
    }
}

uint32_t Option::GetUnsignedMin() const {
    VBAM_CHECK(is_unsigned());
    return nonstd::get<uint32_t>(min_);
}

uint32_t Option::GetUnsignedMax() const {
    VBAM_CHECK(is_unsigned());
    return nonstd::get<uint32_t>(max_);
}

size_t Option::GetEnumMax() const {
    VBAM_CHECK(is_filter() || is_interframe() || is_render_method() ||
               is_color_correction_profile() || is_audio_api() || is_audio_rate());
    return static_cast<size_t>(nonstd::get<uint64_t>(max_));
}

void Option::NextFilter() {
    VBAM_CHECK(is_filter());
    const int old_value = static_cast<int>(GetFilter());
    // Skip kPlugin during cycling - cycle through kNone to kScaleFX9x only
    const int max_filter = static_cast<int>(Filter::kPlugin);
    const int new_value = (old_value + 1) % max_filter;
    SetFilter(static_cast<Filter>(new_value));
}

void Option::NextInterframe() {
    VBAM_CHECK(is_interframe());
    const int old_value = static_cast<int>(GetInterframe());
    const int new_value = (old_value + 1) % kNbInterframes;
    SetInterframe(static_cast<Interframe>(new_value));
}

QString Option::ToHelperString() const {
    QString helper_string = config_name_;

    switch (type_) {
        case Option::Type::kNone:
            break;
        case Option::Type::kBool:
            helper_string.append(" (flag)");
            break;
        case Option::Type::kDouble:
            helper_string.append(" (decimal)");
            break;
        case Option::Type::kInt:
            helper_string.append(" (int)");
            break;
        case Option::Type::kUnsigned:
            helper_string.append(" (unsigned)");
            break;
        case Option::Type::kString:
            helper_string.append(" (string)");
            break;
        case Option::Type::kFilter:
        case Option::Type::kInterframe:
        case Option::Type::kRenderMethod:
        case Option::Type::kColorCorrectionProfile:
        case Option::Type::kAudioApi:
        case Option::Type::kAudioRate:
            helper_string.append(" (");
            helper_string.append(internal::AllEnumValuesForType(type_));
            helper_string.append(")");
            break;
        case Option::Type::kGbPalette:
            helper_string.append(" (XXXX,XXXX,XXXX,XXXX,XXXX,XXXX,XXXX,XXXX)");
            break;
    }
    helper_string.append("\n\t");
    helper_string.append(ux_helper_);
    helper_string.append("\n");

    return helper_string;
}

void Option::AddObserver(Observer* observer) {
    VBAM_CHECK(observer);
    [[maybe_unused]] const auto pair = observers_.emplace(observer);
    VBAM_CHECK(pair.second);
}

void Option::RemoveObserver(Observer* observer) {
    VBAM_CHECK(observer);
    [[maybe_unused]] const size_t removed = observers_.erase(observer);
    VBAM_CHECK(removed == 1u);
}

void Option::CallObservers() {
    VBAM_CHECK(!calling_observers_);
    calling_observers_ = true;
    for (const auto observer : observers_) {
        observer->OnValueChanged();
    }
    calling_observers_ = false;
}

}  // namespace config
