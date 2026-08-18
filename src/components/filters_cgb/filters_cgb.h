#ifndef VBAM_COMPONENTS_FILTERS_CGB_FILTERS_CGB_H_
#define VBAM_COMPONENTS_FILTERS_CGB_FILTERS_CGB_H_

#include <cstdint>

// LCD panel variants. The order is stored in the config, so only append.
enum GbcFilterVariant {
    kGbcFilterCgb = 0,  // gbc-color
    kGbcFilterNso,      // NSO-gbc-color
    kGbcFilterVariantCount
};

void gbcfilter_update_colors(bool lcd = false);
void gbcfilter_pal8(uint8_t* buf, int count);
void gbcfilter_pal(uint16_t* buf, int count);
void gbcfilter_pal32(uint32_t* buf, int count);
void gbcfilter_set_params(int color_mode, float lighten_screen,
                          int variant = kGbcFilterCgb);

// False for NSO GBC, whose shader has no lighten control, so the dialog can
// grey the slider out. The profile table holds lighten_scale at 0 for it.
bool gbcfilter_variant_has_lighten(int variant);

void gbcfilter_update_colors_native(bool lcd = false);
void gbcfilter_pal_565(uint16_t* buf, int count);
void gbcfilter_pal_888(uint32_t* buf, int count);

#endif  // VBAM_COMPONENTS_FILTERS_CGB_FILTERS_CGB_H_
