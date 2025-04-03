#ifndef VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_
#define VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_

#include <cstdint>

void gbafilter_update_colors(bool lcd = false);
void gbafilter_pal(uint16_t* buf, int count);
void gbafilter_pal32(uint32_t* buf, int count);
void gbafilter_pad(uint8_t* buf, int count);

#endif  // VBAM_COMPONENTS_FILTERS_AGB_FILTERS_AGB_H_
