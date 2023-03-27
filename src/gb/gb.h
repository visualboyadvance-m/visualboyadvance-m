#ifndef GB_H
#define GB_H

#include <cstdint>

#include "gbCartData.h"

#define gbWidth   160
#define gbHeight  144
#define sgbWidth  256
#define sgbHeight 224

const int GB_C_FLAG = 0x10;
const int GB_H_FLAG = 0x20;
const int GB_N_FLAG = 0x40;
const int GB_Z_FLAG = 0x80;

typedef union {
    struct {
#ifdef WORDS_BIGENDIAN
        uint8_t B1, B0;
#else
        uint8_t B0, B1;
#endif
    } B;
    uint16_t W;
} gbRegister;

extern gbRegister AF, BC, DE, HL, SP, PC;
extern uint16_t IFF;
int gbDis(char*, uint16_t);

// Attempts to load the ROM file at `filename`. Returns true on success.
bool gbLoadRom(const char* filename);
// Attempts to load the ROM at `romData`, with a size of `romSize`. This will
// make a copy of `romData`. Returns true on success.
bool gbLoadRomData(const char* romData, size_t romSize);

#ifndef __LIBRETRO__
// Attempts to apply `patchName` to the currently loaded ROM. Returns true on
// success.
bool gbApplyPatch(const char* patchName);
#endif  // __LIBRETRO__

void gbEmulate(int);
void gbWriteMemory(uint16_t, uint8_t);
void gbDrawLine();
bool gbIsGameboyRom(const char*);
void gbGetHardwareType();

// Resets gbPalette to systemGbPalette and gbPaletteOption value. This is called
// in gbReset and only needs to be called when systemGbPalette or
// gbPaletteOption is updated while a GB game is running.
void gbResetPalette();

void gbReset();
void gbCleanUp();
void gbCPUInit(const char*, bool);
#ifdef __LIBRETRO__
unsigned int gbWriteSaveState(uint8_t*, unsigned);
bool gbReadSaveState(const uint8_t*, unsigned);
#else
bool gbWriteSaveState(const char*);
bool gbReadSaveState(const char*);
#endif
bool gbWriteBatteryFile(const char*);
bool gbWriteBatteryFile(const char*, bool);
bool gbReadBatteryFile(const char*);
bool gbWriteMemSaveState(char*, int, long&);
bool gbReadMemSaveState(char*, int);
void gbSgbRenderBorder();
bool gbWritePNGFile(const char*);
bool gbWriteBMPFile(const char*);
bool gbReadGSASnapshot(const char*);

// Allows invalid vram/palette access needed for Colorizer hacked games in GBC/GBA hardware
void setColorizerHack(bool value);
bool allowColorizerHack(void);

extern int gbHardware;
extern int gbRomType; // gets type from header 0x147
extern int gbBattery; // enabled when gbRamSize != 0
extern int gbRTCPresent;  // gbROM has RTC support

extern gbCartData g_gbCartData;
extern struct EmulatedSystem GBSystem;

#endif // GB_H
