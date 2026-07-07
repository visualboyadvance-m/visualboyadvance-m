#ifndef VBAM_CORE_GBA_GBACPU_H_
#define VBAM_CORE_GBA_GBACPU_H_

#include <cstdint>

#include "core/base/system.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaGlobals.h"

extern int armExecute();
extern int thumbExecute();

// regparm only has an effect on 32-bit x86; on x86_64 it is ignored by the
// compiler and emits a -Wattributes warning, so restrict it to __i386__.
#if defined(__i386__)
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

extern int SWITicks;
extern uint32_t mastercode;
extern bool busPrefetch;
extern bool busPrefetchEnable;
extern uint32_t busPrefetchCount;     // legacy bitmask (kept for save/restore)
extern int busPrefetchHalfwords;       // previous instruction's fractional prefetch progress
extern int busPrefetchAccum;           // cycles accumulated toward next halfword
extern int busPrefetchFrac;            // persistent prefetch-depth carry (mGBA model)
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

// Per-cycle bus-state simulator helpers (scaffolding for a future
// halfword-FIFO prefetch model). See project_dma_cycle_rework.md for the
// roadmap and the hill-climbing iteration log demonstrating why the
// rewrite cannot complete in a single session.

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
        busPrefetchFrac = 0;
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
        busPrefetchFrac = 0;
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
        busPrefetchFrac = 0;
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
        busPrefetchFrac = 0;
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
            return memoryWaitSeq32[addr] - 2; // NOTE: was memoryWaitSeq[]
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
        } else if (busPrefetchFrac >= (memoryWaitSeq[addr] + 1)) {
            // Fetch-bound sequential fetch, but the persistent prefetch
            // reservoir (busPrefetchRomFloor) has buffered a full extra
            // halfword: the opcode is already in the buffer, so this fetch
            // is one waitstate cheaper (mGBA deep-prefetch behaviour).
            busPrefetchFrac -= (memoryWaitSeq[addr] + 1);
            return memoryWaitSeq[addr] - 1;
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

// GamePak prefetch bus contention (GBATEK "GamePak Prefetch"): while the
// CPU executes from ROM with the prefetch buffer enabled, the prefetcher
// owns the cart bus and the sequential code fetch proceeds in parallel
// with the instruction's internal cycles and any data accesses to
// non-cart regions. The observable duration is therefore the maximum of
// the sequential code-fetch time and the zero-wait execution cost.
// A data access on the cart bus (or BIOS) aborts the prefetch, so those
// instructions keep the legacy busPrefetchCount timing (`cartData`).
//
// `legacy` is the cost computed by the old busPrefetchCount model,
// `pure` the instruction cost assuming zero-wait code fetches (internal
// cycles + data cycles + 1S), `halfwords` the opcode fetch width
// (Thumb 1, ARM 2).
inline int busPrefetchRomFloor(int legacy, int pure, int halfwords, bool cartData)
{
    int addr = (armNextPC >> 24) & 15;
    if (cartData || !busPrefetchEnable || (addr < 0x08) || (addr > 0x0D))
        return legacy;
    int fetch = (memoryWaitSeq[addr] + 1) * halfwords;
    if (pure <= fetch)
        return fetch;
    // The prefetcher keeps fetching during the surplus cycles; publish
    // the halfwords it completes as ready-bits for the legacy
    // busPrefetchCount consumers (a partially fetched halfword still
    // discounts the next fetch, hence the round-up), and record the
    // fractional progress for busPrefetchRomStall.
    int surplus = (pure - fetch + memoryWaitSeq[addr]) / (memoryWaitSeq[addr] + 1);
    if (surplus > 8)
        surplus = 8;
    busPrefetchCount |= (1u << surplus) - 1;
    int frac = (pure - fetch) % (memoryWaitSeq[addr] + 1);
    busPrefetchAccum = frac;
    // mGBA lastPrefetchedPc model: the fractional prefetch progress this
    // fetch leaves behind is not lost — it accumulates into a persistent
    // reservoir that survives across the stores/ALU/loads of a straight-line
    // ROM run (the per-instruction busPrefetchCount bits only track one
    // pending halfword). The reservoir is *separate* from those bits, so the
    // ordinary opcode consumers don't drain it; only a fetch-bound access
    // that finds the single-halfword buffer empty taps it (see
    // codeTicksAccessSeq16). Over a long run it fills deep enough that one
    // later fetch hits a hot buffer — the C-loop +1. Capped at 8 halfworths.
    busPrefetchFrac += frac;
    int cap = (memoryWaitSeq[addr] + 1) * 8;
    if (busPrefetchFrac > cap)
        busPrefetchFrac = cap;
    return pure;
}

// Stall for a cart-bus data access that catches the prefetcher part way
// through a halfword fetch: the CPU waits for the in-flight fetch to
// finish before it gets the bus. busPrefetchHalfwords carries the
// fractional progress left over by the previous instruction's surplus
// (see busPrefetchRomFloor).
inline int busPrefetchRomStall()
{
    int addr = (armNextPC >> 24) & 15;
    if (!busPrefetchEnable || (addr < 0x08) || (addr > 0x0D)
        || busPrefetchHalfwords == 0)
        return 0;
    int rem = (memoryWaitSeq[addr] + 1) - busPrefetchHalfwords;
    return rem > 0 ? rem : 0;
}

// A data access that reaches the cart bus while the prefetcher is
// running aborts the prefetch; if it lands just as the prefetcher has
// started fetching a new halfword, the CPU stalls one extra cycle
// waiting for the bus (GBATEK "GamePak Prefetch"). `elapsed` is the
// cycle count the current instruction has consumed so far.
inline int busPrefetchAbortStall(uint32_t dataAddr, int elapsed)
{
    int addr = (armNextPC >> 24) & 15;
    uint32_t dr = (dataAddr >> 24) & 15;
    if (!busPrefetchEnable || (addr < 0x08) || (addr > 0x0D) || (dr < 0x08))
        return 0;
    return ((elapsed + 1) % (memoryWaitSeq[addr] + 1)) == 0 ? 1 : 0;
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
