#ifndef SRAM_H
#define SRAM_H

#include <cstdint>

uint8_t sramRead(uint32_t address);
void sramWrite(uint32_t address, uint8_t byte);
void sramDelayedWrite(uint32_t address, uint8_t byte);

#endif // SRAM_H
