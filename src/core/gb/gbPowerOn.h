#ifndef VBAM_CORE_GB_GBPOWERON_H_
#define VBAM_CORE_GB_GBPOWERON_H_

#include <cstddef>
#include <cstdint>

// Accurate power-on / post-boot RAM contents (see gbPowerOn.cpp).
void gbPowerOnWramDmg(uint8_t* wram);
void gbPowerOnWramCgb(uint8_t* wram);
void gbPowerOnVram(uint8_t* vram, bool cgb, size_t vram_size);

#endif  // VBAM_CORE_GB_GBPOWERON_H_
