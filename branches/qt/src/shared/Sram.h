#ifndef SRAM_H
#define SRAM_H

#include "System.h"

u8 sramRead(u32 address);
void sramWrite(u32 address, u8 byte);
void sramDelayedWrite(u32 address, u8 byte);

#endif
