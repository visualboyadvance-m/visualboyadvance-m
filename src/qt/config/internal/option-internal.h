#ifndef VBAM_OPTION_INTERNAL_INCLUDE
#error "Do not include "config/internal/option-internal.h" outside of the implementation."
#endif

#include <array>

#include <optional.hpp>

#include <QString>

#include "qt/config/option.h"

namespace config {
namespace internal {

struct OptionData {
    const QString config_name;
    const QString command;
    // Untranslated helper text; translated with QCoreApplication::translate
    // ("vbam", ...) when the Option is constructed.
    const char* ux_helper;
};

// Static data to initialize global values.
extern const std::array<OptionData, kNbOptions + 1> kAllOptionsData;

// Conversion utilities.
nonstd::optional<OptionID> StringToOptionId(const QString& input);
QString FilterToString(const Filter& value);
QString InterframeToString(const Interframe& value);
QString RenderMethodToString(const RenderMethod& value);
QString ColorCorrectionProfileToString(const ColorCorrectionProfile& value);
QString AudioApiToString(const AudioApi& value);
QString AudioRateToString(const AudioRate& value);
Filter StringToFilter(const QString& config_name, const QString& input);
Interframe StringToInterframe(const QString& config_name, const QString& input);
RenderMethod StringToRenderMethod(const QString& config_name, const QString& input);
ColorCorrectionProfile StringToColorCorrectionProfile(const QString& config_name, const QString& input);
AudioApi StringToAudioApi(const QString& config_name, const QString& input);
AudioRate StringToSoundQuality(const QString& config_name, const QString& input);

QString AllEnumValuesForType(Option::Type type);

// Max value for enum types.
size_t MaxForType(Option::Type type);

}  // namespace internal
}  // namespace config
