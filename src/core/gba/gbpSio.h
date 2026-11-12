/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Game Boy Player SIO handshake and rumble, ported from mGBA
 * src/gba/sio/gbp.c and src/gba/sio/gbp.h.  This file stays under the MPL;
 * MPL-2.0 section 3.3 permits distributing it as part of VBA-M, which is
 * GPL-2.0, so recipients may use it under either license.
 */

#ifndef VBAM_CORE_GBA_GBPSIO_H_
#define VBAM_CORE_GBA_GBPSIO_H_

#include <cstdint>

#if defined(NO_LINK)
#error "This file should not be included with NO_LINK."
#endif  // defined(NO_LINK)

/**
 * Called once per VBlank to detect the Game Boy Player boot screen and enable
 * SIO rumble emulation when present.  Also handles coreOptions.gbpEnabled
 * being turned off while emulation is active, so call it unconditionally.
 */
extern void GbpUpdate();

/**
 * Handle the GBP branch of StartLink.  When the GBP screen has been detected,
 * intercepts NORMAL_32 SIO transfers, decodes rumble commands from the data
 * the game sends, and schedules the transfer's completion through the core's
 * kSchedSio event (CPUScheduleSioCompletionFromNow).
 *
 * @param siocnt The value of SIOCNT being written
 * @return true if GBP consumed the transfer.  When true, the caller must
 *         UPDATE_REG(COMM_SIOCNT, siocnt) and return without falling through
 *         to any other link driver's start path.
 */
extern bool GbpHandleSioStart(uint16_t siocnt);

/**
 * Returns true when the GBP module will consume a Normal SIO transfer started
 * this instruction, i.e. the GBP screen is active and rumble emulation is
 * enabled.  Folded into LinkOwnsNormalSio() so the CPU's SIOCNT-write handler
 * suppresses the generic cycle-accurate SIO completion path (kSchedSio),
 * which would otherwise double-handle the transfer and clobber the GBP
 * handshake response.
 */
extern bool GbpWillHandleSio();

/**
 * Called by the core's kSchedSio completion, before it clears Start and
 * raises the serial IRQ: if the completing transfer is one the GBP started,
 * place the GBP's reply in SIODATA32.  No-op otherwise.
 */
extern void GbpOnSioComplete();

/**
 * Reset all GBP SIO state.  Called from CloseLink so rumble doesn't get stuck
 * on across ROM loads or link-mode transitions.
 */
extern void GbpReset();

/**
 * Enter GBP mode as if the boot logo had just been recognised.  For tests,
 * which cannot draw the logo (its tile data comes from the games).
 */
extern void GbpTestActivate();

#endif  // VBAM_CORE_GBA_GBPSIO_H_
