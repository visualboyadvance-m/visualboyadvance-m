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
constexpr uint16_t kSioIrqEnable  = 0x4000;

constexpr int kGbpTransfersPerFrame = 17;

// mgba uses 2048
constexpr int kGbpInterTransferCycles = 2048;
}  //namespace

// ported from mGBA src/gba/sio/gbp.c
// MurmurHash3 from mGBA src/util/hash.c, originally by Austin Appleby, public domain

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
static int  gbp_transfer_end  = 0;  // countdown in CPU ticks until transfer done
static bool gbp_rumble_state  = false;
static int  gbp_inputs_posted = 0;  // cycles 0→1→2→0 each VBlank; at 2 we fake directions pressed


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

void GBPUpdate()
{
    // real gbp doesn't use the internal link if anything is connected to the external serial
    if (GetLinkMode() != LINK_DISCONNECTED) {
        if (gbp_active) {
            gbp_active        = false;
            gbp_transfer_end  = 0;
            gbp_inputs_posted = 0;
            GbpRumbleSet(false);
            P1 = 0x03FF;
            UPDATE_REG(IO_REG_KEYINPUT, P1);
        }
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
            // clear bits 4-7 (Right/Left/Up/Down) = all pressed.
            P1 = P1 & ~0x00F0u;
        } else {
            P1 = 0x03FF;  //nothing pressed
        }
        UPDATE_REG(IO_REG_KEYINPUT, P1);
    } else if (gbp_inputs_posted != 0) {
        // logo gone
        gbp_inputs_posted = 0;
        P1 = 0x03FF;
        UPDATE_REG(IO_REG_KEYINPUT, P1);
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

bool GBPIsActive()
{
    return gbp_transfer_end > 0;
}

bool GbpWillHandleSio()
{
    //same logic as GbpHandleSioStart
    return gbp_active && coreOptions.gbpEnabled;
}

int GBPTransferEnd()
{
    return gbp_transfer_end;
}

bool GbpHandleSioStart(uint16_t siocnt, int& cpuNextEventRef)
{
    if (!gbp_active || !coreOptions.gbpEnabled)
        return false;

    // NORMAL_32 mode test, equivalent to GetSIOMode() == NORMAL32 in gbaLink.cpp:
    //   RCNT bit 15 clear, SIOCNT bits 13:12 == 0b01.
    uint16_t rcnt = READ16LE(&g_ioMem[COMM_RCNT]);
    bool isNormal32 = !(rcnt & 0x8000) && ((siocnt & 0x3000) == 0x1000);

    // match mGBA 0.10.5 (src/gba/sio/gbp.c _gbpSioWriteRegister): every
    // SIOCNT write with START=1 unconditionally reschedules the transfer
    // (mTimingDeschedule + mTimingSchedule(2048)).  
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
            // 0x22 in bits [1:0] and [5:4] = rumble on; 0x00/0x11 = off.
            // matches mGBA 0.10.5
            // (src/gba/sio/gbp.c _gbpSioWriteRegister)
            GbpRumbleSet((rx & 0x33u) == 0x22u);
        }

        // match mGBA's 2048 cycles
        gbp_transfer_end = kGbpInterTransferCycles + cpuTotalTicks;
        // Truncate the current CPULoop sub-period to the transfer
        // deadline.  Without this, the gba.cpp event-block clip at
        // GBPTransferEnd() only takes effect *for the next* sub-period
        // — whatever older event sized the current one decides when the
        // GBP transfer can actually fire, producing variable late
        // transfers and the observed SIO state-machine stalls
        // (long stretches of state_at_fire == 0 while the pattern
        // engine re-asserts level=2).  Mirrors the role of
        // mTimingSchedule's priority-queue insertion in mGBA.
        int rel = gbp_transfer_end - cpuTotalTicks;
        if (rel < cpuNextEventRef) {
            cpuNextEventRef = rel;
        }
    }
    // always return true so we don't fall through to the normal SIO handler
    return true;
}

void GbpLinkUpdateTick(int ticks)
{
    // cap to 17 transfers per frame, as stated on gbatek
    if (gbp_active && gbp_transfer_end > 0 &&
        gbp_transfers_this_frame < (uint32_t)kGbpTransfersPerFrame) {
        gbp_transfer_end -= ticks;
        if (gbp_transfer_end <= 0) {
            gbp_transfer_end = 0;

            // mgba wraps around to 0, should we do the same here?
            int pos = gbp_tx_position;
            if (pos > kGbpTransfersPerFrame - 1) {
                pos = kGbpTransfersPerFrame - 1;
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
            if (gbp_tx_position < kGbpTransfersPerFrame - 1) {
                ++gbp_tx_position;
            }
            UPDATE_REG(COMM_SIODATA32_L, (uint16_t)(tx & 0xFFFFu));
            UPDATE_REG(COMM_SIODATA32_H, (uint16_t)(tx >> 16));
            uint16_t cnt = READ16LE(&g_ioMem[COMM_SIOCNT]) & (uint16_t)~kSioTransStart;
            UPDATE_REG(COMM_SIOCNT, cnt);
            if (cnt & kSioIrqEnable) {
                IF |= 0x80;
                UPDATE_REG(IO_REG_IF, IF);
                // the scheduler rewrite removed the
                // end-of-CPULoop (IF & IE) check, so raising IF alone no
                // longer delivers the serial IRQ -- it must be routed
                // through the kSchedIrq scheduler, same as
                // gbaScheduler_OnSioComplete().
                CPUReevaluateIRQ();
            }

            gbp_transfers_this_frame++;
        }
    }
}

void GbpReset()
{
    gbp_active        = false;
    gbp_tx_position   = 0;
    gbp_transfer_end  = 0;
    gbp_inputs_posted = 0;
    GbpRumbleSet(false);
}
