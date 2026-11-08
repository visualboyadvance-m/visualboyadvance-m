// video_capture_test: DMA3 video-capture mode (start timing 3).
//
//   * Lines 2..161 each trigger one transfer of the programmed count; source
//     and destination continue across lines (dest mode 3 reloads).
//   * The mode is latched at the end of line 162, so a capture armed
//     mid-frame starts at line 2 of the next frame.
//   * Line 162 clears DMA3's enable bit; nothing transfers on it.
//
// Builds a ROM in memory that arms the capture on line 96, then checks VRAM
// and DMA3CNT. Exits nonzero on failure.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/base/file_util.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"

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
void systemCartridgeRumble(bool) {}
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

namespace {

constexpr uint32_t kCodeOff    = 0xC0;   // entry point offset in the ROM
constexpr uint32_t kPatternOff = 0x200;  // 240x160 BGR555 pattern
constexpr int      kW = 240, kH = 160;
constexpr uint32_t kFrameCycles = 280896;

uint16_t patternPixel(int x, int y)
{
    // Every row distinct, so a capture that starts on the wrong line or runs
    // one transfer too many or too few cannot match by accident.
    return (uint16_t)(((x * 7) ^ (y * 131) ^ (y << 10)) & 0x7FFF);
}

// Build the ROM. dmaCnt is the full 32-bit DMA3CNT (count | control << 16).
std::vector<uint8_t> buildRom(uint32_t dmaCnt)
{
    std::vector<uint8_t> rom(kPatternOff + kW * kH * 2 + 0x100, 0);
    auto put32 = [&](uint32_t off, uint32_t v) { memcpy(&rom[off], &v, 4); };

    put32(0, 0xEA000000 | (((kCodeOff - 8) >> 2) & 0xFFFFFF));  // b entry

    static const uint32_t code[] = {
        0xE3A00301,  // mov  r0, #0x04000000
        0xE3A01020,  // mov  r1, #0x20         ; DISPSTAT: VCount IRQ enable
        0xE1C010B4,  // strh r1, [r0, #4]
        0xE3A01B01,  // mov  r1, #0x400
        0xE3811003,  // orr  r1, r1, #3        ; DISPCNT: mode 3, BG2
        0xE1C010B0,  // strh r1, [r0]
        0xE3A00060,  // mov  r0, #96
        0xEB000004,  // bl   wait_line
        0xE28F0034,  // add  r0, pc, #0x34     ; -> pool
        0xE890000F,  // ldm  r0, {r0-r3}       ; &DMA3SAD, src, dst, cnt
        0xE880000E,  // stm  r0, {r1-r3}       ; arm DMA3
        0xEAFFFFFE,  // b    .
        0x00000000,  // (pad)
        // wait_line: halt until VCOUNT == r0
        0xE3A01004,  // mov  r1, #4
        0xE3A02301,  // mov  r2, #0x04000000
        0xE5C20005,  // strb r0, [r2, #5]      ; VCount setting
        0xE2822C02,  // add  r2, r2, #0x200
        0xE1C210B0,  // strh r1, [r2]          ; IE = VCount
        0xEF020000,  // svc  #0x020000         ; Halt
        0xE3A00000,  // mov  r0, #0
        0xE1C200B0,  // strh r0, [r2]          ; IE = 0
        0xE1C210B2,  // strh r1, [r2, #2]      ; ack IF
        0xE12FFF1E,  // bx   lr
    };
    for (size_t i = 0; i < sizeof(code) / 4; i++)
        put32(kCodeOff + 4 * (uint32_t)i, code[i]);
    // pool: the `add r0, pc, #0x34` at index 8 reads pc = entry + 8*4 + 8.
    const uint32_t pool = kCodeOff + 8 * 4 + 8 + 0x34;
    put32(pool + 0, 0x040000D4);
    put32(pool + 4, 0x08000000 + kPatternOff);
    put32(pool + 8, 0x06000000);
    put32(pool + 12, dmaCnt);

    for (int y = 0; y < kH; y++)
        for (int x = 0; x < kW; x++) {
            uint16_t p = patternPixel(x, y);
            memcpy(&rom[kPatternOff + 2 * (y * kW + x)], &p, 2);
        }
    return rom;
}

void boot(const std::vector<uint8_t>& rom)
{
    static bool inited = false;
    if (CPULoadRomData((const char*)rom.data(), (int)rom.size()) == 0) {
        fprintf(stderr, "video_capture_test: synthetic ROM load failed\n");
        exit(2);
    }
    // No save hardware: with the type left on auto, ROM data reads hit the
    // EEPROM window and return 0.
    coreOptions.saveType = GBA_SAVE_NONE;
    CPUInit("", false);
    if (!inited) {
        soundInit();
        inited = true;
    }
    CPUReset();
    emulating = 1;
}

// CPULoop returns at each frame boundary; count frames via systemFrame().
void runFrames(int n)
{
    const int target = g_framesDone + n;
    for (int i = 0; i < n * 4 && g_framesDone < target; i++)
        CPULoop(kFrameCycles);
}

uint16_t vram16(uint32_t off)
{
    uint16_t v;
    memcpy(&v, g_vram + off, 2);
    return v;
}

int gFails = 0;

void expect(bool ok, const char* what)
{
    printf("%-58s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok)
        gFails++;
}

} // namespace

int main()
{
    // Case 1: armed on line 96, destination incrementing. The capture fills
    // VRAM during the following frame.
    {
        boot(buildRom(0xB2000000u | kW));  // enable, timing 3, repeat, 16-bit
        runFrames(4);

        int bad = 0, firstBad = -1;
        for (int i = 0; i < kW * kH; i++)
            if (vram16(2 * i) != patternPixel(i % kW, i / kW)) {
                if (firstBad < 0)
                    firstBad = i;
                bad++;
            }
        if (bad)
            printf("  %d of %d pixels differ; first at row %d col %d\n",
                   bad, kW * kH, firstBad / kW, firstBad % kW);
        expect(bad == 0, "mid-frame arm: 160 rows captured from VCOUNT 2, aligned");
        bool tailClear = true;
        for (int i = 0; i < kW; i++)
            if (vram16(kW * kH * 2 + 2 * i) != 0)
                tailClear = false;
        expect(tailClear, "no transfer on line 162 (row past the frame untouched)");
        uint16_t cnt;
        memcpy(&cnt, g_ioMem + 0xDE, 2);  // DMA3CNT_H as the CPU reads it
        expect((cnt & 0x8000) == 0, "line 162 clears DMA3 enable by itself");
        expect((cnt & 0x3200) == 0x3200, "  ...leaving the rest of DMA3CNT_H intact");
    }

    // Case 2: destination mode 3 (increment + reload). Every line lands on
    // row 0, which ends up holding pattern row 159.
    {
        boot(buildRom(0xB2600000u | kW));
        runFrames(4);

        bool row0 = true, rest = true;
        for (int x = 0; x < kW; x++)
            if (vram16(2 * x) != patternPixel(x, kH - 1))
                row0 = false;
        for (int i = kW; i < kW * kH; i++)
            if (vram16(2 * i) != 0)
                rest = false;
        expect(row0, "dest reload: row 0 holds the 160th line transferred");
        expect(rest, "dest reload: no other row written");
    }

    printf("%s\n", gFails ? "video_capture_test: FAILED" : "video_capture_test: all passed");
    return gFails ? 1 : 0;
}
