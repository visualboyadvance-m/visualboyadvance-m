#ifndef VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_
#define VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_

#include <cstdint>

// LCD panel variants. The order is stored in the config, so only append.
enum GbaFilterVariant {
    kGbaFilterGba = 0,       // gba-color
    kGbaFilterGbaSpBacklit,  // sp101-color
    kGbaFilterMicro,         // gbMicro-color
    kGbaFilterDs,            // nds-color
    kGbaFilterDsLite,        // dslite-color
    kGbaFilterNso,           // NSO-gba-color
    kGbaFilterVariantCount
};

void gbafilter_update_colors(bool lcd = false);
void gbafilter_pal8(uint8_t* buf, int count);
void gbafilter_pal(uint16_t* buf, int count);
void gbafilter_pal32(uint32_t* buf, int count);
void gbafilter_set_params(int color_mode, float darken_screen,
                          int variant = kGbaFilterGba);

// False for the variants whose shader has no darken control, so the dialog can
// grey the slider out. The profile table holds darken_scale at 0 for those.
bool gbafilter_variant_has_darken(int variant);

void gbafilter_update_colors_native(bool lcd = false);
void gbafilter_pal_565(uint16_t* buf, int count);
void gbafilter_pal_888(uint32_t* buf, int count);

#endif  // VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_
