#ifndef VBAM_CORE_GBA_GBAFLASH_H_
#define VBAM_CORE_GBA_GBAFLASH_H_

#include <cstdint>

#if !defined(__LIBRETRO__)
#include <zlib.h>
#endif  // defined(__LIBRETRO__)

#define FLASH_128K_SZ 0x20000

void flashDetectSaveType(const int size);

#if defined(__LIBRETRO__)
extern void flashSaveGame(uint8_t*& data);
extern void flashReadGame(const uint8_t*& data);
#else  // !defined(__LIBRETRO__)
extern void flashSaveGame(gzFile _gzFile);
extern void flashReadGame(gzFile _gzFile, int version);
extern void flashReadGameSkip(gzFile _gzFile, int version);
#endif  // defined(__LIBRETRO__)
extern uint8_t flashSaveMemory[FLASH_128K_SZ];
extern uint8_t flashRead(uint32_t address);
extern void flashWrite(uint32_t address, uint8_t byte);
extern void flashDelayedWrite(uint32_t address, uint8_t byte);
extern void flashSaveDecide(uint32_t address, uint8_t byte);
extern void flashReset();
extern void flashSetSize(int size);
extern void flashInit();

extern int g_flashSize;

#endif // VBAM_CORE_GBA_GBAFLASH_H_
