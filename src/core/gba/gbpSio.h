#ifndef VBAM_CORE_GBA_GBPSIO_H_
#define VBAM_CORE_GBA_GBPSIO_H_

#include <cstdint>

#if defined(NO_LINK)
#error "This file should not be included with NO_LINK."
#endif  // defined(NO_LINK)

/**
 * Called once per VBlank to detect the Game Boy Player boot screen and enable
 * SIO rumble emulation when present.
 */
extern void GBPUpdate();

/**
 * Returns true while Game Boy Player SIO rumble emulation is active.
 * Used to ensure LinkUpdate is ticked even when no link driver is in use.
 */
extern bool GBPIsActive();

/**
 * Returns the remaining CPU ticks until the in-flight GBP transfer completes,
 * or 0 when no transfer is pending.  Read by the CPU event scheduler to clip
 * cpuNextEvent so the transfer fires on time.
 */
extern int GBPTransferEnd();

/**
 * Handle the GBP branch of StartLink.  When the GBP screen has been detected,
 * intercepts NORMAL_32 SIO transfers, decodes rumble commands from the data
 * the game sends, anchors the transfer deadline, and clips cpuNextEvent to it.
 *
 * The cpuNextEvent reference parameter is load-bearing: the deadline anchor
 * and the cpuNextEvent clip must remain atomic with one another.  See the
 * comment block inside the implementation for the full rationale.
 *
 * @param siocnt          The value of SIOCNT being written
 * @param cpuNextEventRef Reference to the CPU event scheduler's deadline
 * @return true if GBP consumed the transfer.  When true, the caller must
 *         UPDATE_REG(COMM_SIOCNT, siocnt) and return without falling through
 *         to any other link driver's start path.
 */
extern bool GbpHandleSioStart(uint16_t siocnt, int& cpuNextEventRef);

/**
 * Returns true when the GBP module will consume a Normal SIO transfer started
 * this instruction, i.e. the GBP screen is active and rumble emulation is
 * enabled.  Used by the CPU's SIOCNT-write handler to suppress the generic
 * cycle-accurate SIO completion path (kSchedSio), which would otherwise
 * double-handle the transfer and clobber the GBP handshake response.
 *
 * Unlike GBPIsActive() (which is true only while a transfer is in flight),
 * this reflects whether GBP owns the transfer at the START edge.
 */
extern bool GbpWillHandleSio();

/**
 * Handle the GBP branch of LinkUpdate.  Ticks the transfer countdown, fires
 * the SIO completion IRQ when it expires, and (in GBP_SIO_DEBUG builds)
 * samples the game's SIO-FSM state.  Safe to call unconditionally — early-outs
 * when no GBP transfer is pending.
 */
extern void GbpLinkUpdateTick(int ticks);

/**
 * Reset all GBP SIO state.  Called from CloseLink so rumble doesn't get stuck
 * on across ROM loads or link-mode transitions.
 */
extern void GbpReset();

#endif  // VBAM_CORE_GBA_GBPSIO_H_
