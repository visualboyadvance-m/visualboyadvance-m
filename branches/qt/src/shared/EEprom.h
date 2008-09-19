#ifndef VBA_EEPROM_H
#define VBA_EEPROM_H

extern void eepromSaveGame(gzFile _gzFile);
extern void eepromReadGame(gzFile _gzFile, int version);
extern void eepromReadGameSkip(gzFile _gzFile, int version);
extern int eepromRead(u32 address);
extern void eepromWrite(u32 address, u8 value);
extern void eepromInit();
extern void eepromReset();
extern u8 eepromData[0x2000];
extern bool eepromInUse;
extern int eepromSize;

#define EEPROM_IDLE           0
#define EEPROM_READADDRESS    1
#define EEPROM_READDATA       2
#define EEPROM_READDATA2      3
#define EEPROM_WRITEDATA      4

#endif // VBA_EEPROM_H
