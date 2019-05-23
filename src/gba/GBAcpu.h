#ifndef GBACPU_H
#define GBACPU_H

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
        WRITE16LE(((uint16_t*)&ioMem[address]), value); \
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
extern uint8_t cpuBitsSet[256];
extern uint8_t cpuLowestBitSet[256];
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
            return memoryWaitSeq[addr] - 1;
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
            return memoryWaitSeq[addr];
        } else if (busPrefetchCount > 0xFF) {
            busPrefetchCount = 0;
            return memoryWait32[addr];
        } else
            return memoryWaitSeq32[addr];
    } else {
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

#endif // GBACPU_H
