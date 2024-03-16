#ifndef VBAM_GBA_GBAFILTER_H_
#define VBAM_GBA_GBAFILTER_H_

#include <cstdint>

void gbafilter_pal(uint16_t* buf, int count);
void gbafilter_pal32(uint32_t* buf, int count);
void gbafilter_pad(uint8_t* buf, int count);

#endif  // VBAM_GBA_GBAFILTER_H_