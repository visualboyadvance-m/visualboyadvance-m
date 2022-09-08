#ifndef VBAM_OPTIONS_INTERNAL_INCLUDE
#error "Do not include "vbam-options-internal.h" outside of the implementation."
#endif

#include <array>
#include <string>
#include <wx/string.h>

#include "nonstd/optional.hpp"
#include "vbam-options.h"

namespace internal {

struct VbamOptionData {
    const wxString config_name;
    const wxString command;
    const wxString ux_helper;
    const VbamOption::Type type;
};

// Static data to initialize global values.
extern const std::array<VbamOptionData, kNbOptions + 1> kAllOptionsData;

// Conversion utilities.
nonstd::optional<VbamOptionID> StringToOptionId(const wxString& input);
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

wxString AllEnumValuesForType(VbamOption::Type type);

// Max value for enum types.
int MaxForType(VbamOption::Type type);

}  // namespace internal
