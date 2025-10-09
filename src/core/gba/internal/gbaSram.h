#ifndef VBAM_CORE_GBA_INTERNAL_GBASRAM_H_
#define VBAM_CORE_GBA_INTERNAL_GBASRAM_H_

#include <cstdint>

uint8_t sramRead(uint32_t address);
void sramWrite(uint32_t address, uint8_t byte);
void sramDelayedWrite(uint32_t address, uint8_t byte);

#endif  // VBAM_CORE_GBA_INTERNAL_GBASRAM_H_
