#ifndef VBAM_CORE_GBA_GBAEEPROM_H_
#define VBAM_CORE_GBA_GBAEEPROM_H_

#include <cstdint>

#if !defined(__LIBRETRO__)
#include <zlib.h>
#endif  // defined(__LIBRETRO__)

#if defined(__LIBRETRO__)
extern void eepromSaveGame(uint8_t*& data);
extern void eepromReadGame(const uint8_t*& data);
#else // !defined(__LIBRETRO__)
extern void eepromSaveGame(gzFile _gzFile);
extern void eepromReadGame(gzFile _gzFile, int version);
extern void eepromReadGameSkip(gzFile _gzFile, int version);
#endif  // defined(__LIBRETRO__)
extern uint8_t eepromData[0x2000];
extern int eepromRead(uint32_t address);
extern void eepromWrite(uint32_t address, uint8_t value);
extern void eepromInit();
extern void eepromReset();
extern bool eepromInUse;
extern int eepromSize;

#define EEPROM_IDLE 0
#define EEPROM_READADDRESS 1
#define EEPROM_READDATA 2
#define EEPROM_READDATA2 3
#define EEPROM_WRITEDATA 4

#endif  // VBAM_CORE_GBA_GBAEEPROM_H_
