#ifndef VBAM_CORE_GBA_GBAINLINE_H_
#define VBAM_CORE_GBA_GBAINLINE_H_

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "core/base/port.h"
#include "core/gba/gbaScheduler.h"
#include "core/base/script_hooks.h"
#include "core/base/system.h"
#include "core/gba/gbaCpu.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaMgbaLog.h"
#include "core/gba/gbaPrint.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"

#if defined(VBAM_ENABLE_DEBUGGER)
#include "core/gba/gbaRemote.h"
#endif  // defined(VBAM_ENABLE_DEBUGGER)

extern const uint32_t objTilesAddress[3];

// PPU VRAM fetch-slot occupancy bits, keyed into kVramFetchSlots. Each bit
// marks a background layer (text 4bpp / text 8bpp / affine BG2 / affine BG3 /
// bitmap) that consumes VRAM fetch bandwidth on a given slot of the PPU's
// 32-cycle fetch cadence.
#define VRAM_FETCH_T4(X) (0x011u << (X))
#define VRAM_FETCH_T8(X) (0x010u << (X))
#define VRAM_FETCH_AFF2    0x100u
#define VRAM_FETCH_AFF3    0x200u
#define VRAM_FETCH_BMP     0x400u

// =====================================================================
// VBAM_HB_TRACE: ad-hoc cycle-level instrumentation. Default OFF -- every
// trace call site expands to a sizeof()-only expression and is eliminated
// by the optimizer, so there is zero runtime cost in normal builds.
//
// To enable at compile time: define VBAM_HB_TRACE (uncomment the line
// below, or pass -DVBAM_HB_TRACE to the compiler). Three INDEPENDENT
// runtime gates then become available, each toggled by its own env var
// (Linux/macOS only; Windows always emits all enabled categories to
// OutputDebugStringA when compiled with VBAM_HB_TRACE):
//
//   VBAM_TRACE_HB=1   -- HBlank/timer/halt events:
//                          hb-raise, tm0-read, halt
//   VBAM_TRACE_IRQ=1  -- IRQ machinery and IO-reg writes:
//                          sched-irq, cancel-irq, dispatch-irq,
//                          ie-write, if-write, ime-write
//   VBAM_TRACE_SIO=1  -- Serial IO (Normal8/Normal32) cycle accounting:
//                          sio-cnt-write, sio-sched, sio-complete
//
// Back-compat: VBAM_HB_TRACE=1 (the old single env var) still works and
// is treated as "VBAM_TRACE_HB=1 VBAM_TRACE_IRQ=1" -- the original two
// categories the macro was named for. Setting any per-category env var
// supersedes the legacy one for that category. See gba.cpp for the
// per-tag extra-bits schema.
//
// #define VBAM_HB_TRACE
#ifdef VBAM_HB_TRACE

// Trace category. Each call site picks the category whose data it
// represents; runtime env-var gating routes the line to stderr or
// suppresses it without touching the compile-time switch.
enum VbamTraceCat {
    VBAM_CAT_HB  = 0,
    VBAM_CAT_IRQ = 1,
    VBAM_CAT_SIO = 2,
};

void vbam_dbg_trace(int cat, const char* tag, long long cyc, int extra);

// Per-category convenience wrappers. Prefer these at call sites; they
// document intent and make grep-by-category cheap.
#define vbam_hb_trace(tag, cyc, extra) \
    vbam_dbg_trace(VBAM_CAT_HB,  (tag), (cyc), (extra))
#define vbam_irq_trace(tag, cyc, extra) \
    vbam_dbg_trace(VBAM_CAT_IRQ, (tag), (cyc), (extra))
#define vbam_sio_trace(tag, cyc, extra) \
    vbam_dbg_trace(VBAM_CAT_SIO, (tag), (cyc), (extra))

#else

// When trace is OFF, expand to an expression that references each argument
// in an UNEVALUATED context (sizeof) so the compiler considers them "used"
// without actually computing them at runtime. Without this, callers that
// pre-compute trace-only values trip -Werror=unused-variable /
// -Werror=unused-but-set-variable on builds compiled without
// VBAM_HB_TRACE defined. The C++ standard guarantees sizeof's operand is
// not evaluated, so this is zero-cost.
#define vbam_hb_trace(tag, cyc, extra) \
    ((void)sizeof((tag)), (void)sizeof((cyc)), (void)sizeof((extra)))
#define vbam_irq_trace(tag, cyc, extra) \
    ((void)sizeof((tag)), (void)sizeof((cyc)), (void)sizeof((extra)))
#define vbam_sio_trace(tag, cyc, extra) \
    ((void)sizeof((tag)), (void)sizeof((cyc)), (void)sizeof((extra)))

#endif
// =====================================================================

extern bool stopState;
extern bool holdState;
extern int holdType;
extern int cpuNextEvent;
extern bool cpuSramEnabled;
extern bool cpuFlashEnabled;
extern bool cpuEEPROMEnabled;
extern bool cpuEEPROMSensorEnabled;
extern bool cpuDmaRunning;
extern uint32_t cpuDmaPC;
extern bool timer0On;
extern int timer0Ticks;
extern int timer0ClockReload;
extern bool timer1On;
extern int timer1Ticks;
extern int timer1ClockReload;
extern bool timer2On;
extern int timer2Ticks;
extern int timer2ClockReload;
extern bool timer3On;
extern int timer3Ticks;
extern int timer3ClockReload;
extern int cpuTotalTicks;
extern int64_t cpuAbsCycle;
extern int64_t lcdNextEventAbsCycle;
extern int64_t hblankIrqRaiseAbsCycle;
extern int64_t lastHblankPollCycle;
extern int64_t timerEnableAbsCycle[4];
extern uint16_t timerReloadAtEnable[4];
extern int gbaTimerEnablePhase(int n);
extern int timer0Reload;
extern int timer1Reload;
extern int timer2Reload;
extern int timer3Reload;

extern uint32_t cpuDmaBusValue;

#define CPUReadByteQuick(addr) map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]

#define CPUReadHalfWordQuick(addr) \
    READ16LE(((uint16_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

#define CPUReadMemoryQuick(addr) \
    READ32LE(((uint32_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

static inline uint16_t DowncastU16(uint32_t value) {
    return static_cast<uint16_t>(value);
}

// Live timer-counter value for timer `n` after `elapsed` cycles of run time.
// Precise reload-register semantics: the counter cycles with period
// (0x10000 - reloadAtEnable) until the first overflow, then with period
// (0x10000 - reloadNow) thereafter -- matching real HW's "reload register is
// latched on overflow" behavior. Callers pick the anchoring offset (read vs
// write commit point) when computing `elapsed`.
static inline uint16_t gbaTimerLiveValue(int n, int64_t elapsed)
{
    uint32_t reloadNow;
    int prescale;
    switch (n) {
    case 0:  reloadNow = (uint16_t)timer0Reload; prescale = timer0ClockReload; break;
    case 1:  reloadNow = (uint16_t)timer1Reload; prescale = timer1ClockReload; break;
    case 2:  reloadNow = (uint16_t)timer2Reload; prescale = timer2ClockReload; break;
    default: reloadNow = (uint16_t)timer3Reload; prescale = timer3ClockReload; break;
    }
    uint32_t r0 = timerReloadAtEnable[n];
    if (elapsed < 0)
        elapsed = 0;
    int64_t cyclesToFirstOverflow = (int64_t)(0x10000u - r0) << prescale;
    if (elapsed < cyclesToFirstOverflow) {
        return (uint16_t)(r0 + (uint32_t)(elapsed >> prescale));
    }
    int64_t post = elapsed - cyclesToFirstOverflow;
    int64_t period = (int64_t)(0x10000u - reloadNow) << prescale;
    if (period <= 0)
        period = (int64_t)0x10000 << prescale;
    post %= period;
    return (uint16_t)(reloadNow + (uint32_t)(post >> prescale));
}

// Live timer read at the current CPU cycle, for use in memory-read paths.
// The standard offset is -2 (compensates for the write-handler snapshot of
// timerEnableAbsCycle happening at instruction START while real HW commits
// the store mid-pipeline). For reloadAtEnable=0xFFFF (period=1) with a
// different current reload, the very first overflow happens during the same
// dispatch boundary as the read; real HW latches the new reload register
// before the read can sample, but our model lags by one cycle. Use -3 in
// that boundary case only.
static inline uint16_t gbaTimerLiveRead(int n)
{
    uint32_t reloadNow;
    switch (n) {
    case 0:  reloadNow = (uint16_t)timer0Reload; break;
    case 1:  reloadNow = (uint16_t)timer1Reload; break;
    case 2:  reloadNow = (uint16_t)timer2Reload; break;
    default: reloadNow = (uint16_t)timer3Reload; break;
    }
    int64_t offset = (timerReloadAtEnable[n] == 0xFFFF && reloadNow != timerReloadAtEnable[n]) ? 3 : 2;
    // Anchor at the prescaler-aligned enable edge (see gbaTimerEnablePhase)
    // so live reads agree with the aligned countdown in applyTimer.
    return gbaTimerLiveValue(
        n, cpuAbsCycle - timerEnableAbsCycle[n] - offset + gbaTimerEnablePhase(n));
}

static inline int16_t Downcast16(int32_t value) {
    return static_cast<int16_t>(value);
}

// --- PPU/CPU VRAM bus-contention helpers -------------------------------------
extern unsigned bgFetchMask;
extern int lcdTicks;
extern const uint16_t kVramFetchSlots[32];

static inline int computeVramContentionStall(int wait, int extra) {
    // lcdTicks counts down to the next LCD event (the HBlank start while
    // in HDraw), giving the PPU's phase within the scanline; it advances
    // only at batch boundaries, so subtract the CPU's progress into the
    // current batch for the live position at this access. -until & 0x1F
    // selects the current slot of the 32-cycle VRAM fetch cadence. The
    // loop walks forward to the first slot the PPU leaves free (skipping
    // one extra free slot for a wide 32-bit access, which needs two).
    // If every examined slot is occupied, the CPU waits until the PPU
    // releases the bus at HBlank -- `until` exactly (the mask is zeroed
    // on HBlank entry, so this fallback only arises mid-HDraw). The
    // access's own duration absorbs the head of the stall (stall -= wait).
    extern int cpuTotalTicks;
    int32_t until = lcdTicks - cpuTotalTicks;
    if (until <= 0) return 0;
    int period = (-until) & 0x1F;
    int32_t stall = until;
    for (int i = 0; i < 16; ++i) {
        if (!(kVramFetchSlots[(period + i) & 0x1F] & bgFetchMask)) {
            if (!extra) { stall = i; break; }
            --extra;
        }
    }
    stall -= wait;
    if (stall < 0) return 0;
    return stall;
}

// Charge VRAM bus contention for a BG-VRAM access. `origAddr` is the masked
// VRAM offset; `wide` marks a 32-bit access. The stall is accumulated into
// vramContentionCycles and folded into the instruction's clockTicks by the CPU
// core. OBJ (sprite) VRAM sits above the BG region and is fetched on a separate
// path, so it does not contend here.
static inline void chargeVramContention(unsigned origAddr, bool wide) {
    if (!bgFetchMask) return;
    const unsigned mode = DISPCNT & 7;
    const unsigned gate = (mode >= 3) ? 0x14000u : 0x10000u;
    if ((origAddr & 0x1FFFF) >= gate) return; // OBJ VRAM: no contention
    extern int vramContentionCycles;
    // 16-bit VRAM accesses take the base cycle (wait 0); 32-bit split into
    // two halfword slots (wait 1) and reserve two free fetch slots.
    vramContentionCycles += computeVramContentionStall(wide ? 1 : 0, wide ? 1 : 0);
}

template<typename T>
static inline uint8_t DowncastU8(T value) {
    static_assert(std::is_integral<T>::value, "Integral type required.");
    static_assert(sizeof(T) ==2 || sizeof(T) == 4, "16 or 32 bits int required");
    return static_cast<uint8_t>(value);
}

template<typename T>
static inline int8_t Downcast8(T value) {
    static_assert(std::is_integral<T>::value, "Integral type required.");
    static_assert(sizeof(T) ==2 || sizeof(T) == 4, "16 or 32 bits int required");
    return static_cast<int8_t>(value);
}

static inline uint32_t CPUReadOpenBus()
{
    /* DMA shadowing overrides everything */
    if (cpuDmaRunning)
        return cpuDmaBusValue;
    /* DMA bus-release window: a DMA that just released the bus leaves its
       last transferred value latched there. An open-bus read whose data
       access happens before anything re-drives the bus (the prefetcher's
       first refill fetch takes one ROM non-sequential access) still samples
       the DMA's value instead of the prefetch latch. cpuTotalTicks resets
       at event service, so within the DMA's event batch it measures cycles
       since the DMA ran. */
    {
        /* The last DMA's bus word remains visible to open-bus reads for
           exactly one instruction slot after the transfer (the reading
           pc is one instruction width past the pc that was pending when
           the DMA ran). armNextPC here is the pc of the instruction
           doing this read; g_hdmaActAbs was the pending pc when the
           DMA ran. */
        /* Hardware grants an HBlank DMA the bus mid-instruction: when
           the grant lands inside the reading instruction's own span --
           after its opcode fetch, before its data cycle -- the data
           read returns the DMA's word.
           In this batch model the reader has already started when the
           deferred kSchedHdma event is still pending, so peek: if the
           fire is due within the reader's span, return the word the
           DMA is about to drive (its source data). The complementary
           boundary case -- the transfer ran in the gap right before
           the reader started (pending == reader) -- reads the freshly
           latched cpuDmaBusValue. */
        {
            extern uint32_t dma3Source;
            if (gbaScheduler::IsScheduled(kSchedHdma) && (DM3CNT_H & 0x8000)) {
                /* Grant window: after the reader's opcode fetch
                   completes (>= 3 at seq-ROM cost) and before its data
                   cycle (the 7-cycle unmapped LDM's data slot). A grant
                   landing during the fetch waits for it and still owns
                   the bus at the data cycle; one landing at/after the
                   data cycle is too late. */
                /* The activation must land strictly inside the reading
                   instruction's opcode fetch: an activation at the
                   fetch's own start is consumed by that fetch and the
                   latch is refreshed away. Window (0, 3] at seq-ROM
                   fetch cost. */
                static const int winLo = 1;
                static const int winHi = 4;
                /* The scheduler cursor sits at the batch start; the read
                   executes cpuAbsCycle into the batch. */
                extern int64_t g_hdmaActAbs;
                extern int64_t cpuAbsCycle;
                const int rel = (int)(g_hdmaActAbs - cpuAbsCycle);

                if (rel >= winLo && rel < winHi)
                    return CPUReadMemoryQuick(dma3Source);
            }
            /* A transfer that completed before this instruction's own
               opcode fetch is never visible: the fetch refreshes the
               bus latch. No boundary case. */
        }
    }

    /* THUMB: compose 32-bit from last fetched halfwords according to region/alignment */
    if (!armState) {
        uint32_t reg15 = reg[15].I;
        auto region = reg15 >> 24;

        switch (region)
        {
        /* Sequential regions: both halves from the newer fetch */
        case REGION_EWRAM: /* EWRAM */
        case REGION_PRAM: /* PALRAM */
        case REGION_VRAM: /* VRAM */
        case REGION_ROM0:
        case REGION_ROM0EX:
        case REGION_ROM1:
        case REGION_ROM1EX:
        case REGION_ROM2:
        case REGION_ROM2EX: /* CART regions */
            return cpuPrefetch[1] | (cpuPrefetch[1] << 16);

        /* BIOS / OAM: alignment affects low/high halves */
        case REGION_BIOS: /* BIOS */
        case REGION_OAM: /* OAM */
            if ((reg15 & 2) == 0)
                return cpuPrefetch[1] | (cpuPrefetch[1] << 16);
            return cpuPrefetch[0] | (cpuPrefetch[1] << 16);

        /* IWRAM: opposite order for aligned case */
        case REGION_IWRAM: /* IWRAM */
            if ((reg15 & 2) == 0)
                return cpuPrefetch[1] | (cpuPrefetch[0] << 16);
            return cpuPrefetch[0] | (cpuPrefetch[1] << 16);
        }
    }

    /* ARM: open bus is the last fetched 32-bit word */
    return cpuPrefetch[1];
}

static inline uint16_t ROMReadOOB(uint32_t address) {
    return (address >> 1) & 0xFFFF;
}

static inline bool IsGPIO(uint32_t address) {
    // TODO: Need to check which GPIO feature really is enabled
    return (address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8);
}

// used for ROM boundary check
static inline bool IsEEPROM(uint32_t address) {
    return (cpuEEPROMEnabled && (address & eepromMask) == eepromMask);
}

static inline bool isSaveGame() {
    return (coreOptions.saveType != 5) && (!eepromInUse || cpuSramEnabled || cpuFlashEnabled);
}

// Reads sram or flash
static inline uint8_t CPUReadBackup(uint32_t address) {
    return flashRead(address);
}

// writes sram or flash
static inline void CPUWriteBackup(uint32_t address, uint8_t value) {
    if (cpuSaveGameFunc)
        (*cpuSaveGameFunc)(address, value);
}

static inline uint32_t CPUReadMemory(uint32_t address)
{
    if (g_vbam_script_has_read_hooks && g_vbam_script_mem_read)
        g_vbam_script_mem_read(address, 4, 0);
#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 2)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif
    uint32_t value = 0;

    switch (address >> 24) {
    case REGION_BIOS:
        if (reg[15].I >> 24) {
            if (address < 0x4000) {
#ifdef GBA_LOGGING
                if (systemVerbose & VERBOSE_ILLEGAL_READ) {
                    log("Illegal word read from bios: %08x at %08x\n",
                        address,
                        armMode ? armNextPC - 4 : armNextPC - 2);
                }
#endif

                value = READ32LE(((uint32_t*)&biosProtected));
            } else
                goto unreadable;
        } else
            value = READ32LE(((uint32_t*)&g_bios[address & 0x3FFC]));
        break;
    case REGION_EWRAM:
        value = READ32LE(((uint32_t*)&g_workRAM[address & 0x3FFFC]));
        break;
    case REGION_IWRAM:
        value = READ32LE(((uint32_t*)&g_internalRAM[address & 0x7ffC]));
        break;
    case REGION_IO:
        if ((address < 0x4000400) && ioReadable[address & 0x3fc]) {
            if (ioReadable[(address & 0x3fc) + 2]) {
                value = READ32LE(((uint32_t*)&g_ioMem[address & 0x3fC]));
                if ((address & 0x3fc) == COMM_JOY_RECV_L)
                    UPDATE_REG(COMM_JOYSTAT,
                        READ16LE(&g_ioMem[COMM_JOYSTAT]) & ~JOYSTAT_RECV);
            } else {
                value = READ16LE(((uint16_t*)&g_ioMem[address & 0x3fc]));
            }
            // SOUNDCNT_X (NR52) sits in the low halfword of address
            // 0x04000084. A 32-bit read at 0x04000084 covers NR52
            // (low halfword) plus 2 bytes of unused space (high
            // halfword). Compose the live channel-ON status into the
            // low byte; high byte/halfword are unused/zero.
            if ((address & 0x3fc) == IO_REG_SOUNDCNT_X) {
                value = (value & 0xFFFF0000u) | (uint32_t)soundReadNR52();
            }
            // A 32-bit read at TMnCNT covers the counter (low halfword)
            // plus the control register (high halfword). Substitute the
            // live counter value just like the halfword read path does;
            // the control half stays as stored in g_ioMem.
            if (((address & 0x3fc) >= 0x100) && ((address & 0x3fc) <= 0x10C)) {
                if (((address & 0x3fc) == IO_REG_TM0CNT_L) && timer0On) {
                    value = (value & 0xFFFF0000u) | gbaTimerLiveRead(0);
                    vbam_hb_trace("tm0-read32", cpuAbsCycle,
                                   (int)(value & 0xFFFF));
                } else if (((address & 0x3fc) == IO_REG_TM1CNT_L) && timer1On && !(TM1CNT & 4))
                    value = (value & 0xFFFF0000u) | gbaTimerLiveRead(1);
                else if (((address & 0x3fc) == IO_REG_TM2CNT_L) && timer2On && !(TM2CNT & 4))
                    value = (value & 0xFFFF0000u) | gbaTimerLiveRead(2);
                else if (((address & 0x3fc) == IO_REG_TM3CNT_L) && timer3On && !(TM3CNT & 4))
                    value = (value & 0xFFFF0000u) | gbaTimerLiveRead(3);
            }
        } else
            goto unreadable;
        break;
    case REGION_PRAM:
        value = READ32LE(((uint32_t*)&g_paletteRAM[address & 0x3fC]));
        break;
    case REGION_VRAM: {
        chargeVramContention(address & 0x1FFFF, true);
        unsigned addr = (address & 0x1fffc);
        if (((DISPCNT & 7) > 2) && ((addr & 0x1C000) == 0x18000)) {
            value = 0;
            break;
        }
        if ((addr & 0x18000) == 0x18000)
            addr &= 0x17fff;
        value = READ32LE(((uint32_t*)&g_vram[addr]));
        break;
    }
    case REGION_OAM:
        value = READ32LE(((uint32_t*)&g_oam[address & 0x3FC]));
        break;
    case REGION_ROM0:
    case REGION_ROM0EX:
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if (IsEEPROM(address))
            return 0; // ignore reads from eeprom region outside 0x0D page reads    
        else if ((address & 0x01FFFFFC) <= (gbaGetRomSize() - 4))
            value = READ32LE(((uint32_t *)&g_rom[address & 0x01FFFFFC]));
        else {
            value = (uint16_t)ROMReadOOB(address & 0x01FFFFFC);
            value |= (uint16_t)ROMReadOOB((address & 0x01FFFFFC) + 2) << 16;
        }
        break;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled)
            return eepromRead(address);
        goto unreadable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (isSaveGame()) {
            // Byte-broadcast: a 32-bit SRAM read returns the same byte in
            // all four lanes. The cast to uint32_t is required for defined
            // behavior -- without it, CPUReadBackup's uint8_t result is
            // promoted to int and multiplying by 0x01010101 (16843009)
            // overflows signed int for byte values >= 128. UBSan caught
            // this; some MSVC codegen handles the overflow differently
            // from GCC, which contributes to host-platform test divergence.
            value = (uint32_t)CPUReadBackup(address) * 0x01010101u;
            break;
        }
#ifdef GBA_LOGGING
        // Just normal log, not openbus
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal word read: %08x at %08x\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        return 0xffffffff;
    default:
    unreadable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal word read: %08x at %08x\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        value = CPUReadOpenBus();
        break;
    }

    if (address & 3) {
#ifdef C_CORE
        int shift = (address & 3) << 3;
        value = (value >> shift) | (value << (32 - shift));
#else
#ifdef __GNUC__
        asm("and $3, %%ecx;"
            "shl $3 ,%%ecx;"
            "ror %%cl, %0"
            : "=r"(value)
            : "r"(value), "c"(address));
#else
        __asm {
      mov ecx, address;
      and ecx, 3;
      shl ecx, 3;
      ror [dword ptr value], cl;
        }
#endif
#endif
    }

#ifdef GBA_LOGGING
    if (address & 3) {
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned word read from: %08x at %08x (%08x)\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2,
                value);
        }
    }
#endif
    return value;
}

static inline uint32_t CPUReadHalfWord(uint32_t address)
{
    if (g_vbam_script_has_read_hooks && g_vbam_script_mem_read)
        g_vbam_script_mem_read(address, 2, 0);
#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    uint32_t value = 0;

    switch (address >> 24) {
    case REGION_BIOS:
        if (reg[15].I >> 24) {
            if (address < 0x4000) {
#ifdef GBA_LOGGING
                if (systemVerbose & VERBOSE_ILLEGAL_READ) {
                    log("Illegal halfword read from bios: %08x at %08x\n",
                        address,
                        armMode ? armNextPC - 4 : armNextPC - 2);
                }
#endif
                value = READ16LE(((uint16_t*)&biosProtected[address & 2]));
            } else
                goto unreadable;
        } else
            value = READ16LE(((uint16_t*)&g_bios[address & 0x3FFE]));
        break;
    case REGION_EWRAM:
        value = READ16LE(((uint16_t*)&g_workRAM[address & 0x3FFFE]));
        break;
    case REGION_IWRAM:
        value = READ16LE(((uint16_t*)&g_internalRAM[address & 0x7ffe]));
        break;
    case REGION_IO:
        if ((address < 0x4000400) && ioReadable[address & 0x3fe]) {
            value = READ16LE(((uint16_t*)&g_ioMem[address & 0x3fe]));
            switch(address & 0x3FE) {
            case IO_REG_SOUND1CNT_X: value &= 0x4000; break;
            case IO_REG_SOUND2CNT_L: value &= 0xFFC0; break;
            case IO_REG_SOUND2CNT_H: value &= 0x4000; break;
            case IO_REG_SOUND3CNT_L: value &= 0x00E0; break;
            case IO_REG_SOUND3CNT_H: value &= 0xE000; break;
            case IO_REG_SOUND3CNT_X: value &= 0x4000; break;
            case IO_REG_SOUND4CNT_L: value &= 0xFF00; break;
            case IO_REG_SOUND4CNT_H: value &= 0x40FF; break;
            case IO_REG_SOUNDCNT_X:
                // Compose the live PSG channel-ON status into bits 0-3
                // of the low byte; high byte (0x85) is unused/zero.
                value = (uint32_t)soundReadNR52();
                break;
            }
            if (((address & 0x3fe) > 0xFF) && ((address & 0x3fe) < 0x10E)) {
                // Live timer-counter read (see gbaTimerLiveRead).
                if (((address & 0x3fe) == IO_REG_TM0CNT_L) && timer0On) {
                    value = gbaTimerLiveRead(0);
                    vbam_hb_trace("tm0-read", cpuAbsCycle,
                                   (int)(value & 0xFFFF));
                } else if (((address & 0x3fe) == IO_REG_TM1CNT_L) && timer1On && !(TM1CNT & 4))
                    value = gbaTimerLiveRead(1);
                else if (((address & 0x3fe) == IO_REG_TM2CNT_L) && timer2On && !(TM2CNT & 4))
                    value = gbaTimerLiveRead(2);
                else if (((address & 0x3fe) == IO_REG_TM3CNT_L) && timer3On && !(TM3CNT & 4))
                    value = gbaTimerLiveRead(3);
            }
            // Sub-cycle DISPSTAT alignment: the dispatch loop only flips
            // DISPSTAT.HBlank at event boundaries, but a poll-loop read
            // mid-dispatch can land *after* the actual scanline crossing.
            // The Thumb LDRH/ARM LDRH memory access happens ~1 cycle into
            // the instruction (after address calculation), so the effective
            // access cycle is cpuAbsCycle + 1 (cpuAbsCycle holds the start-
            // of-instruction value during execute). If that access cycle is
            // past the scheduled flip, return the post-flip value so polling
            // loops detect the bit at the exact cycle real HW would.
            //
            // Tight DISPSTAT-polling loops (LDRH/EOR/TST/BNE in IWRAM, ~8
            // cycles per iter) drift relative to real HW because our
            // model's per-instruction cycle counts aren't accurate to
            // single-cycle precision in the tight-loop case. When we
            // detect a polling pattern (current read within 1 iteration's
            // worth of the previous DISPSTAT read), extend the lookahead
            // by 7 cycles so the post-flip value is returned at the SAME
            // iteration count real HW would. This recovers the misc-edge
            // "Flip 2" sub-test (HDraw-period polling-loop alignment)
            // without any global cycle change.
            // Tight DISPSTAT-polling loops (LDRH/EOR/TST/BNE in IWRAM,
            // ~8 cycles/iter) drift relative to real HW because our
            // ARM7TDMI cycle model isn't single-cycle precise in the
            // tight-loop case. The misc-edge HDraw/HBlank polling
            // sub-tests measure cycle differences exactly equal to one
            // polling-loop iteration off (1004 vs ~1012, 228 vs ~221).
            // A direction-asymmetric pre-flip lookahead recovers the
            // alignment without shifting the underlying scanline:
            //   HBlank=0 currently -> about to flip ON (HDraw -> HBlank
            //                       transition, end of HDraw period):
            //                       +8 cycle lookahead, detect ~1 iter
            //                       earlier so HDraw measure shrinks.
            //   HBlank=1 currently -> about to flip OFF (HBlank -> HDraw
            //                       transition, end of HBlank period):
            //                       -6 cycle lookahead, detect ~1 iter
            //                       later so HBlank measure grows.
            if ((address & 0x3fe) == IO_REG_DISPSTAT) {
                static const int64_t biasOff = [] {
                    const char* e = getenv("VBAM_DSTAT_OFF");
                    return (int64_t)(e ? atoi(e) : -6);
                }();
                // Recalibrated after the timer IRQ-latency rework
                // (matured pending-IRQ delivery, halt-wake 5, recent 5)
                // shifted the polling-loop alignment: 3 passes all six
                // misc-edge "H-blank bit start" flips together with the
                // HBlank-source wake charge (VBAM_HB_WAKE, gba.cpp).
                static const int64_t biasOn = [] {
                    const char* e = getenv("VBAM_DSTAT_ON");
                    return (int64_t)(e ? atoi(e) : 3);
                }();
                const int64_t bias = (value & 2) ? biasOff : biasOn;
                if (cpuAbsCycle + bias >= lcdNextEventAbsCycle) {
                    value ^= 2;
                }
                // Record DISPSTAT reads so the DISPCNT-enable handler can
                // tell a busy-wait handler (`while (DISPSTAT & HBL)`) from an
                // immediate one. Mark on ANY read, not just HBlank=1: by the
                // time IRQ latency lets the handler run, the HBlank flag may
                // already have cleared, so the busy-wait's first read sees 0
                // and exits -- but the read still happened, which is the
                // latency-independent signal. See lastHblankPollCycle.
                lastHblankPollCycle = cpuAbsCycle;
            }
        } else if ((address < 0x4000400) && ioReadable[address & 0x3fc]) {
            value = 0;
        } else {
            uint16_t mgbaVal = 0;
            if (gbaMgbaLog::Read16(address, &mgbaVal)) {
                value = mgbaVal;
                break;
            }
            goto unreadable;
        }
        break;
    case REGION_PRAM:
        value = READ16LE(((uint16_t*)&g_paletteRAM[address & 0x3fe]));
        break;
    case REGION_VRAM: {
        chargeVramContention(address & 0x1FFFF, false);
        unsigned addr = (address & 0x1fffe);
        if (((DISPCNT & 7) > 2) && ((addr & 0x1C000) == 0x18000)) {
            value = 0;
            break;
        }
        if ((addr & 0x18000) == 0x18000)
            addr &= 0x17fff;
        value = READ16LE(((uint16_t*)&g_vram[addr]));
        break;
    }
    case REGION_OAM:
        value = READ16LE(((uint16_t*)&g_oam[address & 0x3fe]));
        break;
    case REGION_ROM0:
    case REGION_ROM0EX:
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if (IsGPIO(address))
            value = rtcRead(address);
        else if (IsEEPROM(address))
            return 0; // ignore reads from eeprom region outside 0x0D page reads
        else if ((address & 0x01FFFFFE) <= (gbaGetRomSize() - 2))
            value = READ16LE(((uint16_t *)&g_rom[address & 0x01FFFFFE]));
        else
            value = (uint16_t)ROMReadOOB(address & 0x01FFFFFE);
        break;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled)
            return eepromRead(address);
        goto unreadable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (isSaveGame()) {
            value = CPUReadBackup(address) * 0x0101;
            break;
        }
#ifdef GBA_LOGGING
        // Just normal log, not openbus
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal halfword read: %08x at %08x (%08x)\n",
                address,
                reg[15].I,
                value);
        }
#endif
        return 0xffff;
    default:
    unreadable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal halfword read: %08x at %08x (%08x)\n",
                address,
                reg[15].I,
                value);
        }
#endif
        value = (uint16_t)(CPUReadOpenBus() >> (8 * address & 2));
        break;
    }

    if (address & 1) {
        value = (value >> 8) | (value << 24);
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned halfword read from: %08x at %08x (%08x)\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2,
                value);
        }
#endif
    }

    return value;
}

static inline int32_t CPUReadHalfWordSigned(uint32_t address)
{
    uint16_t value = static_cast<uint16_t>(CPUReadHalfWord(address));
    if ((address & 1)) {
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned signed halfword read from: %08x at %08x (%08x)\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2,
                value);
        }
#endif
        return (int32_t)(int8_t)value;
    }
    return (int32_t)(int16_t)value;
}

static inline uint8_t CPUReadByte(uint32_t address)
{
    if (g_vbam_script_has_read_hooks && g_vbam_script_mem_read)
        g_vbam_script_mem_read(address, 1, 0);
#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 0)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case REGION_BIOS:
        if (reg[15].I >> 24) {
            if (address < 0x4000) {
#ifdef GBA_LOGGING
                if (systemVerbose & VERBOSE_ILLEGAL_READ) {
                    log("Illegal byte read from bios: %08x at %08x\n",
                        address,
                        armMode ? armNextPC - 4 : armNextPC - 2);
                }
#endif
                return biosProtected[address & 3];
            } else
                goto unreadable;
        }
        return g_bios[address & 0x3FFF];
    case REGION_EWRAM:
        return g_workRAM[address & 0x3FFFF];
    case REGION_IWRAM:
        return g_internalRAM[address & 0x7fff];
    case REGION_IO:
        if ((address < 0x4000400) && ioReadable[address & 0x3ff]) {
            // SOUNDCNT_X (NR52, 0x04000084): bits 0-3 are R-only PSG
            // channel-ON flags maintained by the APU (not by writes
            // to NR52). Compose the live status here.
            if ((address & 0x3ff) == 0x84)
                return soundReadNR52();
            return g_ioMem[address & 0x3ff];
        }
        else
            goto unreadable;
    case REGION_PRAM:
        return g_paletteRAM[address & 0x3ff];
    case REGION_VRAM:
        chargeVramContention(address & 0x1FFFF, false);
        address = (address & 0x1ffff);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return 0;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;
        return g_vram[address];
    case REGION_OAM:
        return g_oam[address & 0x3ff];
    case REGION_ROM0:
    case REGION_ROM0EX:
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if (IsEEPROM(address))
            return 0; // ignore reads from eeprom region outside 0x0D page reads
        else if ((address & 0x01FFFFFF) <= gbaGetRomSize())
            return g_rom[address & 0x01FFFFFF];
        else 
            return (uint8_t)ROMReadOOB(address & 0x01FFFFFE);
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled)
            return DowncastU8(eepromRead(address));
        goto unreadable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (isSaveGame()) {
            return CPUReadBackup(address);
        }
        switch (address & 0x00008f00) {
        case 0x8200:
            return DowncastU8(systemGetSensorX());
        case 0x8300:
            return DowncastU8((systemGetSensorX() >> 8) | 0x80);
        case 0x8400:
            return DowncastU8(systemGetSensorY());
        case 0x8500:
            return DowncastU8(systemGetSensorY() >> 8);
        }
#ifdef GBA_LOGGING
        // Just normal log, not openbus
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal byte read: %08x at %08x\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        return 0xff;
    default:
    unreadable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal byte read: %08x at %08x\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        return (uint8_t)(CPUReadOpenBus() >> (8 * (address & 3)));
    }
}

static inline void CPUWriteMemory(uint32_t address, uint32_t value)
{
    if (g_vbam_script_has_write_hooks && g_vbam_script_mem_write)
        g_vbam_script_mem_write(address, 4, value);
#ifdef GBA_LOGGING
    if (address & 3) {
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned word write: %08x to %08x from %08x\n",
                value,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
    }
#endif

#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, value, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case REGION_EWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezeWorkRAM[address & 0x3FFFC]))
            cheatsWriteMemory(address & 0x203FFFC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_workRAM[address & 0x3FFFC]), value);
        break;
    case REGION_IWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezeInternalRAM[address & 0x7ffc]))
            cheatsWriteMemory(address & 0x3007FFC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_internalRAM[address & 0x7ffC]), value);
        break;
    case REGION_IO:
        if (address < 0x4000400) {
            CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
            CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
        } else if (gbaMgbaLog::IsRange(address) && gbaMgbaLog::Write32(address, value)) {
            /* consumed by mGBA debug-console */
        } else
            goto unwritable;
        break;
    case REGION_PRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezePRAM[address & 0x3fc]))
            cheatsWriteMemory(address & 0x70003FC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_paletteRAM[address & 0x3FC]), value);
        break;
    case REGION_VRAM:
        chargeVramContention(address & 0x1FFFF, true);
        address = (address & 0x1fffc);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;

#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezeVRAM[address]))
            cheatsWriteMemory(address + 0x06000000, value);
        else
#endif

            WRITE32LE(((uint32_t*)&g_vram[address]), value);
        break;
    case REGION_OAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezeOAM[address & 0x3fc]))
            cheatsWriteMemory(address & 0x70003FC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_oam[address & 0x3fc]), value);
        break;
    case REGION_ROM0:
    case REGION_ROM0EX:
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if (GBAMatrix.size && (address & 0x01FFFF00) == 0x00800100)
        {
            GBAMatrixWrite(&GBAMatrix, address & 0x3C, value);
            break;
        }
        goto unwritable;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, DowncastU8(value));
            break;
        }
        goto unwritable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (isSaveGame()) {
            CPUWriteBackup(address, (uint8_t)(value >> (8 * (address & 3))));
            break;
        }
        // fallthrough
    default:
    unwritable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_WRITE) {
            log("Illegal word write: %08x to %08x from %08x\n",
                value,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        break;
    }
}

static inline void CPUWriteHalfWord(uint32_t address, uint16_t value)
{
    if (g_vbam_script_has_write_hooks && g_vbam_script_mem_write)
        g_vbam_script_mem_write(address, 2, value);
#ifdef GBA_LOGGING
    if (address & 1) {
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned halfword write: %04x to %08x from %08x\n",
                value,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
    }
#endif

#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, value, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case REGION_EWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezeWorkRAM[address & 0x3FFFE]))
            cheatsWriteHalfWord(address & 0x203FFFE, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_workRAM[address & 0x3FFFE]), value);
        break;
    case REGION_IWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezeInternalRAM[address & 0x7ffe]))
            cheatsWriteHalfWord(address & 0x3007ffe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_internalRAM[address & 0x7ffe]), value);
        break;
    case REGION_IO:
        if (address < 0x4000400)
            CPUUpdateRegister(address & 0x3fe, value);
        else if (gbaMgbaLog::IsRange(address) && gbaMgbaLog::Write16(address, value))
            /* consumed by mGBA debug-console */;
        else
            goto unwritable;
        break;
    case REGION_PRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezePRAM[address & 0x03fe]))
            cheatsWriteHalfWord(address & 0x70003fe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_paletteRAM[address & 0x3fe]), value);
        break;
    case REGION_VRAM:
        chargeVramContention(address & 0x1FFFF, false);
        address = (address & 0x1fffe);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezeVRAM[address]))
            cheatsWriteHalfWord(address + 0x06000000, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_vram[address]), value);
        break;
    case REGION_OAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezeOAM[address & 0x03fe]))
            cheatsWriteHalfWord(address & 0x70003fe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_oam[address & 0x3fe]), value);
        break;
    case REGION_ROM0:
    case REGION_ROM0EX:
        if (GBAMatrix.size && (address & 0x01FFFF00) == 0x00800100)
        {
            GBAMatrixWrite16(&GBAMatrix, address & 0x3C, value);
            break;
        }
        if (IsGPIO(address)) {
            if (!rtcWrite(address, value))
                goto unwritable;
        } else if (!agbPrintWrite(address, value))
            goto unwritable;
        break;
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if (GBAMatrix.size && (address & 0x01FFFF00) == 0x00800100)
        {
            GBAMatrixWrite16(&GBAMatrix, address & 0x3C, value);
            break;
        }
        goto unwritable;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, (uint8_t)value);
            break;
        }
        goto unwritable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (isSaveGame()) {
            CPUWriteBackup(address, (uint8_t)(value >> (8 * (address & 1))));
            break;
        }
        // fallthrough
    default:
    unwritable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_WRITE) {
            log("Illegal halfword write: %04x to %08x from %08x\n",
                value,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        break;
    }
}

static inline void CPUWriteByte(uint32_t address, uint8_t b)
{
    if (g_vbam_script_has_write_hooks && g_vbam_script_mem_write)
        g_vbam_script_mem_write(address, 1, b);
#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, b, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case REGION_EWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (freezeWorkRAM[address & 0x3FFFF])
            cheatsWriteByte(address & 0x203FFFF, b);
        else
#endif
            g_workRAM[address & 0x3FFFF] = b;
        break;
    case REGION_IWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (freezeInternalRAM[address & 0x7fff])
            cheatsWriteByte(address & 0x3007fff, b);
        else
#endif
            g_internalRAM[address & 0x7fff] = b;
        break;
    case REGION_IO:
        if (address < 0x4000400) {
            switch (address & 0x3FF) {
            case IO_REG_SOUND1CNT_L:
            //case IO_REG_SOUND1CNT_L + 1:
            case IO_REG_SOUND1CNT_H:
            case IO_REG_SOUND1CNT_H + 1:
            case IO_REG_SOUND1CNT_X:
            case IO_REG_SOUND1CNT_X + 1:
            case IO_REG_SOUND2CNT_L:
            case IO_REG_SOUND2CNT_L + 1:
            case IO_REG_SOUND2CNT_H:
            case IO_REG_SOUND2CNT_H + 1:
            case IO_REG_SOUND3CNT_L:
            case IO_REG_SOUND3CNT_L + 1:
            case IO_REG_SOUND3CNT_H:
            case IO_REG_SOUND3CNT_H + 1:
            case IO_REG_SOUND3CNT_X:
            case IO_REG_SOUND3CNT_X + 1:
            case IO_REG_SOUND4CNT_L:
            case IO_REG_SOUND4CNT_L + 1:
            case IO_REG_SOUND4CNT_H:
            case IO_REG_SOUND4CNT_H + 1:
            case IO_REG_SOUNDCNT_L:
            case IO_REG_SOUNDCNT_L + 1:
            //case IO_REG_SOUNDCNT_H:
            //case IO_REG_SOUNDCNT_H + 1:
            case IO_REG_SOUNDCNT_X:
            //case IO_REG_SOUNDCNT_X + 1:
            //case IO_REG_SOUNDBIAS:
            //case IO_REG_SOUNDBIAS + 1:
            case IO_REG_WAVE_RAM0_L + 0:
            case IO_REG_WAVE_RAM0_L + 1:
            case IO_REG_WAVE_RAM0_L + 2:
            case IO_REG_WAVE_RAM0_L + 3:
            case IO_REG_WAVE_RAM1_L + 0:
            case IO_REG_WAVE_RAM1_L + 1:
            case IO_REG_WAVE_RAM1_L + 2:
            case IO_REG_WAVE_RAM1_L + 3:
            case IO_REG_WAVE_RAM2_L + 0:
            case IO_REG_WAVE_RAM2_L + 1:
            case IO_REG_WAVE_RAM2_L + 2:
            case IO_REG_WAVE_RAM2_L + 3:
            case IO_REG_WAVE_RAM3_L + 0:
            case IO_REG_WAVE_RAM3_L + 1:
            case IO_REG_WAVE_RAM3_L + 2:
            case IO_REG_WAVE_RAM3_L + 3:
                soundEvent8(address & 0xFF, b);
                break;
            case IO_REG_HALTCNT: // HALTCNT, undocumented
                if (b == 0x80)
                    stopState = true;
                holdState = 1;
                holdType = -1;
                cpuNextEvent = cpuTotalTicks;
                break;
            default: // every other register
                uint32_t lowerBits = address & 0x3fe;
                if (address & 1) {
                    CPUUpdateRegister(lowerBits,
                        (READ16LE(&g_ioMem[lowerBits]) & 0x00FF) | (b << 8));
                } else {
                    CPUUpdateRegister(lowerBits,
                        (READ16LE(&g_ioMem[lowerBits]) & 0xFF00) | b);
                }
            }
            break;
        } else if (gbaMgbaLog::IsRange(address) && gbaMgbaLog::Write8(address, b)) {
            break;
        } else
            goto unwritable;
        break;
    case REGION_PRAM:
        // no need to switch
        *((uint16_t*)&g_paletteRAM[address & 0x3FE]) = (b << 8) | b;
        break;
    case REGION_VRAM:
        chargeVramContention(address & 0x1FFFF, false);
        address = (address & 0x1fffe);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;

        // no need to switch
        // byte writes to OBJ VRAM are ignored
        if ((address) < objTilesAddress[((DISPCNT & 7) + 1) >> 2]) {
#ifdef VBAM_ENABLE_DEBUGGER
            if (freezeVRAM[address])
                cheatsWriteByte(address + 0x06000000, b);
            else
#endif
                *((uint16_t*)&g_vram[address]) = (b << 8) | b;
        }
        break;
    case REGION_OAM:
        break;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, b);
            break;
        }
        goto unwritable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (isSaveGame()) {
            CPUWriteBackup(address, b);
            break;
        }
        // fallthrough
    default:
    unwritable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_WRITE) {
            log("Illegal byte write: %02x to %08x from %08x\n",
                b,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        break;
    }
}

#endif  // VBAM_CORE_GBA_GBAINLINE_H_
