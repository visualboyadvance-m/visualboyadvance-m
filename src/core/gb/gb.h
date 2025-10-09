#ifndef VBAM_CORE_GB_GB_H_
#define VBAM_CORE_GB_GB_H_

#include <cstdint>

#include "core/gb/gbCartData.h"

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
bool gbIsGameboyRom(const char*);
void gbGetHardwareType();

// Resets gbPalette to systemGbPalette and gbPaletteOption value. This is called
// in gbReset and only needs to be called when systemGbPalette or
// gbPaletteOption is updated while a GB game is running.
void gbResetPalette();

void gbReset();
void gbCleanUp();
void gbCPUInit(const char*, bool);
void gbSgbRenderBorder();
bool gbReadGSASnapshot(const char*);

// Allows invalid vram/palette access needed for Colorizer hacked games in GBC/GBA hardware
void setColorizerHack(bool value);
bool allowColorizerHack(void);

extern int gbHardware;

extern gbCartData g_gbCartData;
extern struct EmulatedSystem GBSystem;

#endif // VBAM_CORE_GB_GB_H_
