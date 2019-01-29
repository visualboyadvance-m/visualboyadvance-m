#include "Sram.h"
#include "Flash.h"
#include "GBA.h"
#include "Globals.h"

uint8_t sramRead(uint32_t address)
{
    return flashSaveMemory[address & 0xFFFF];
}
void sramDelayedWrite(uint32_t address, uint8_t byte)
{
    saveType = GBA_SAVE_SRAM;
    cpuSaveGameFunc = sramWrite;
    sramWrite(address, byte);
}

void sramWrite(uint32_t address, uint8_t byte)
{
    flashSaveMemory[address & 0xFFFF] = byte;
    systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
}
