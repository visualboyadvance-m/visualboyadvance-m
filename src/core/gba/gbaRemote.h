#ifndef VBAM_CORE_GBA_GBAREMOTE_H_
#define VBAM_CORE_GBA_GBAREMOTE_H_

#if !defined(VBAM_ENABLE_DEBUGGER)
#error "This file should only be included when VBAM_ENABLE_DEBUGGER is defined."
#endif  // !defined(VBAM_ENABLE_DEBUGGER)

#include <cstdint>

#define BitSet(array, bit) ((uint8_t*)(array))[(bit) >> 3] |= (1 << ((bit)&7))

#define BitClear(array, bit) ((uint8_t*)(array))[(bit) >> 3] &= ~(1 << ((bit)&7))

#define BitGet(array, bit) ((uint8_t)((array)[(bit) >> 3]) & (uint8_t)(1 << ((bit)&7)))

#define BreakSet(array, addr, flag) \
    ((uint8_t*)(array))[(addr) >> 1] |= ((addr & 1) ? (flag << 4) : (flag & 0xf))

#define BreakClear(array, addr, flag) \
    ((uint8_t*)(array))[(addr) >> 1] &= ~((addr & 1) ? (flag << 4) : (flag & 0xf))

// check
#define BreakThumbCheck(array, addr) ((uint8_t*)(array))[(addr) >> 1] & ((addr & 1) ? 0x80 : 0x8)

#define BreakARMCheck(array, addr) ((uint8_t*)(array))[(addr) >> 1] & ((addr & 1) ? 0x40 : 0x4)

#define BreakReadCheck(array, addr) ((uint8_t*)(array))[(addr) >> 1] & ((addr & 1) ? 0x20 : 0x2)

#define BreakWriteCheck(array, addr) ((uint8_t*)(array))[(addr) >> 1] & ((addr & 1) ? 0x10 : 0x1)

#define BreakCheck(array, addr, flag) \
    ((uint8_t*)(array))[(addr) >> 1] & ((addr & 1) ? (flag << 4) : (flag & 0xf))

extern bool dexp_eval(char*, uint32_t*);
extern void dexp_setVar(char*, uint32_t);
extern void dexp_listVars();
extern void dexp_saveVars(char*);
extern void dexp_loadVars(char*);

void debuggerOutput(const char* s, uint32_t addr);

bool debuggerBreakOnExecution(uint32_t address, uint8_t state);
bool debuggerBreakOnWrite(uint32_t address, uint32_t value, int size);
void debuggerBreakOnWrite(uint32_t address, uint32_t oldvalue, uint32_t value, int size, int t);
bool debuggerBreakOnRead(uint32_t address, int size);

struct regBreak {
    // uint8_t regNum; /No longer needed
    // bit 0 = equal
    // bit 1 = greater
    // bit 2 = smaller
    // bit 3 = signed
    uint8_t flags;
    uint32_t intVal;
    struct regBreak* next;
};
extern uint8_t lowRegBreakCounter[4]; //(r0-r3)
extern uint8_t medRegBreakCounter[4]; //(r4-r7)
extern uint8_t highRegBreakCounter[4]; //(r8-r11)
extern uint8_t statusRegBreakCounter[4]; //(r12-r15)

extern bool enableRegBreak;
extern regBreak* breakRegList[16];
extern void breakReg_check(int i);

struct regBreak* getFromBreakRegList(uint8_t regnum, int location);

void clearBreakRegList();
void clearParticularRegListBreaks(int reg);
void deleteFromBreakRegList(uint8_t regnum, int location);

void addBreakRegToList(uint8_t regnum, uint8_t flags, uint32_t value);
void printBreakRegList(bool verbose);

void remoteStubMain();
void remoteStubSignal(int sig, int number);
void remoteOutput(const char* s, uint32_t addr);
void remoteSetProtocol(int p);
void remoteSetPort(int port);

#endif // VBAM_CORE_GBA_GBAREMOTE_H_
