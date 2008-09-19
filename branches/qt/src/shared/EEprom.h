#ifndef EEPROM_H
#define EEPROM_H

void eepromSaveGame(gzFile _gzFile);
void eepromReadGame(gzFile _gzFile, int version);
void eepromReadGameSkip(gzFile _gzFile, int version);
int eepromRead(u32 address);
void eepromWrite(u32 address, u8 value);
void eepromInit();
void eepromReset();

extern u8 eepromData[0x2000];
extern bool eepromInUse;
extern int eepromSize;

#define EEPROM_IDLE           0
#define EEPROM_READADDRESS    1
#define EEPROM_READDATA       2
#define EEPROM_READDATA2      3
#define EEPROM_WRITEDATA      4

#endif
