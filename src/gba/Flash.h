#ifndef FLASH_H
#define FLASH_H

#ifndef __LIBRETRO__
#include "../common/cstdint.h"
#else
#include <stdint.h>
#endif

#include <zlib.h>

#define FLASH_128K_SZ 0x20000

#ifdef __LIBRETRO__
extern void flashSaveGame(uint8_t*& data);
extern void flashReadGame(const uint8_t*& data, int);
#else
extern void flashSaveGame(gzFile _gzFile);
extern void flashReadGame(gzFile _gzFile, int version);
extern void flashReadGameSkip(gzFile _gzFile, int version);
#endif
extern uint8_t flashRead(uint32_t address);
extern void flashWrite(uint32_t address, uint8_t byte);
extern void flashDelayedWrite(uint32_t address, uint8_t byte);
#ifdef __LIBRETRO__
extern uint8_t* flashSaveMemory;
#else
extern uint8_t flashSaveMemory[FLASH_128K_SZ];
#endif
extern void flashSaveDecide(uint32_t address, uint8_t byte);
extern void flashReset();
extern void flashSetSize(int size);
extern void flashInit();

extern int flashSize;

#endif // FLASH_H
