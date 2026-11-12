// gbp_sio_test: Game Boy Player serial transfers go through the core's
// kSchedSio completion.
//
// In GBP mode the GBP clocks each NORMAL_32 transfer (SIOCNT bit 0 clear),
// taking about 2048 cycles; the serial IRQ is raised at completion with the
// GBP's reply in SIODATA32, following the handshake table, and once past the
// handshake the game's word selects rumble ((rx & 0x33) == 0x22 is on).
//
// GBP mode is entered through GbpTestActivate(), as a test cannot draw the
// boot logo. Exits nonzero on failure.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/base/file_util.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "core/gba/gbpSio.h"

// ---- frontend stubs ----
struct CoreOptions coreOptions;

void systemMessage(int, const char*, ...) {}
void log(const char*, ...) {}
bool systemPauseOnFrame() { return false; }
void systemGbPrint(uint8_t*, int, int, int, int, int) {}
void systemScreenCapture(int) {}
void systemDrawScreen() {}
void systemSendScreen() {}
bool systemReadJoypads() { return true; }
uint32_t systemReadJoypad(int) { return 0; }
uint32_t systemGetClock() { return 0; }
void systemSetTitle(const char*) {}
namespace {
class NullSoundDriver : public SoundDriver {
  public:
    bool init(long) override { return true; }
    void pause() override {}
    void reset() override {}
    void resume() override {}
    void write(uint16_t*, int) override {}
    void setThrottle(unsigned short) override {}
};
} // namespace
std::unique_ptr<SoundDriver> systemSoundInit() {
    return std::unique_ptr<SoundDriver>(new NullSoundDriver);
}
void systemOnWriteDataToSoundBuffer(const uint16_t*, int) {}
void systemOnSoundShutdown() {}
void systemScreenMessage(const char*) {}
void systemUpdateMotionSensor() {}
int systemGetSensorX() { return 0; }
int systemGetSensorY() { return 0; }
int systemGetSensorZ() { return 0; }
uint8_t systemGetSensorDarkness() { return 0; }
static int g_rumble = -1;
void systemCartridgeRumble(bool on) { g_rumble = on; }
void systemPossibleCartridgeRumble(bool) {}
void updateRumbleFrame() {}
bool systemCanChangeSoundQuality() { return false; }
void systemShowSpeed(int) {}
void system10Frames() {}
static int g_framesDone = 0;
void systemFrame() { ++g_framesDone; }
void systemGbBorderOn() {}
void (*dbgOutput)(const char* s, uint32_t addr) = nullptr;
void (*dbgSignal)(int sig, int number) = nullptr;

uint8_t  systemColorMap8[0x10000];
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
uint16_t systemGbPalette[24];
int systemRedShift = 0;
int systemGreenShift = 0;
int systemBlueShift = 0;
int systemColorDepth = 32;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = 0;
int systemSpeed = 0;
int emulating = 0;


extern int64_t cpuAbsCycle;

namespace {

int gFails = 0;

void expect(bool ok, const char* what)
{
    printf("%-60s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok)
        gFails++;
}

uint16_t io16(uint32_t off)
{
    uint16_t v;
    memcpy(&v, g_ioMem + off, 2);
    return v;
}

uint32_t io32(uint32_t off)
{
    uint32_t v;
    memcpy(&v, g_ioMem + off, 4);
    return v;
}

void boot()
{
    // The CPU does the serial writes, as a game would: it waits for the test
    // to set a "go" word in IWRAM, copies the test's send word into
    // SIODATA32, starts a NORMAL_32 external-clock transfer with the serial
    // IRQ enabled, and waits for Start to clear.
    static const uint32_t code[] = {
        0xE59F0038,  //      ldr  r0, =0x04000128  ; SIOCNT
        0xE59F1038,  //      ldr  r1, =0x5088
        0xE59F3038,  //      ldr  r3, =0x03000000  ; [0] send word, [4] go
        0xE59F4038,  //      ldr  r4, =0x04000120  ; SIODATA32
        0xE1D020B0,  // idle: ldrh r2, [r0]
        0xE3120080,  //      tst  r2, #0x80
        0x1AFFFFFC,  //      bne  idle
        0xE5936004,  // wait: ldr  r6, [r3, #4]
        0xE3560000,  //      cmp  r6, #0
        0x0AFFFFFC,  //      beq  wait
        0xE3A06000,  //      mov  r6, #0
        0xE5836004,  //      str  r6, [r3, #4]
        0xE5935000,  //      ldr  r5, [r3]
        0xE5845000,  //      str  r5, [r4]
        0xE1C010B0,  //      strh r1, [r0]         ; Start
        0xEAFFFFF3,  //      b    idle
        0x04000128, 0x00005088, 0x03000000, 0x04000120,
    };
    std::vector<uint8_t> rom(0x400, 0);
    memcpy(rom.data(), code, sizeof(code));
    if (CPULoadRomData((const char*)rom.data(), (int)rom.size()) == 0) {
        fprintf(stderr, "gbp_sio_test: ROM load failed\n");
        exit(2);
    }
    coreOptions.saveType = GBA_SAVE_NONE;
    CPUInit("", false);
    soundInit();
    CPUReset();
    emulating = 1;
    // Normal serial mode, as a game selects before using SIOCNT.
    CPUUpdateRegister(0x134, 0x0000);
}

// Have the CPU run one transfer sending `rx`; return the cycles from Start
// to completion (-1 if it does not complete within 8192 cycles).
int64_t transfer(uint32_t rx)
{
    const uint32_t go = 1;
    memcpy(g_internalRAM + 0, &rx, 4);
    memcpy(g_internalRAM + 4, &go, 4);
    CPUUpdateRegister(0x202, 0x0080);  // acknowledge the previous serial IRQ

    const int64_t give_up = cpuAbsCycle + 8192;
    while (!(io16(0x128) & 0x0080)) {
        if (cpuAbsCycle > give_up)
            return -1;
        CPULoop(4);
    }
    const int64_t start = cpuAbsCycle;
    while (io16(0x128) & 0x0080) {
        if (cpuAbsCycle - start > 8192)
            return -1;
        CPULoop(4);
    }
    return cpuAbsCycle - start;
}

}  // namespace

int main()
{
    static const uint32_t kHandshake[] = {
        0x0000494E, 0x0000494E, 0xB6B1494E, 0xB6B1544E, 0xABB1544E,
        0xABB14E45, 0xB1BA4E45, 0xB1BA4F44, 0xB0BB4F44, 0xB0BB8002,
        0x10000010, 0x20000013, 0x30000003,
    };

    boot();
    GbpTestActivate();

    bool timed = true, replies = true, irqs = true;
    int64_t lo = 1 << 30, hi = 0;
    for (unsigned i = 0; i < sizeof(kHandshake) / sizeof(kHandshake[0]); ++i) {
        const int64_t t = transfer(0);
        if (t < 0) {
            timed = false;
            break;
        }
        lo = t < lo ? t : lo;
        hi = t > hi ? t : hi;
        if (t < 2048 - 32 || t > 2048 + 64)
            timed = false;
        if (io32(0x120) != kHandshake[i])
            replies = false;
        if (!(io16(0x202) & 0x0080))
            irqs = false;
    }
    printf("  completion after %lld..%lld cycles\n", (long long)lo, (long long)hi);
    expect(timed, "each transfer completes about 2048 cycles after Start");
    expect(replies, "replies follow the GBP handshake table");
    expect(irqs, "each completion raises the serial IRQ");

    // Past the handshake the game's word drives rumble, decoded at Start.
    transfer(0x00000022);
    expect(g_rumble == 1, "(rx & 0x33) == 0x22 turns rumble on");
    transfer(0x00000011);
    expect(g_rumble == 0, "(rx & 0x33) == 0x11 turns rumble off");

    // Out of GBP mode nothing clocks an external-clock transfer, so it does
    // not complete.
    GbpReset();
    expect(transfer(0) < 0, "after GbpReset() the GBP no longer clocks transfers");

    printf("%s\n", gFails ? "gbp_sio_test: FAILED" : "gbp_sio_test: all passed");
    return gFails ? 1 : 0;
}
