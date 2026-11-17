/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Game Boy Player SIO handshake and rumble, ported from mGBA
 * src/gba/sio/gbp.c.  This file stays under the MPL; MPL-2.0 section 3.3
 * permits distributing it as part of VBA-M, which is GPL-2.0, so
 * recipients may use it under either license.
 *
 * MurmurHash3 (gbp_murmur3 below) is from mGBA src/util/hash.c, written by
 * Austin Appleby, who placed it in the public domain.
 */

#include "core/gba/gbpSio.h"

#if defined(NO_LINK)
#error "This file should not be compiled with NO_LINK."
#endif  // defined(NO_LINK)

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "core/base/port.h"
#include "core/base/system.h"
#include "core/gba/gba.h"
#include "core/gba/gbaCpu.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaLink.h"

// same thing gbalink.cpp does
#ifdef UPDATE_REG
#undef UPDATE_REG
#endif
#define UPDATE_REG(address, value) WRITE16LE(((uint16_t*)&g_ioMem[address]), value)

namespace {
constexpr uint16_t kSioTransStart = 0x0080;
}  //namespace

// transfer table from gbatek: https://problemkaputt.de/gbatek-gba-gameboy-player.htm
static const uint32_t gbp_tx_data[] = {
    0x0000494E,
    0x0000494E,
    0xB6B1494E,
    0xB6B1544E,
    0xABB1544E,
    0xABB14E45,
    0xB1BA4E45,
    0xB1BA4F44,
    0xB0BB4F44,
    0xB0BB8002,
    0x10000010,
    0x20000013,
    0x30000003,
    0x30000003,
    0x30000003,
    0x30000003,
    0x30000003,
};

// Last usable index into gbp_tx_data.  mGBA stops at its own last entry
// (_gbpTxData[12], 0x30000003) and repeats it; the four trailing copies
// above make the same thing fall out of a plain clamp.
static const int kGbpTxDataLast =
    (int)(sizeof(gbp_tx_data) / sizeof(gbp_tx_data[0])) - 1;

// first 112 bytes of GBA paletteRAM when the GBP boot logo is on screen
static const uint8_t gbp_logo_palette[112] = {
    0xDF, 0xFF, 0x0C, 0x64, 0x0C, 0xE4, 0x2D, 0xE4,
    0x4E, 0x64, 0x4E, 0xE4, 0x6E, 0xE4, 0xAF, 0x68,
    0xB0, 0xE8, 0xD0, 0x68, 0xF0, 0x68, 0x11, 0x69,
    0x11, 0xE9, 0x32, 0x6D, 0x32, 0xED, 0x73, 0xED,
    0x93, 0x6D, 0x94, 0xED, 0xB4, 0x6D, 0xD5, 0xF1,
    0xF5, 0x71, 0xF6, 0xF1, 0x16, 0x72, 0x57, 0x72,
    0x57, 0xF6, 0x78, 0x76, 0x78, 0xF6, 0x99, 0xF6,
    0xB9, 0xF6, 0xD9, 0x76, 0xDA, 0xF6, 0x1B, 0x7B,
    0x1B, 0xFB, 0x3C, 0xFB, 0x5C, 0x7B, 0x7D, 0x7B,
    0x7D, 0xFF, 0x9D, 0x7F, 0xBE, 0x7F, 0xFF, 0x7F,
    0x2D, 0x64, 0x8E, 0x64, 0x8F, 0xE8, 0xF1, 0xE8,
    0x52, 0x6D, 0x73, 0x6D, 0xB4, 0xF1, 0x16, 0xF2,
    0x37, 0x72, 0x98, 0x76, 0xFA, 0x7A, 0xFA, 0xFA,
    0x5C, 0xFB, 0xBE, 0xFF, 0xDE, 0x7F, 0xFF, 0xFF
};

// mgba compares against 0xEEDA6963 but vba-m pre-swaps 16-bit vram values internally
static const uint32_t GBP_LOGO_HASH = 0xE6BC5CD1;

// rumblepong (gba homebrew) seems to draw the gbp logo
// in mode 3. I'm not aware of any retail game that does,
// and mgba doesn't check for it, but it can be enabled
// if needed
#ifdef GBP_DETECT_MODE3
static const uint32_t GBP_LOGO_HASH_MODE3 = 0xc13fffd9;
#endif

static bool gbp_active        = false;
static int  gbp_tx_position   = 0;
// A NORMAL_32 transfer started while the GBP owns the port is in flight: its
// kSchedSio completion is ours to answer (GbpOnSioComplete).
static bool gbp_transfer_pending = false;
static bool gbp_rumble_state  = false;
static int  gbp_inputs_posted = 0;  // cycles 0→1→2→0 each VBlank; at 2 we fake directions pressed


// Only read by the GBP_SIO_DEBUG instrumentation.
static uint32_t gbp_transfers_this_frame     = 0;


#ifdef GBP_SIO_DEBUG
static uint32_t gbp_vblank_counter           = 0;
static int      gbp_frames_since_last_irq    = 0;
#endif

// MurmurHash3_x86_32 (seed=0, little-endian), ported from mGBA src/util/hash.c
static uint32_t gbp_murmur3(const uint8_t* data, size_t len)
{
    static const uint32_t c1 = 0xcc9e2d51u;
    static const uint32_t c2 = 0x1b873593u;

    uint32_t h1 = 0;
    const int nblocks = (int)(len / 4);

    for (int i = -nblocks; i; i++) {
        uint32_t k1;
        memcpy(&k1, data + ((size_t)(nblocks + i) * 4), 4);
        // little-endian: no byte-swap needed on LE hosts; for BE hosts we swap
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        k1 = ((k1 >> 24) & 0xFF) | ((k1 >> 8) & 0xFF00) |
             ((k1 << 8) & 0xFF0000) | ((k1 << 24) & 0xFF000000u);
#endif
        k1 *= c1;
        k1  = (k1 << 15) | (k1 >> 17);
        k1 *= c2;

        h1 ^= k1;
        h1  = (h1 << 13) | (h1 >> 19);
        h1  = h1 * 5 + 0xe6546b64u;
    }

    // tail bytes
    const uint8_t* tail = data + (size_t)nblocks * 4;
    uint32_t k1 = 0;
    switch (len & 3) {
    case 3: k1 ^= (uint32_t)tail[2] << 16; /* fall through */
    case 2: k1 ^= (uint32_t)tail[1] << 8;  /* fall through */
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1  = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
    }

    h1 ^= (uint32_t)len;
    // fmix32
    h1 ^= h1 >> 16; h1 *= 0x85ebca6bu;
    h1 ^= h1 >> 13; h1 *= 0xc2b2ae35u;
    h1 ^= h1 >> 16;
    return h1;
}

static void GbpRumbleSet(bool enable)
{
    if (gbp_rumble_state == enable)
        return;
    gbp_rumble_state = enable;
    systemCartridgeRumble(enable);
#ifdef GBP_SIO_DEBUG
    fprintf(stderr, "  >> rumble %s (VBlank %u, tx%u) <<\n",
            enable ? "ON" : "OFF", gbp_vblank_counter, gbp_transfers_this_frame + 1);
#endif
}

// is the gba currently displaying the gbp logo?
static bool CheckGBPScreen()
{
    if (!g_paletteRAM || !g_vram)
        return false;

#ifdef GBP_DETECT_MODE3
    uint16_t dispcnt = READ16LE(&g_ioMem[IO_REG_DISPCNT]);
    if ((dispcnt & 0x7) == 3) {
        return gbp_murmur3(&g_vram[0x4000], 0x4000) == GBP_LOGO_HASH_MODE3;
    }
#endif
    // Hash the VRAM map region only after the cheaper palette compare passes.
    if (memcmp(g_paletteRAM, gbp_logo_palette, sizeof(gbp_logo_palette)) != 0)
        return false;
    return gbp_murmur3(&g_vram[0x4000], 0x4000) == GBP_LOGO_HASH;
}

void GbpUpdate()
{
    // Switched off from the UI or the command line partway through a session:
    // drop out of the handshake and let go of rumble instead of freezing in
    // whatever state we were in.
    if (!coreOptions.gbpEnabled) {
        if (gbp_active)
            GbpReset();
        return;
    }

    // real gbp doesn't use the internal link if anything is connected to the external serial
    if (GetLinkMode() != LINK_DISCONNECTED) {
        if (gbp_active)
            GbpReset();
        return;
    }

    if (CheckGBPScreen()) {
        if (!gbp_active) {
#ifdef GBP_SIO_DEBUG
            uint16_t dispcnt = READ16LE(&g_ioMem[IO_REG_DISPCNT]);
            fprintf(stderr,
                    "  >> GBP active=true (DISPCNT mode=%d) <<\n",
                    dispcnt & 0x7);
#endif
            gbp_active        = true;
            gbp_tx_position   = 0;
            gbp_inputs_posted = 0;
        }

        gbp_inputs_posted = (gbp_inputs_posted + 1) % 3; //0 → 1 → 2 → 0
        if (gbp_inputs_posted == 2) {
            // The boot screen wants the d-pad held every third frame.  Add
            // just those four bits and leave the rest of P1 alone: CPULoop
            // has already refreshed it from the host pad at the top of this
            // same VBlank, and mGBA likewise ORs its key callback into the
            // real input rather than replacing it (_gbpRead returns 0xF0).
            // Assigning a full 0x03FF on the other two frames threw the
            // player's own input away two frames out of three.
            P1 = (uint16_t)(P1 & ~0x00F0u);  // 0 = pressed
            UPDATE_REG(IO_REG_KEYINPUT, P1);
        }
    } else if (gbp_inputs_posted != 0) {
        // logo gone -- stop forcing; the next VBlank's joypad read restores P1
        gbp_inputs_posted = 0;
    }

    // 12 step nintendo handshake then rumble data rx
    if (gbp_active) {
        gbp_tx_position = 0;
#ifdef GBP_SIO_DEBUG
        ++gbp_vblank_counter;
        fprintf(stderr, "== VBlank %u ==\n", gbp_vblank_counter);

        // stall watchdog + transfer-rate sampler
        {
            if (gbp_transfers_this_frame == 0) {
                ++gbp_frames_since_last_irq;
                if (gbp_frames_since_last_irq == 30) {
                    fprintf(stderr,
                            "  >> STALL: %d frames with no transfers (VBlank %u) <<\n",
                            gbp_frames_since_last_irq, gbp_vblank_counter);
                }
            } else {
                if (gbp_frames_since_last_irq >= 30) {
                    fprintf(stderr,
                            "  >> STALL RECOVERED after %d frames (VBlank %u) <<\n",
                            gbp_frames_since_last_irq, gbp_vblank_counter);
                }
                gbp_frames_since_last_irq = 0;
            }
        }
#endif
        gbp_transfers_this_frame = 0;
    }
}

bool GbpWillHandleSio()
{
    //same logic as GbpHandleSioStart
    return gbp_active && coreOptions.gbpEnabled;
}

bool GbpHandleSioStart(uint16_t siocnt)
{
    if (!gbp_active || !coreOptions.gbpEnabled)
        return false;

    // NORMAL_32 mode test, equivalent to GetSIOMode() == NORMAL32 in gbaLink.cpp:
    //   RCNT bit 15 clear, SIOCNT bits 13:12 == 0b01.
    uint16_t rcnt = READ16LE(&g_ioMem[COMM_RCNT]);
    bool isNormal32 = !(rcnt & 0x8000) && ((siocnt & 0x3000) == 0x1000);

    // Every SIOCNT write with START=1 (re)starts the transfer, as mGBA's
    // _startTransfer() does (src/gba/sio.c).
    if (isNormal32 && (siocnt & kSioTransStart)) {
        uint32_t rx = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
#ifdef GBP_SIO_DEBUG
        {
            int      clamped     = gbp_tx_position > 16 ? 16 : gbp_tx_position;
            uint32_t tx_will_send = gbp_tx_data[clamped];
            fprintf(stderr,
                    "  arm  tx%u pos=%d siocnt=0x%04X rcnt=0x%04X rx=0x%08X "
                    "tx_will_send=0x%08X ticks=%d\n",
                    gbp_transfers_this_frame + 1, gbp_tx_position, siocnt, rcnt, rx,
                    tx_will_send, cpuTotalTicks);
        }
#endif
        if (gbp_tx_position >= 12) {
            // 0x22 in bits [1:0] and [5:4] = rumble on; 0x00/0x11 = off,
            // decoded at Start as mGBA's _gbpSioStart() does.
            GbpRumbleSet((rx & 0x33u) == 0x22u);
        }

        // The GBP clocks the transfer (SIOCNT bit 0 clear: external clock).
        // Its clock is taken to run at 256 kHz, the GBA's slow serial rate,
        // as mGBA does for any external clock: 32 bits, 2048 cycles.
        // kSchedSio completes the transfer and raises the serial IRQ, and
        // GbpOnSioComplete() supplies the reply.  Scheduling again replaces
        // a pending completion, as a restart does on the GBP.
        gbp_transfer_pending = true;
        CPUScheduleSioCompletionFromNow(CPUSioNormalTransferCycles(32, false));
    }
    // always return true so we don't fall through to the normal SIO handler
    return true;
}

void GbpOnSioComplete()
{
    if (!gbp_transfer_pending)
        return;
    gbp_transfer_pending = false;

    // Stood down mid-transfer, or the game cleared Start itself: the core
    // finishes (or has finished) the transfer without a reply from us.
    if (!gbp_active || !(READ16LE(&g_ioMem[COMM_SIOCNT]) & kSioTransStart))
        return;

    // mGBA wraps to 0 past its last entry, but it also resets txPosition
    // every VBlank, as GbpUpdate() does above, so the wrap is unreachable and
    // a clamp is equivalent.
    int pos = gbp_tx_position;
    if (pos > kGbpTxDataLast) {
        pos = kGbpTxDataLast;
    }
    uint32_t tx = gbp_tx_data[pos];
#ifdef GBP_SIO_DEBUG
    {
        uint32_t rx_at_fire = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
        fprintf(stderr,
                "  fire tx%u pos=%d rx=0x%08X tx=0x%08X ticks=%d\n",
                gbp_transfers_this_frame + 1, pos, rx_at_fire, tx, cpuTotalTicks);
    }
#endif
    if (gbp_tx_position < kGbpTxDataLast) {
        ++gbp_tx_position;
    }
    UPDATE_REG(COMM_SIODATA32_L, (uint16_t)(tx & 0xFFFFu));
    UPDATE_REG(COMM_SIODATA32_H, (uint16_t)(tx >> 16));
    gbp_transfers_this_frame++;
}

void GbpReset()
{
    gbp_active               = false;
    gbp_tx_position          = 0;
    gbp_transfer_pending     = false;
    gbp_inputs_posted        = 0;
    gbp_transfers_this_frame = 0;
    GbpRumbleSet(false);
}

void GbpTestActivate()
{
    GbpReset();
    gbp_active = true;
}
