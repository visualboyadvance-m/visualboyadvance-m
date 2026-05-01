// Minimal cycle-accurate event scheduler for the GBA core.
//
// Each scheduled event records the absolute cycle (relative to the start of
// the current emulation "epoch") at which it should fire. The CPU loop asks
// the scheduler how many cycles until the next event and runs instructions
// up to that point.
//
// Phase 1 of the scheduler refactor: this coexists with the legacy
// lcdTicks/timer0Ticks/SWITicks/IRQTicks decrement system. CPUUpdateTicks()
// takes min(existing_ticks, scheduler.nextDelta()), and the main loop pops
// expired scheduler events alongside the existing dispatch blocks. Later
// phases will migrate LCD/timers/IRQ to scheduler-native events.
//
// Events are identified by a small integer type (kSchedSio, ...). Only one
// instance of a given type can be pending at a time; scheduling re-queues.

#ifndef VBAM_CORE_GBA_GBASCHEDULER_H_
#define VBAM_CORE_GBA_GBASCHEDULER_H_

#include <cstddef>
#include <cstdint>

enum SchedulerEventType : uint32_t {
    kSchedSio            = 0,  // SIO transfer completion (Normal-8/32 internal)
    kSchedHblankIrqDelay = 1,  // Deferred HBlank IRQ raise. Real HW raises
                               // the HBlank IRQ ~1 cycle after the
                               // DISPSTAT.HBlank bit toggle (bus-arbitration
                               // latency from LCD edge to IRQ controller),
                               // so the scanline period in lcdTicks stays
                               // GBATEK-correct at 1008 + 224 = 1232 while
                               // the IRQ-to-IRQ measurement is 1233.
    // Reserved for later phases:
    // kSchedLcdHdraw, kSchedLcdHblank, kSchedLcdVblank,
    // kSchedTimer0..Timer3, kSchedSwi, kSchedIrq,
    kSchedCount          = 8   // upper bound of event-type values
};

namespace gbaScheduler {

// Reset the scheduler, dropping all pending events and zeroing the cycle
// counter. Call from CPUReset / CPULoadRomData.
void Reset();

// Advance the scheduler's internal time cursor by the given number of
// emulated cycles (the same "clockTicks" unit the CPU uses).
void AdvanceCycles(int32_t cycles);

// Schedule an event of the given type to fire `fromNow` cycles from the
// scheduler's current time. Re-scheduling the same type replaces the
// previous entry.
void Schedule(SchedulerEventType type, int32_t fromNow);

// Cancel any pending event of the given type. No-op if none pending.
void Cancel(SchedulerEventType type);

// True if at least one event is pending.
bool HasPending();

// Cycles until the next event fires (>= 0). Caller must have already
// checked HasPending(); returns INT32_MAX if nothing is scheduled.
int32_t NextDelta();

// Pop and return the next expired event type, advancing the "current"
// cursor to its firing time. Returns kSchedCount and leaves state
// unchanged if the next event is still in the future.
SchedulerEventType PopExpired();

// Save/restore state (for save-state compatibility). Returns the number
// of bytes written / expected.
size_t SaveStateSize();
void SaveState(void* buf);
void LoadState(const void* buf);

} // namespace gbaScheduler

#endif // VBAM_CORE_GBA_GBASCHEDULER_H_
