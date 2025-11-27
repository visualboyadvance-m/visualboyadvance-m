#ifndef VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_
#define VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_

#include <cstdint>

void gbafilter_update_colors(bool lcd = false);
void gbafilter_pal8(uint8_t* buf, int count);
void gbafilter_pal(uint16_t* buf, int count);
void gbafilter_pal32(uint32_t* buf, int count);
void gbafilter_set_params(int color_mode, float darken_screen);

void gbafilter_update_colors_native(bool lcd = false);
void gbafilter_pal_565(uint16_t* buf, int count);
void gbafilter_pal_888(uint32_t* buf, int count);

#endif  // VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_
