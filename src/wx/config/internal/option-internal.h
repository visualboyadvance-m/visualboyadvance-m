#ifndef VBAM_OPTION_INTERNAL_INCLUDE
#error "Do not include "config/internal/option-internal.h" outside of the implementation."
#endif

#include <array>

#include <optional.hpp>

#include <wx/string.h>

#include "wx/config/option.h"

namespace config {
namespace internal {

struct OptionData {
    const wxString config_name;
    const wxString command;
    const wxString ux_helper;
};

// Static data to initialize global values.
extern const std::array<OptionData, kNbOptions + 1> kAllOptionsData;

// Conversion utilities.
nonstd::optional<OptionID> StringToOptionId(const wxString& input);
wxString FilterToString(const Filter& value);
wxString InterframeToString(const Interframe& value);
wxString RenderMethodToString(const RenderMethod& value);
wxString ColorCorrectionProfileToString(const ColorCorrectionProfile& value);
wxString AudioApiToString(const AudioApi& value);
wxString AudioRateToString(const AudioRate& value);
Filter StringToFilter(const wxString& config_name, const wxString& input);
Interframe StringToInterframe(const wxString& config_name, const wxString& input);
RenderMethod StringToRenderMethod(const wxString& config_name, const wxString& input);
ColorCorrectionProfile StringToColorCorrectionProfile(const wxString& config_name, const wxString& input);
AudioApi StringToAudioApi(const wxString& config_name, const wxString& input);
AudioRate StringToSoundQuality(const wxString& config_name, const wxString& input);

wxString AllEnumValuesForType(Option::Type type);

// Max value for enum types.
size_t MaxForType(Option::Type type);

}  // namespace internal
}  // namespace config
