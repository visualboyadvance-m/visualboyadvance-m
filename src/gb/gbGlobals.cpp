#include <cstddef>
#include "../common/Types.h"

uint8_t* gbMemoryMap[16];

int gbRomSizeMask = 0;
int gbRomSize = 0;
int gbRamSizeMask = 0;
int gbRamSize = 0;

uint8_t* gbMemory = nullptr;
uint8_t* gbVram = nullptr;
uint8_t* gbRom = nullptr;
uint8_t* gbRam = nullptr;
uint8_t* gbWram = nullptr;
uint16_t* gbLineBuffer = nullptr;
uint8_t* gbTAMA5ram = nullptr;

uint16_t gbPalette[128];
uint8_t gbBgp[4] = { 0, 1, 2, 3 };
uint8_t gbObp0[4] = { 0, 1, 2, 3 };
uint8_t gbObp1[4] = { 0, 1, 2, 3 };
int gbWindowLine = -1;

bool genericflashcardEnable = false;
int gbCgbMode = 0;

uint16_t gbColorFilter[32768];
uint32_t gbEmulatorType = 0;
uint32_t gbPaletteOption = 0;
int gbBorderLineSkip = 160;
int gbBorderRowSkip = 0;
int gbBorderColumnSkip = 0;
int gbDmaTicks = 0;
bool gbBorderAutomatic = false;
bool gbBorderOn = false;
bool gbColorOption = false;

uint8_t (*gbSerialFunction)(uint8_t) = NULL;
