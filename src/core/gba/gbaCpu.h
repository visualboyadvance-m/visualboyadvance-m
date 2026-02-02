#ifndef VBAM_CORE_GBA_GBACPU_H_
#define VBAM_CORE_GBA_GBACPU_H_

#include <cstdint>

#include "core/base/system.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaGlobals.h"

extern int armExecute();
extern int thumbExecute();

#if defined(__i386__) || defined(__x86_64__)
#define INSN_REGPARM __attribute__((regparm(1)))
#else
#define INSN_REGPARM /*nothing*/
#endif

#ifdef __GNUC__
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#define UPDATE_REG(address, value)                 \
    {                                              \
        WRITE16LE(((uint16_t*)&g_ioMem[address]), value); \
    }

#define ARM_PREFETCH                                        \
    {                                                       \
        cpuPrefetch[0] = CPUReadMemoryQuick(armNextPC);     \
        cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC + 4); \
    }

#define THUMB_PREFETCH                                        \
    {                                                         \
        cpuPrefetch[0] = CPUReadHalfWordQuick(armNextPC);     \
        cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC + 2); \
    }

#define ARM_PREFETCH_NEXT cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC + 4);

#define THUMB_PREFETCH_NEXT cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC + 2);

constexpr uint8_t cpuBitsSet[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

/* cpuLowestBitSet is unused anywhere in the cores, but i'll put this here in case a use is found somewhere. 
constexpr uint8_t cpuLowestBitSet[256] = {
    8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
};*/

extern int SWITicks;
extern uint32_t mastercode;
extern bool busPrefetch;
extern bool busPrefetchEnable;
extern uint32_t busPrefetchCount;
extern int cpuNextEvent;
extern bool holdState;
extern uint32_t cpuPrefetch[2];
extern int cpuTotalTicks;
extern uint8_t memoryWait[16];
extern uint8_t memoryWait32[16];
extern uint8_t memoryWaitSeq[16];
extern uint8_t memoryWaitSeq32[16];
extern void CPUSwitchMode(int mode, bool saveState, bool breakLoop);
extern void CPUSwitchMode(int mode, bool saveState);
extern void CPUUpdateCPSR();
extern void CPUUpdateFlags(bool breakLoop);
extern void CPUUpdateFlags();
extern void CPUUndefinedException();
extern void CPUSoftwareInterrupt();
extern void CPUSoftwareInterrupt(int comment);

// Waitstates when accessing data
inline int dataTicksAccess16(uint32_t address) // DATA 8/16bits NON SEQ
{
    int addr = (address >> 24) & 15;
    int value = memoryWait[addr];

    if ((addr >= 0x08) || (addr < 0x02)) {
        busPrefetchCount = 0;
        busPrefetch = false;
    } else if (busPrefetch) {
        int waitState = value;
        if (!waitState)
            waitState = 1;
        busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
    }

    return value;
}

inline int dataTicksAccess32(uint32_t address) // DATA 32bits NON SEQ
{
    int addr = (address >> 24) & 15;
    int value = memoryWait32[addr];

    if ((addr >= 0x08) || (addr < 0x02)) {
        busPrefetchCount = 0;
        busPrefetch = false;
    } else if (busPrefetch) {
        int waitState = value;
        if (!waitState)
            waitState = 1;
        busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
    }

    return value;
}

inline int dataTicksAccessSeq16(uint32_t address) // DATA 8/16bits SEQ
{
    int addr = (address >> 24) & 15;
    int value = memoryWaitSeq[addr];

    if ((addr >= 0x08) || (addr < 0x02)) {
        busPrefetchCount = 0;
        busPrefetch = false;
    } else if (busPrefetch) {
        int waitState = value;
        if (!waitState)
            waitState = 1;
        busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
    }

    return value;
}

inline int dataTicksAccessSeq32(uint32_t address) // DATA 32bits SEQ
{
    int addr = (address >> 24) & 15;
    int value = memoryWaitSeq32[addr];

    if ((addr >= 0x08) || (addr < 0x02)) {
        busPrefetchCount = 0;
        busPrefetch = false;
    } else if (busPrefetch) {
        int waitState = value;
        if (!waitState)
            waitState = 1;
        busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
    }

    return value;
}

// Waitstates when executing opcode
inline int codeTicksAccess16(uint32_t address) // THUMB NON SEQ
{
    int addr = (address >> 24) & 15;

    if ((addr >= 0x08) && (addr <= 0x0D)) {
        if (busPrefetchCount & 0x1) {
            if (busPrefetchCount & 0x2) {
                busPrefetchCount = ((busPrefetchCount & 0xFF) >> 2) | (busPrefetchCount & 0xFFFFFF00);
                return 0;
            }
            busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
            return memoryWaitSeq[addr] - 1;
        } else {
            busPrefetchCount = 0;
            return memoryWait[addr];
        }
    } else {
        busPrefetchCount = 0;
        return memoryWait[addr];
    }
}

inline int codeTicksAccess32(uint32_t address) // ARM NON SEQ
{
    int addr = (address >> 24) & 15;

    if ((addr >= 0x08) && (addr <= 0x0D)) {
        if (busPrefetchCount & 0x1) {
            if (busPrefetchCount & 0x2) {
                busPrefetchCount = ((busPrefetchCount & 0xFF) >> 2) | (busPrefetchCount & 0xFFFFFF00);
                return 0;
            }
            busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
            return memoryWaitSeq32[addr] - 1; // NOTE: was memoryWaitSeq[]
        } else {
            busPrefetchCount = 0;
            return memoryWait32[addr];
        }
    } else {
        busPrefetchCount = 0;
        return memoryWait32[addr];
    }
}

inline int codeTicksAccessSeq16(uint32_t address) // THUMB SEQ
{
    int addr = (address >> 24) & 15;

    if ((addr >= 0x08) && (addr <= 0x0D)) {
        if (busPrefetchCount & 0x1) {
            busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
            return 0;
        } else if (busPrefetchCount > 0xFF) {
            busPrefetchCount = 0;
            return memoryWait[addr];
        } else
            return memoryWaitSeq[addr];
    } else {
        busPrefetchCount = 0;
        return memoryWaitSeq[addr];
    }
}

inline int codeTicksAccessSeq32(uint32_t address) // ARM SEQ
{
    int addr = (address >> 24) & 15;

    if ((addr >= 0x08) && (addr <= 0x0D)) {
        if (busPrefetchCount & 0x1) {
            if (busPrefetchCount & 0x2) {
                busPrefetchCount = ((busPrefetchCount & 0xFF) >> 2) | (busPrefetchCount & 0xFFFFFF00);
                return 0;
            }
            busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
            return memoryWaitSeq32[addr]; // NOTE: was memoryWaitSeq[]
        } else if (busPrefetchCount > 0xFF) {
            busPrefetchCount = 0;
            return memoryWait32[addr];
        } else
            return memoryWaitSeq32[addr];
    } else {
        busPrefetchCount = 0; // NOTE: was previosly missing
        return memoryWaitSeq32[addr];
    }
}

// Emulates the Cheat System (m) code
inline void cpuMasterCodeCheck()
{
    if ((mastercode) && (mastercode == armNextPC)) {
        uint32_t joy = 0;
        if (systemReadJoypads())
            joy = systemReadJoypad(-1);
        uint32_t ext = (joy >> 10);
        cpuTotalTicks += cheatsCheckKeys(P1 ^ 0x3FF, ext);
    }
}

#endif  // VBAM_CORE_GBA_GBACPU_H_
