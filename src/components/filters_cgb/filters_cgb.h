#ifndef VBAM_COMPONENTS_FILTERS_CGB_FILTERS_CGB_H_
#define VBAM_COMPONENTS_FILTERS_CGB_FILTERS_CGB_H_

#include <cstdint>

void gbcfilter_update_colors(bool lcd = false);
void gbcfilter_pal(uint16_t* buf, int count);
void gbcfilter_pal32(uint32_t* buf, int count);
void gbcfilter_pad(uint8_t* buf, int count);
void gbcfilter_set_params(int color_mode, float lighten_screen);

#endif  // VBAM_COMPONENTS_FILTERS_CGB_FILTERS_CGB_H_
