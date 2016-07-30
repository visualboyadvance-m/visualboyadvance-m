#include "GBA.h"

#ifdef BKPT_SUPPORT
int oldreg[18];
char oldbuffer[10];
#endif

reg_pair reg[45];
memoryMap map[256];
bool ioReadable[0x400];
bool N_FLAG = 0;
bool C_FLAG = 0;
bool Z_FLAG = 0;
bool V_FLAG = 0;
bool armState = true;
bool armIrqEnable = true;
uint32_t armNextPC = 0x00000000;
int armMode = 0x1f;
uint32_t stop = 0x08000568;

// this is an optional hack to change the backdrop/background color:
// -1: disabled
// 0x0000 to 0x7FFF: set custom 15 bit color
int customBackdropColor = -1;

uint8_t* bios = 0;
uint8_t* rom = 0;
uint8_t* internalRAM = 0;
uint8_t* workRAM = 0;
uint8_t* paletteRAM = 0;
uint8_t* vram = 0;
uint8_t* pix = 0;
uint8_t* oam = 0;
uint8_t* ioMem = 0;

uint16_t DISPCNT = 0x0080;
uint16_t DISPSTAT = 0x0000;
uint16_t VCOUNT = 0x0000;
uint16_t BG0CNT = 0x0000;
uint16_t BG1CNT = 0x0000;
uint16_t BG2CNT = 0x0000;
uint16_t BG3CNT = 0x0000;
uint16_t BG0HOFS = 0x0000;
uint16_t BG0VOFS = 0x0000;
uint16_t BG1HOFS = 0x0000;
uint16_t BG1VOFS = 0x0000;
uint16_t BG2HOFS = 0x0000;
uint16_t BG2VOFS = 0x0000;
uint16_t BG3HOFS = 0x0000;
uint16_t BG3VOFS = 0x0000;
uint16_t BG2PA = 0x0100;
uint16_t BG2PB = 0x0000;
uint16_t BG2PC = 0x0000;
uint16_t BG2PD = 0x0100;
uint16_t BG2X_L = 0x0000;
uint16_t BG2X_H = 0x0000;
uint16_t BG2Y_L = 0x0000;
uint16_t BG2Y_H = 0x0000;
uint16_t BG3PA = 0x0100;
uint16_t BG3PB = 0x0000;
uint16_t BG3PC = 0x0000;
uint16_t BG3PD = 0x0100;
uint16_t BG3X_L = 0x0000;
uint16_t BG3X_H = 0x0000;
uint16_t BG3Y_L = 0x0000;
uint16_t BG3Y_H = 0x0000;
uint16_t WIN0H = 0x0000;
uint16_t WIN1H = 0x0000;
uint16_t WIN0V = 0x0000;
uint16_t WIN1V = 0x0000;
uint16_t WININ = 0x0000;
uint16_t WINOUT = 0x0000;
uint16_t MOSAIC = 0x0000;
uint16_t BLDMOD = 0x0000;
uint16_t COLEV = 0x0000;
uint16_t COLY = 0x0000;
uint16_t DM0SAD_L = 0x0000;
uint16_t DM0SAD_H = 0x0000;
uint16_t DM0DAD_L = 0x0000;
uint16_t DM0DAD_H = 0x0000;
uint16_t DM0CNT_L = 0x0000;
uint16_t DM0CNT_H = 0x0000;
uint16_t DM1SAD_L = 0x0000;
uint16_t DM1SAD_H = 0x0000;
uint16_t DM1DAD_L = 0x0000;
uint16_t DM1DAD_H = 0x0000;
uint16_t DM1CNT_L = 0x0000;
uint16_t DM1CNT_H = 0x0000;
uint16_t DM2SAD_L = 0x0000;
uint16_t DM2SAD_H = 0x0000;
uint16_t DM2DAD_L = 0x0000;
uint16_t DM2DAD_H = 0x0000;
uint16_t DM2CNT_L = 0x0000;
uint16_t DM2CNT_H = 0x0000;
uint16_t DM3SAD_L = 0x0000;
uint16_t DM3SAD_H = 0x0000;
uint16_t DM3DAD_L = 0x0000;
uint16_t DM3DAD_H = 0x0000;
uint16_t DM3CNT_L = 0x0000;
uint16_t DM3CNT_H = 0x0000;
uint16_t TM0D = 0x0000;
uint16_t TM0CNT = 0x0000;
uint16_t TM1D = 0x0000;
uint16_t TM1CNT = 0x0000;
uint16_t TM2D = 0x0000;
uint16_t TM2CNT = 0x0000;
uint16_t TM3D = 0x0000;
uint16_t TM3CNT = 0x0000;
uint16_t P1 = 0xFFFF;
uint16_t IE = 0x0000;
uint16_t IF = 0x0000;
uint16_t IME = 0x0000;
