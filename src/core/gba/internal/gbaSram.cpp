#include "core/gba/internal/gbaSram.h"

#include "core/gba/gbaFlash.h"
#include "core/gba/gba.h"

uint8_t sramRead(uint32_t address)
{
    return flashSaveMemory[address & 0xFFFF];
}
void sramDelayedWrite(uint32_t address, uint8_t b)
{
    coreOptions.saveType = GBA_SAVE_SRAM;
    cpuSaveGameFunc = sramWrite;
    sramWrite(address, b);
}

void sramWrite(uint32_t address, uint8_t b)
{
    flashSaveMemory[address & 0xFFFF] = b;
    systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
}
