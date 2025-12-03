#include "core/gb/gbGlobals.h"
#include <stdlib.h>

uint8_t* gbMemoryMap[16];

uint8_t* gbMemory = NULL;
uint8_t* gbVram = NULL;
uint8_t* gbRom = NULL;
uint8_t* gbRam = NULL;
uint8_t* gbWram = NULL;
uint16_t* gbLineBuffer = NULL;
uint8_t* gbTAMA5ram = NULL;

uint16_t gbPalette[128];
uint8_t gbBgp[4] = { 0, 1, 2, 3 };
uint8_t gbObp0[4] = { 0, 1, 2, 3 };
uint8_t gbObp1[4] = { 0, 1, 2, 3 };
int gbWindowLine = -1;

uint32_t gbEmulatorType = 0;
uint32_t gbPaletteOption = 0;
int gbBorderLineSkip = 160;
int gbBorderRowSkip = 0;
int gbBorderColumnSkip = 0;
int gbDmaTicks = 0;
uint8_t gbCartBus = 0xff;
bool gbBorderAutomatic = false;
bool gbBorderOn = false;
bool gbCgbMode = false;
bool gbSgbMode = false;

uint8_t (*gbSerialFunction)(uint8_t) = NULL;
