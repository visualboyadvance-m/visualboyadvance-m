#include "vbam-options.h"

#include "nonstd/variant.hpp"

#include <wx/log.h>
#include <wx/translation.h>

#define VBAM_OPTIONS_INTERNAL_INCLUDE
#include "vbam-options-internal.h"
#undef VBAM_OPTIONS_INTERNAL_INCLUDE

// static
VbamOption const* VbamOption::FindOptionByName(const wxString &config_name) {
    nonstd::optional<VbamOptionID> option_id = internal::StringToOptionId(config_name);
    if (!option_id) {
        return nullptr;
    }
    return &FindOptionByID(option_id.value());
}

// static
VbamOption& VbamOption::FindOptionByID(VbamOptionID id) {
    assert (id != VbamOptionID::Last);
    return AllOptions()[static_cast<size_t>(id)];
}

VbamOption::~VbamOption() = default;

VbamOption::VbamOption(VbamOptionID id) :
    id_(id),
    config_name_(internal::kAllOptionsData[static_cast<size_t>(id)].config_name),
    command_(internal::kAllOptionsData[static_cast<size_t>(id)].command),
    ux_helper_(wxGetTranslation(internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper)),
    type_(internal::kAllOptionsData[static_cast<size_t>(id)].type),
    value_(),
    min_(),
    max_() {
    assert(id != VbamOptionID::Last);
    assert(is_none());
}

VbamOption::VbamOption(VbamOptionID id, bool* option) :
    id_(id),
    config_name_(internal::kAllOptionsData[static_cast<size_t>(id)].config_name),
    command_(internal::kAllOptionsData[static_cast<size_t>(id)].command),
    ux_helper_(wxGetTranslation(internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper)),
    type_(internal::kAllOptionsData[static_cast<size_t>(id)].type),
    value_(option),
    min_(),
    max_() {
    assert(id != VbamOptionID::Last);
    assert(is_bool());
}

VbamOption::VbamOption(VbamOptionID id, double* option, double min, double max) :
    id_(id),
    config_name_(internal::kAllOptionsData[static_cast<size_t>(id)].config_name),
    command_(internal::kAllOptionsData[static_cast<size_t>(id)].command),
    ux_helper_(wxGetTranslation(internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper)),
    type_(internal::kAllOptionsData[static_cast<size_t>(id)].type),
    value_(option),
    min_(min),
    max_(max) {
    assert(id != VbamOptionID::Last);
    assert(is_double());

    // Validate the initial value.
    SetDouble(*option);
}

VbamOption::VbamOption(VbamOptionID id, int32_t* option, int32_t min, int32_t max) :
    id_(id),
    config_name_(internal::kAllOptionsData[static_cast<size_t>(id)].config_name),
    command_(internal::kAllOptionsData[static_cast<size_t>(id)].command),
    ux_helper_(wxGetTranslation(internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper)),
    type_(internal::kAllOptionsData[static_cast<size_t>(id)].type),
    value_(option),
    min_(min),
    max_(max) {
    assert(id != VbamOptionID::Last);
    assert(is_int());

    // Validate the initial value.
    SetInt(*option);
}

VbamOption::VbamOption(VbamOptionID id, uint32_t* option, uint32_t min, uint32_t max) :
    id_(id),
    config_name_(internal::kAllOptionsData[static_cast<size_t>(id)].config_name),
    command_(internal::kAllOptionsData[static_cast<size_t>(id)].command),
    ux_helper_(wxGetTranslation(internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper)),
    type_(internal::kAllOptionsData[static_cast<size_t>(id)].type),
    value_(option),
    min_(min),
    max_(max) {
    assert(id != VbamOptionID::Last);
    assert(is_unsigned());

    // Validate the initial value.
    SetUnsigned(*option);
}

VbamOption::VbamOption(VbamOptionID id, wxString* option) :
    id_(id),
    config_name_(internal::kAllOptionsData[static_cast<size_t>(id)].config_name),
    command_(internal::kAllOptionsData[static_cast<size_t>(id)].command),
    ux_helper_(wxGetTranslation(internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper)),
    type_(internal::kAllOptionsData[static_cast<size_t>(id)].type),
    value_(option),
    min_(),
    max_() {
    assert(id != VbamOptionID::Last);
    assert(is_string());
}

VbamOption::VbamOption(VbamOptionID id, int* option) :
    id_(id),
    config_name_(internal::kAllOptionsData[static_cast<size_t>(id)].config_name),
    command_(internal::kAllOptionsData[static_cast<size_t>(id)].command),
    ux_helper_(wxGetTranslation(internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper)),
    type_(internal::kAllOptionsData[static_cast<size_t>(id)].type),
    value_(option),
    min_(0),
    max_(internal::MaxForType(type_)) {
    assert(id != VbamOptionID::Last);
    assert(is_filter() || is_interframe() || is_render_method() || is_audio_api() || is_sound_quality());

    // Validate the initial value.
    SetEnumInt(*option);
}

VbamOption::VbamOption(VbamOptionID id, uint16_t* option) :
    id_(id),
    config_name_(internal::kAllOptionsData[static_cast<size_t>(id)].config_name),
    command_(internal::kAllOptionsData[static_cast<size_t>(id)].command),
    ux_helper_(wxGetTranslation(internal::kAllOptionsData[static_cast<size_t>(id)].ux_helper)),
    type_(internal::kAllOptionsData[static_cast<size_t>(id)].type),
    value_(option),
    min_(),
    max_() {
    assert(id != VbamOptionID::Last);
    assert(is_gb_palette());
}

bool VbamOption::GetBool() const {
    assert(is_bool());
    return *(nonstd::get<bool*>(value_));
}

double VbamOption::GetDouble() const {
    assert(is_double());
    return *(nonstd::get<double*>(value_));
}

int32_t VbamOption::GetInt() const {
    assert(is_int());
    return *(nonstd::get<int32_t*>(value_));
}

uint32_t VbamOption::GetUnsigned() const {
    assert(is_unsigned());
    return *(nonstd::get<uint32_t*>(value_));
}

const wxString VbamOption::GetString() const {
    assert(is_string());
    return *(nonstd::get<wxString*>(value_));
}

wxString VbamOption::GetEnumString() const {
    switch (type_) {
    case VbamOption::Type::kFilter:
        return internal::FilterToString(*(nonstd::get<int32_t*>(value_)));
    case VbamOption::Type::kInterframe:
        return internal::InterframeToString(*(nonstd::get<int32_t*>(value_)));
    case VbamOption::Type::kRenderMethod:
        return internal::RenderMethodToString(*(nonstd::get<int32_t*>(value_)));
    case VbamOption::Type::kAudioApi:
        return internal::AudioApiToString(*(nonstd::get<int32_t*>(value_)));
    case VbamOption::Type::kSoundQuality:
        return internal::SoundQualityToString(*(nonstd::get<int32_t*>(value_)));

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

wxString VbamOption::GetGbPaletteString() const {
    assert(is_gb_palette());

    wxString palette_string;
    uint16_t const* value = nonstd::get<uint16_t*>(value_);
    palette_string.Printf("%04X,%04X,%04X,%04X,%04X,%04X,%04X,%04X",
        value[0], value[1], value[2], value[3],
        value[4], value[5], value[6], value[7]);
    return palette_string;
}

void VbamOption::SetBool(bool value) const {
    assert(is_bool());
    *nonstd::get<bool*>(value_) = value;
}

void VbamOption::SetDouble(double value) const {
    assert(is_double());
    if (value < nonstd::get<double>(min_) || value > nonstd::get<double>(max_)) {
        wxLogWarning(
            _("Invalid value %f for option %s; valid values are %f - %f"),
            value,
            config_name_,
            nonstd::get<double>(min_),
            nonstd::get<double>(max_));
        return;
    }
    *nonstd::get<double*>(value_) = value;
}

void VbamOption::SetInt(int32_t value) const {
    assert(is_int());
    if (value < nonstd::get<int32_t>(min_) || value > nonstd::get<int32_t>(max_)) {
        wxLogWarning(
            _("Invalid value %d for option %s; valid values are %d - %d"),
            value,
            config_name_,
            nonstd::get<int32_t>(min_),
            nonstd::get<int32_t>(max_));
        return;
    }
    *nonstd::get<int32_t*>(value_) = value;
}

void VbamOption::SetUnsigned(uint32_t value) const {
    assert(is_unsigned());
    if (value < nonstd::get<uint32_t>(min_) || value > nonstd::get<uint32_t>(max_)) {
        wxLogWarning(
            _("Invalid value %d for option %s; valid values are %d - %d"),
            value,
            config_name_,
            nonstd::get<uint32_t>(min_),
            nonstd::get<uint32_t>(max_));
        return;
    }
    *nonstd::get<uint32_t*>(value_) = value;
}

void VbamOption::SetString(const wxString& value) const {
    assert(is_string());
    *nonstd::get<wxString*>(value_) = value;
}

void VbamOption::SetEnumString(const wxString& value) const {
    switch (type_) {
    case VbamOption::Type::kFilter:
        SetEnumInt(internal::StringToFilter(config_name_, value));
        return;
    case VbamOption::Type::kInterframe:
        SetEnumInt(internal::StringToInterframe(config_name_, value));
        return;
    case VbamOption::Type::kRenderMethod:
        SetEnumInt(internal::StringToRenderMethod(config_name_, value));
        return;
    case VbamOption::Type::kAudioApi:
        SetEnumInt(internal::StringToAudioApi(config_name_, value));
        return;
    case VbamOption::Type::kSoundQuality:
        SetEnumInt(internal::StringToSoundQuality(config_name_, value));
        return;

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
        return;
    }
}

void VbamOption::SetEnumInt(int value) const {
    assert(is_filter() || is_interframe() || is_render_method() || is_audio_api() || is_sound_quality());
    if (value < nonstd::get<int32_t>(min_) || value > nonstd::get<int32_t>(max_)) {
        wxLogWarning(
            _("Invalid value %d for option %s; valid values are %s"),
            value,
            config_name_,
            internal::AllEnumValuesForType(type_));
        return;
    }
    *nonstd::get<int32_t*>(value_) = value;
}

void VbamOption::SetGbPalette(const wxString& value) const {
    assert(is_gb_palette());

    // 8 values of 4 chars and 7 commas.
    static constexpr size_t kPaletteStringSize = 8 * 4 + 7;

    if (value.size() != kPaletteStringSize) {
        wxLogWarning(_("Invalid value %s for option %s"),
             value,
             config_name_);
        return;
    }
    uint16_t* dest = nonstd::get<uint16_t*>(value_);

    for (size_t i = 0; i < 8; i++) {
        wxString number = value.substr(i * 5, 4);
        long temp = 0;
        if (number.ToLong(&temp, 16)) {
            dest[i] = temp;
        }
    }
}

wxString VbamOption::ToHelperString() const {
    wxString helper_string = config_name_;

    switch (type_) {
    case VbamOption::Type::kNone:
        break;
    case VbamOption::Type::kBool:
        helper_string.Append(" (flag)");
        break;
    case VbamOption::Type::kDouble:
        helper_string.Append(" (decimal)");
        break;
    case VbamOption::Type::kInt:
        helper_string.Append(" (int)");
        break;
    case VbamOption::Type::kUnsigned:
        helper_string.Append(" (unsigned)");
        break;
    case VbamOption::Type::kString:
        helper_string.Append(" (string)");
        break;
    case VbamOption::Type::kFilter:
    case VbamOption::Type::kInterframe:
    case VbamOption::Type::kRenderMethod:
    case VbamOption::Type::kAudioApi:
    case VbamOption::Type::kSoundQuality:
        helper_string.Append(" (");
        helper_string.Append(internal::AllEnumValuesForType(type_));
        helper_string.Append(")");
        break;
    case VbamOption::Type::kGbPalette:
        helper_string.Append(" (XXXX,XXXX,XXXX,XXXX,XXXX,XXXX,XXXX,XXXX)");
        break;
    }
    helper_string.Append("\n\t");
    helper_string.Append(ux_helper_);
    helper_string.Append("\n");

    return helper_string;
}
