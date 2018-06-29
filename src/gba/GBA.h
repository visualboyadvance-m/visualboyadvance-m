#ifndef GBA_H
#define GBA_H

#include "../common/Types.h"
#include "../System.h"

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

typedef struct {
    uint8_t* address;
    uint32_t mask;
#ifdef BKPT_SUPPORT
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

#ifdef BKPT_SUPPORT
extern uint8_t freezeWorkRAM[0x40000];
extern uint8_t freezeInternalRAM[0x8000];
extern uint8_t freezeVRAM[0x18000];
extern uint8_t freezeOAM[0x400];
extern uint8_t freezePRAM[0x400];
extern bool debugger_last;
extern int oldreg[18];
extern char oldbuffer[10];
extern bool debugger;
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
extern bool CPUReadState(const uint8_t*, unsigned);
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
void SetLoadDotCodeFile(const char* szFile);
void SetSaveDotCodeFile(const char* szFile);

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

#define WORK_RAM_SIZE 0x40000
#define ROM_SIZE      0x2000000

#include "Cheats.h"
#include "EEprom.h"
#include "Flash.h"
#include "Globals.h"

#endif // GBA_H
