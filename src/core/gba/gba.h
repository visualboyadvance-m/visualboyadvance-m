#ifndef VBAM_CORE_GBA_GBA_H_
#define VBAM_CORE_GBA_GBA_H_

#include <cstdint>

#include "core/base/system.h"

const uint64_t TICKS_PER_SECOND = 16777216;

#define SAVE_GAME_VERSION_1 1
#define SAVE_GAME_VERSION_2 2
#define SAVE_GAME_VERSION_3 3
#define SAVE_GAME_VERSION_4 4
#define SAVE_GAME_VERSION_5 5
#define SAVE_GAME_VERSION_6 6
#define SAVE_GAME_VERSION_7 7
#define SAVE_GAME_VERSION_8 8
#define SAVE_GAME_VERSION_9 9
#define SAVE_GAME_VERSION_10 10
#define SAVE_GAME_VERSION_11 11
#define SAVE_GAME_VERSION SAVE_GAME_VERSION_11

#define gbaWidth  240
#define gbaHeight 160

enum {
    GBA_SAVE_AUTO = 0,
    GBA_SAVE_EEPROM,
    GBA_SAVE_SRAM,
    GBA_SAVE_FLASH,
    GBA_SAVE_EEPROM_SENSOR,
    GBA_SAVE_NONE
};

enum {
    SIZE_SRAM      = 32768,
    SIZE_FLASH512   = 65536,
    SIZE_FLASH1M   = 131072,
    SIZE_EEPROM_512 = 512,
    SIZE_EEPROM_8K = 8192
};

enum {
    SIZE_ROM   = 0x2000000,
    SIZE_BIOS  = 0x0004000,
    SIZE_IRAM  = 0x0008000,
    SIZE_WRAM  = 0x0040000,
    SIZE_PRAM  = 0x0000400,
    SIZE_VRAM  = 0x0020000,
    SIZE_OAM   = 0x0000400,
    SIZE_IOMEM = 0x0000400,
#ifndef __LIBRETRO__
    SIZE_PIX   = (4 * 241 * 162)
#else
    SIZE_PIX   = (4 * 240 * 160)
#endif
};

typedef struct {
    uint8_t* address;
    uint32_t mask;
#ifdef VBAM_ENABLE_DEBUGGER
    uint8_t* breakPoints;
    uint8_t* searchMatch;
    uint8_t* trace;
    uint32_t size;
#endif
} memoryMap;

typedef union {
    struct {
#ifdef WORDS_BIGENDIAN
        uint8_t B3;
        uint8_t B2;
        uint8_t B1;
        uint8_t B0;
#else
        uint8_t B0;
        uint8_t B1;
        uint8_t B2;
        uint8_t B3;
#endif
    } B;
    struct {
#ifdef WORDS_BIGENDIAN
        uint16_t W1;
        uint16_t W0;
#else
        uint16_t W0;
        uint16_t W1;
#endif
    } W;
#ifdef WORDS_BIGENDIAN
    volatile uint32_t I;
#else
    uint32_t I;
#endif
} reg_pair;

#ifndef NO_GBA_MAP
extern memoryMap map[256];
#endif

extern uint8_t biosProtected[4];

extern void (*cpuSaveGameFunc)(uint32_t, uint8_t);

extern bool cpuSramEnabled;
extern bool cpuFlashEnabled;
extern bool cpuEEPROMEnabled;
extern bool cpuEEPROMSensorEnabled;
extern bool debugger;

#ifdef VBAM_ENABLE_DEBUGGER
extern uint8_t freezeWorkRAM[0x40000];
extern uint8_t freezeInternalRAM[0x8000];
extern uint8_t freezeVRAM[0x18000];
extern uint8_t freezeOAM[0x400];
extern uint8_t freezePRAM[0x400];
extern bool debugger_last;
extern int oldreg[18];
extern char oldbuffer[10];
#endif

extern bool CPUReadGSASnapshot(const char*);
extern bool CPUReadGSASPSnapshot(const char*);
extern bool CPUWriteGSASnapshot(const char*, const char*, const char*, const char*);
extern bool CPUWriteBatteryFile(const char*);
extern bool CPUReadBatteryFile(const char*);
extern bool CPUExportEepromFile(const char*);
extern bool CPUImportEepromFile(const char*);
extern bool CPUWritePNGFile(const char*);
extern bool CPUWriteBMPFile(const char*);
extern void CPUCleanUp();
extern void CPUUpdateRender();
extern void CPUUpdateRenderBuffers(bool);
extern bool CPUReadMemState(char*, int);
extern bool CPUWriteMemState(char*, int);
#ifdef __LIBRETRO__
extern bool CPUReadState(const uint8_t*);
extern unsigned int CPUWriteState(uint8_t* data, unsigned int size);
#else
extern bool CPUReadState(const char*);
extern bool CPUWriteState(const char*);
#endif
extern int CPULoadRom(const char*);
extern int CPULoadRomData(const char* data, int size);
extern void doMirroring(bool);
extern void CPUUpdateRegister(uint32_t, uint16_t);
extern void applyTimer();
extern void CPUInit(const char*, bool);
void SetSaveType(int st);
extern void CPUReset();
extern void CPULoop(int);
extern void CPUCheckDMA(int, int);
extern bool CPUIsGBAImage(const char*);
extern bool CPUIsZipFile(const char*);
#ifdef PROFILING
#include "prof/prof.h"
extern void cpuProfil(profile_segment* seg);
extern void cpuEnableProfiling(int hz);
#endif

const char* GetLoadDotCodeFile();
const char* GetSaveDotCodeFile();
void ResetLoadDotCodeFile();
void SetLoadDotCodeFile(const char* szFile);
void ResetSaveDotCodeFile();
void SetSaveDotCodeFile(const char* szFile);

// Update the stored ROM size after applying a soft patch.
// If the new size is larger than the current buffer, reallocate the ROM buffer.
void gbaUpdateRomSize(int size);

// Retrieve the current ROM size in bytes.
// NOTE: size will not be always be power-of-twos, dont use for mask calculation
size_t gbaGetRomSize();

extern struct EmulatedSystem GBASystem;

enum MemoryRegion : uint8_t {
    REGION_BIOS     = 0x00,
    REGION_UNUSED   = 0x01,
    REGION_EWRAM    = 0x02,
    REGION_IWRAM    = 0x03,
    REGION_IO       = 0x04,
    REGION_PRAM     = 0x05,
    REGION_VRAM     = 0x06,
    REGION_OAM      = 0x07,
    REGION_ROM0     = 0x08,
    REGION_ROM0EX   = 0x09,
    REGION_ROM1     = 0x0A,
    REGION_ROM1EX   = 0x0B,
    REGION_ROM2     = 0x0C,
    REGION_ROM2EX   = 0x0D,
    REGION_SRAM     = 0x0E,
    REGION_SRAMEX   = 0x0F
};

#define R13_IRQ 18
#define R14_IRQ 19
#define SPSR_IRQ 20
#define R13_USR 26
#define R14_USR 27
#define R13_SVC 28
#define R14_SVC 29
#define SPSR_SVC 30
#define R13_ABT 31
#define R14_ABT 32
#define SPSR_ABT 33
#define R13_UND 34
#define R14_UND 35
#define SPSR_UND 36
#define R8_FIQ 37
#define R9_FIQ 38
#define R10_FIQ 39
#define R11_FIQ 40
#define R12_FIQ 41
#define R13_FIQ 42
#define R14_FIQ 43
#define SPSR_FIQ 44

// register definitions
#define COMM_SIODATA32_L 0x120 // Lower 16bit on Normal mode
#define COMM_SIODATA32_H 0x122 // Higher 16bit on Normal mode
#define COMM_SIOCNT 0x128
#define COMM_SIODATA8 0x12a // 8bit on Normal/UART mode, (up to 4x8bit with FIFO)
#define COMM_SIOMLT_SEND 0x12a // SIOMLT_SEND (16bit R/W) on MultiPlayer mode (local outgoing)
#define COMM_SIOMULTI0 0x120 // SIOMULTI0 (16bit) on MultiPlayer mode (Parent/Master)
#define COMM_SIOMULTI1 0x122 // SIOMULTI1 (16bit) on MultiPlayer mode (Child1/Slave1)
#define COMM_SIOMULTI2 0x124 // SIOMULTI2 (16bit) on MultiPlayer mode (Child2/Slave2)
#define COMM_SIOMULTI3 0x126 // SIOMULTI3 (16bit) on MultiPlayer mode (Child3/Slave3)
#define COMM_RCNT 0x134 // SIO Mode (4bit data) on GeneralPurpose mode
#define COMM_IR 0x136 // Infrared Register (16bit) 1bit data at a time(LED On/Off)?
#define COMM_JOYCNT 0x140
#define COMM_JOY_RECV_L 0x150 // Send/Receive 8bit Lower first then 8bit Higher
#define COMM_JOY_RECV_H 0x152
#define COMM_JOY_TRANS_L 0x154 // Send/Receive 8bit Lower first then 8bit Higher
#define COMM_JOY_TRANS_H 0x156
#define COMM_JOYSTAT 0x158 // Send/Receive 8bit lower only

#define JOYSTAT_RECV 2
#define JOYSTAT_SEND 8

/*
** An enumeration of all IO registers.
*/
enum ioRegs {
    IO_REG_START        = 0x0000,

    /* Video */

    IO_REG_DISPCNT      = 0x0000,
    IO_REG_GREENSWP     = 0x0002,
    IO_REG_DISPSTAT     = 0x0004,
    IO_REG_VCOUNT       = 0x0006,
    IO_REG_BG0CNT       = 0x0008,
    IO_REG_BG1CNT       = 0x000A,
    IO_REG_BG2CNT       = 0x000C,
    IO_REG_BG3CNT       = 0x000E,
    IO_REG_BG0HOFS      = 0x0010,
    IO_REG_BG0VOFS      = 0x0012,
    IO_REG_BG1HOFS      = 0x0014,
    IO_REG_BG1VOFS      = 0x0016,
    IO_REG_BG2HOFS      = 0x0018,
    IO_REG_BG2VOFS      = 0x001A,
    IO_REG_BG3HOFS      = 0x001C,
    IO_REG_BG3VOFS      = 0x001E,

    IO_REG_BG2PA        = 0x0020,
    IO_REG_BG2PB        = 0x0022,
    IO_REG_BG2PC        = 0x0024,
    IO_REG_BG2PD        = 0x0026,
    IO_REG_BG2X         = 0x0028,
    IO_REG_BG2X_L       = 0x0028,
    IO_REG_BG2X_H       = 0x002A,
    IO_REG_BG2Y         = 0x002C,
    IO_REG_BG2Y_L       = 0x002C,
    IO_REG_BG2Y_H       = 0x002E,
    IO_REG_BG3PA        = 0x0030,
    IO_REG_BG3PB        = 0x0032,
    IO_REG_BG3PC        = 0x0034,
    IO_REG_BG3PD        = 0x0036,
    IO_REG_BG3X         = 0x0038,
    IO_REG_BG3X_L       = 0x0038,
    IO_REG_BG3X_H       = 0x003A,
    IO_REG_BG3Y         = 0x003C,
    IO_REG_BG3Y_L       = 0x003C,
    IO_REG_BG3Y_H       = 0x003E,

    IO_REG_WIN0H        = 0x0040,
    IO_REG_WIN1H        = 0x0042,
    IO_REG_WIN0V        = 0x0044,
    IO_REG_WIN1V        = 0x0046,
    IO_REG_WININ        = 0x0048,
    IO_REG_WINOUT       = 0x004A,
    IO_REG_MOSAIC       = 0x004C,
    IO_REG_BLDCNT       = 0x0050, // BLDMOD
    IO_REG_BLDALPHA     = 0x0052, // COLEV
    IO_REG_BLDY         = 0x0054, // COLY

    /* Sound */

    IO_REG_SOUND1CNT_L  = 0x0060,
    IO_REG_SOUND1CNT_H  = 0x0062,
    IO_REG_SOUND1CNT_X  = 0x0064,
    IO_REG_SOUND2CNT_L  = 0x0068,
    IO_REG_SOUND2CNT_H  = 0x006C,
    IO_REG_SOUND3CNT_L  = 0x0070,
    IO_REG_SOUND3CNT_H  = 0x0072,
    IO_REG_SOUND3CNT_X  = 0x0074,
    IO_REG_SOUND4CNT_L  = 0x0078,
    IO_REG_SOUND4CNT_H  = 0x007C,
    IO_REG_SOUNDCNT_L   = 0x0080,
    IO_REG_SOUNDCNT_H   = 0x0082,
    IO_REG_SOUNDCNT_X   = 0x0084,
    IO_REG_SOUNDBIAS    = 0x0088,
    IO_REG_WAVE_RAM0_L  = 0x0090,
    IO_REG_WAVE_RAM0_H  = 0x0092,
    IO_REG_WAVE_RAM1_L  = 0x0094,
    IO_REG_WAVE_RAM1_H  = 0x0096,
    IO_REG_WAVE_RAM2_L  = 0x0098,
    IO_REG_WAVE_RAM2_H  = 0x009A,
    IO_REG_WAVE_RAM3_L  = 0x009C,
    IO_REG_WAVE_RAM3_H  = 0x009E,
    IO_REG_FIFO_A_L     = 0x00A0,
    IO_REG_FIFO_A_H     = 0x00A2,
    IO_REG_FIFO_B_L     = 0x00A4,
    IO_REG_FIFO_B_H     = 0x00A6,

    /* DMA Transfer Channels */

    IO_REG_DMA0SAD      = 0x00B0,
    IO_REG_DMA0SAD_L    = 0x00B0,
    IO_REG_DMA0SAD_H    = 0x00B2,
    IO_REG_DMA0DAD      = 0x00B4,
    IO_REG_DMA0DAD_L    = 0x00B4,
    IO_REG_DMA0DAD_H    = 0x00B6,
    IO_REG_DMA0CNT      = 0x00B8,
    IO_REG_DMA0CTL      = 0x00BA,

    IO_REG_DMA1SAD      = 0x00BC,
    IO_REG_DMA1SAD_L    = 0x00BC,
    IO_REG_DMA1SAD_H    = 0x00BE,
    IO_REG_DMA1DAD      = 0x00C0,
    IO_REG_DMA1DAD_L    = 0x00C0,
    IO_REG_DMA1DAD_H    = 0x00C2,
    IO_REG_DMA1CNT      = 0x00C4,
    IO_REG_DMA1CTL      = 0x00C6,

    IO_REG_DMA2SAD      = 0x00C8,
    IO_REG_DMA2SAD_L    = 0x00C8,
    IO_REG_DMA2SAD_H    = 0x00CA,
    IO_REG_DMA2DAD      = 0x00CC,
    IO_REG_DMA2DAD_L    = 0x00CC,
    IO_REG_DMA2DAD_H    = 0x00CE,
    IO_REG_DMA2CNT      = 0x00D0,
    IO_REG_DMA2CTL      = 0x00D2,

    IO_REG_DMA3SAD      = 0x00D4,
    IO_REG_DMA3SAD_L    = 0x00D4,
    IO_REG_DMA3SAD_H    = 0x00D6,
    IO_REG_DMA3DAD      = 0x00D8,
    IO_REG_DMA3DAD_L    = 0x00D8,
    IO_REG_DMA3DAD_H    = 0x00DA,
    IO_REG_DMA3CNT      = 0x00DC,
    IO_REG_DMA3CTL      = 0x00DE,

    /* Timer */
    IO_REG_TM0CNT       = 0x0100,
    IO_REG_TM0CNT_L     = 0x0100,
    IO_REG_TM0CNT_H     = 0x0102,
    IO_REG_TM1CNT       = 0x0104,
    IO_REG_TM1CNT_L     = 0x0104,
    IO_REG_TM1CNT_H     = 0x0106,
    IO_REG_TM2CNT       = 0x0108,
    IO_REG_TM2CNT_L     = 0x0108,
    IO_REG_TM2CNT_H     = 0x010A,
    IO_REG_TM3CNT       = 0x010C,
    IO_REG_TM3CNT_L     = 0x010C,
    IO_REG_TM3CNT_H     = 0x010E,

    /* Input */
    IO_REG_KEYINPUT     = 0x0130,
    IO_REG_KEYCNT       = 0x0132,

    /* Serial Communication (2) */
    IO_REG_SIOCNT       = 0x0128,
    IO_REG_RCNT         = 0x0134,
    IO_REG_IR           = 0x0136,
    IO_REG_UNKNOWN_1    = 0x0142,
    IO_REG_UNKNOWN_2    = 0x015A,

    /* Interrupts */

    IO_REG_IE           = 0x0200,
    IO_REG_IF           = 0x0202,
    IO_REG_WAITCNT      = 0x0204,
    IO_REG_IME          = 0x0208,

    /* System */

    IO_REG_POSTFLG      = 0x0300,
    IO_REG_HALTCNT      = 0x0301,
    IO_REG_UNKNOWN_3    = 0x0302,

    IO_REG_END,
};

#endif  // VBAM_CORE_GBA_GBA_H_
