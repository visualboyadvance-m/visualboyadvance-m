#ifndef VBAM_OPTION_INTERNAL_INCLUDE
#error "Do not include "config/internal/option-internal.h" outside of the implementation."
#endif

#include <wx/string.h>
#include <array>
#include <string>

#include "config/option.h"
#include "nonstd/optional.hpp"

namespace config {
namespace internal {

struct OptionData {
    const wxString config_name;
    const wxString command;
    const wxString ux_helper;
    const Option::Type type;
};

// Static data to initialize global values.
extern const std::array<OptionData, kNbOptions + 1> kAllOptionsData;

// Conversion utilities.
nonstd::optional<OptionID> StringToOptionId(const wxString& input);
wxString FilterToString(int value);
wxString InterframeToString(int value);
wxString RenderMethodToString(int value);
wxString AudioApiToString(int value);
wxString SoundQualityToString(int value);
int StringToFilter(const wxString& config_name, const wxString& input);
int StringToInterframe(const wxString& config_name, const wxString& input);
int StringToRenderMethod(const wxString& config_name, const wxString& input);
int StringToAudioApi(const wxString& config_name, const wxString& input);
int StringToSoundQuality(const wxString& config_name, const wxString& input);

wxString AllEnumValuesForType(Option::Type type);

// Max value for enum types.
int MaxForType(Option::Type type);

}  // namespace internal
}  // namespace config
