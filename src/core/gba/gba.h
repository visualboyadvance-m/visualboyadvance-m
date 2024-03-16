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
#define SAVE_GAME_VERSION SAVE_GAME_VERSION_10

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

// Updates romSize and realloc rom pointer if needed after soft-patching
void gbaUpdateRomSize(int size);

extern struct EmulatedSystem GBASystem;

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

#endif  // VBAM_CORE_GBA_GBA_H_
