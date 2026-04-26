// Headless runner for mGBA's test suite (suite.gba).
//
// Drives the emulator through every sub-suite by injecting synthetic keypad
// input, then reads IWRAM to report per-suite pass/total counts and dumps the
// SRAM log at the end.
//
// Usage: suite_runner [path/to/suite.gba]
// Default ROM path: /Users/andyvand/Downloads/suite-master/suite.gba

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

#include "core/base/system.h"
#include "core/base/sound_driver.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaSound.h"

// ---- System-callback stubs (must be provided by the embedder) --------------

static uint32_t g_joy_mask = 0;

// Core expects the embedder to instantiate this.
struct CoreOptions coreOptions;

void systemMessage(int, const char*, ...) {}
void log(const char*, ...) {}
bool systemPauseOnFrame() { return false; }
void systemGbPrint(uint8_t*, int, int, int, int, int) {}
void systemScreenCapture(int) {}
void systemDrawScreen() {}
void systemSendScreen() {}
bool systemReadJoypads() { return true; }
uint32_t systemReadJoypad(int) { return g_joy_mask; }
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
void systemFrame() {}
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

// ---- GBA joypad bits (what systemReadJoypad returns in active-high) --------
static constexpr uint32_t KEY_A      = 1u << 0;
static constexpr uint32_t KEY_B      = 1u << 1;
//static constexpr uint32_t KEY_SELECT = 1u << 2;
//static constexpr uint32_t KEY_START  = 1u << 3;
//static constexpr uint32_t KEY_RIGHT  = 1u << 4;
//static constexpr uint32_t KEY_LEFT   = 1u << 5;
//static constexpr uint32_t KEY_UP     = 1u << 6;
static constexpr uint32_t KEY_DOWN   = 1u << 7;

// ---- Memory helpers --------------------------------------------------------
//
// These are GBA-address-space safe: they dispatch on the top nibble of the
// address and return 0 for any region the runner doesn't need to read. This
// keeps the harness from segfaulting when a TestSuite struct's `.passes`
// pointer lives in ROM (the video suite does this — both pointers aim at a
// `constZero` in .rodata). Previously the blanket "subtract 0x03000000"
// deref produced a wild out-of-bounds into g_internalRAM for those reads
// and LLDB trapped in iwram_read32.

static uint32_t mem_read32(uint32_t addr) {
    switch (addr >> 24) {
    case 0x02: { // EWRAM
        uint32_t off = (addr - 0x02000000u) & 0x3FFFFu;
        if (!g_workRAM) return 0;
        return (uint32_t)g_workRAM[off] |
               ((uint32_t)g_workRAM[off + 1] << 8) |
               ((uint32_t)g_workRAM[off + 2] << 16) |
               ((uint32_t)g_workRAM[off + 3] << 24);
    }
    case 0x03: { // IWRAM
        uint32_t off = (addr - 0x03000000u) & 0x7FFFu;
        if (!g_internalRAM) return 0;
        return (uint32_t)g_internalRAM[off] |
               ((uint32_t)g_internalRAM[off + 1] << 8) |
               ((uint32_t)g_internalRAM[off + 2] << 16) |
               ((uint32_t)g_internalRAM[off + 3] << 24);
    }
    case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x0C: case 0x0D: { // ROM mirrors
        uint32_t off = (addr - 0x08000000u) & 0x01FFFFFFu;
        if (!g_rom) return 0;
        return (uint32_t)g_rom[off] |
               ((uint32_t)g_rom[off + 1] << 8) |
               ((uint32_t)g_rom[off + 2] << 16) |
               ((uint32_t)g_rom[off + 3] << 24);
    }
    default:
        return 0;
    }
}

#if 0
static int32_t mem_read_i32(uint32_t addr) {
    return (int32_t)mem_read32(addr);
}
#endif

// Backwards-compat wrappers used by the rest of the runner.
static uint32_t iwram_read32(uint32_t addr)   { return mem_read32(addr); }

#if 0
static int32_t  iwram_read_i32(uint32_t addr) { return mem_read_i32(addr); }
#endif

static uint32_t rom_read32(uint32_t addr)     { return mem_read32(addr); }

// ---- Suite table -----------------------------------------------------------
//
// These TestSuite struct addresses were found with arm-none-eabi-objdump -t on
// suite.elf. The definition order (in main.c `suites[]`) is the order we
// navigate through by pressing DOWN.
struct SuiteDesc {
    const char* name;
    uint32_t struct_addr; // TestSuite struct in ROM
};

static const SuiteDesc kSuites[] = {
    {"memory",        0x0803d798},
    {"io-read",       0x0803cef4},
    {"timing",        0x0804417c},
    {"timers",        0x08043598},
    {"timer-irq",     0x08043480},
    {"shifter",       0x08042664},
    {"carry",         0x0802dd60},
    {"multiply-long", 0x08041cb8},
    {"bios-math",     0x0802cbf4},
    {"dma",           0x0802e258},
    {"sio-read",      0x08042e28},
    {"sio-timing",    0x080433e4},
    {"misc-edge",     0x08041a88},
    {"video",         0x080471ac},
};
static constexpr int kNumSuites = sizeof(kSuites) / sizeof(kSuites[0]);

// Offsets inside the TestSuite struct on ARM (4-byte aligned, 28 bytes):
//   +0  const char* name
//   +4  void (*run)(void)
//   +8  size_t (*list)(...)
//   +12 void (*show)(size_t)
//   +16 const size_t nTests
//   +20 const unsigned* passes
//   +24 const unsigned* totalResults

struct SuiteCounters {
    uint32_t passes_addr;
    uint32_t total_addr;
    uint32_t n_tests;
};

static SuiteCounters read_suite_counters(uint32_t struct_addr) {
    SuiteCounters c;
    c.n_tests      = rom_read32(struct_addr + 16);
    c.passes_addr  = rom_read32(struct_addr + 20);
    c.total_addr   = rom_read32(struct_addr + 24);
    return c;
}

// ---- Emulation driver ------------------------------------------------------

static constexpr int TICKS_PER_FRAME = 280896;

static void run_frames(int n, uint32_t joy = 0) {
    g_joy_mask = joy;
    for (int i = 0; i < n; ++i) {
        GBASystem.emuMain(TICKS_PER_FRAME);
    }
}

static void press_button(uint32_t mask, int hold_frames = 6) {
    // Release → press → release so the ROM's keysDown logic fires.
    run_frames(4, 0);
    run_frames(hold_frames, mask);
    run_frames(4, 0);
}

// ---- activeTestInfo in IWRAM ----------------------------------------------
//
// From suite-master/src/main.c:
//   IWRAM_DATA struct ActiveInfo activeTestInfo = { {'I','n','f','o'}, -1, -1, -1 };
// Layout on ARM (packed/aligned, 4-byte members): int32 tag, int32 suiteId,
// int32 testId, int32 subTestId. The .iwram section starts at 0x03000000 and
// the linker places activeTestInfo first after any preceding IWRAM data.
//
// We resolve the address at runtime by scanning IWRAM for the 'Info' tag since
// there can be multiple copies (IWRAM vs shadow). This avoids hardcoding.
static uint32_t g_active_info_addr = 0;

static bool locate_active_info() {
    // `activeTestInfo` is at a fixed IWRAM address in suite.gba (found via
    // `arm-none-eabi-nm suite.elf`). Hardcoding avoids false positives from
    // any other 'Info' ASCII strings that might live in IWRAM — for example
    // the 'I','n','f','o' characters embedded in some message text. The
    // former IWRAM scan returned a bogus address for test 2+ of the video
    // suite because it matched such a copy instead of the struct itself.
    g_active_info_addr = 0x030000ac;
    uint32_t tag = iwram_read32(g_active_info_addr);
    return tag == 0x6f666e49u;
}

// The `ActiveInfo` struct in the test ROM (see suite-master/include/common.h)
// is 8 bytes, not 16 as an earlier draft of this runner assumed:
//   +0 char magic[4];       // 'Info'
//   +4 uint16_t subtestId;
//   +6 uint8_t  testId;
//   +7 uint8_t  suiteId;
// Reading suiteId as a 32-bit little-endian word at +4 conflates it with
// subtestId/testId and produces garbage like 0x0DFFFFFF (confirmed under
// LLDB: IWRAM[0xb0..0xb3] = FF FF FF 0D when suiteId == 13).
static uint8_t iwram_read_u8(uint32_t addr) {
    if (!g_internalRAM) return 0;
    return g_internalRAM[(addr - 0x03000000u) & 0x7FFFu];
}

static int32_t active_suite_id() {
    if (!g_active_info_addr) return -2;
    return (int32_t)(int8_t)iwram_read_u8(g_active_info_addr + 7);
}

static int32_t active_test_id() {
    if (!g_active_info_addr) return -2;
    return (int32_t)(int8_t)iwram_read_u8(g_active_info_addr + 6);
}

// ---- Video-suite driver ----------------------------------------------------
//
// The video suite has no `run()` callback — each test is inherently visual:
// `expected(refresh)` manually paints an image into VRAM, `actual(refresh)`
// asks the GBA graphics hardware to paint the same thing, and the human user
// is supposed to toggle between the two via A/LEFT/RIGHT and compare them by
// eye. To produce a PASS/FAIL automatically we:
//
//   1. Enter the video suite (the runSuite viewer for it).
//   2. For each of the 7 tests:
//      a. Press A to enter that test's show() loop (renders "actual" first).
//      b. Let the renderer run a few frames and snapshot g_pix → `actual`.
//      c. Press A to toggle to "expected", snapshot → `expected`.
//      d. Press B to exit show().
//      e. Press DOWN to advance to the next test in the viewer.
//   3. Compare actual vs expected per test; count differing pixels.
//
// `passes` is the number of tests where every pixel matched exactly.

static constexpr int kGbaPixW = 240;
static constexpr int kGbaPixH = 160;
static constexpr size_t kFrameBytes = 4 * kGbaPixW * kGbaPixH;

// The video-suite show() loop draws an "Actual" / "Expected" label sprite at
// OAM[0] with OBJ_Y(148). That sprite's text differs between the two views
// but is NOT part of the test's rendering. Exclude the label strip from the
// pixel compare by only considering rows 0..kLabelTopY-1.
static constexpr int kLabelTopY = 148;
static constexpr size_t kCompareBytes = 4 * kGbaPixW * kLabelTopY;

// Write a 240x160 framebuffer as a PPM file for inspection. Only used when
// VIDEO_DUMP_DIR env var is set.
static void dump_frame_ppm(const uint8_t* px, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", kGbaPixW, kGbaPixH);
    for (int y = 0; y < kGbaPixH; ++y) {
        for (int x = 0; x < kGbaPixW; ++x) {
            const uint8_t* p = px + 4 * (y * kGbaPixW + x);
            fputc(p[0], f); fputc(p[1], f); fputc(p[2], f);
        }
    }
    fclose(f);
}

static void copy_framebuffer(uint8_t* dst) {
    // VBA-M's 32-bit-depth g_pix layout (non-libretro build) is a 241×162
    // uint32 grid with one leading padding row and one trailing padding
    // column — see `uint32_t* dest = g_pix + 241 * (VCOUNT + 1)` in gba.cpp.
    // We copy the visible 240×160 region row-by-row, skipping the leading
    // padding row and the trailing-column padding byte.
    if (!g_pix) {
        std::memset(dst, 0, kFrameBytes);
        return;
    }
    const uint32_t* src_rows = (const uint32_t*)g_pix + 241 /* skip pad row */;
    uint8_t* out = dst;
    for (int y = 0; y < kGbaPixH; ++y) {
        std::memcpy(out, src_rows + 241 * y, 4 * kGbaPixW);
        out += 4 * kGbaPixW;
    }
}

static bool frame_equal(const uint8_t* a, const uint8_t* b) {
    // Only compare the visible test area above the Actual/Expected label.
    return std::memcmp(a, b, kCompareBytes) == 0;
}

// Cheap FNV-ish checksum so we can distinguish "actual and expected rendered
// to the same image" (a real PASS) from "both snapshots captured the same
// unmodified frame" (a bogus PASS that would mean the harness never got the
// renderer to paint anything). Hashes the same region frame_equal compares.
static uint64_t frame_hash(const uint8_t* p) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < kCompareBytes; i += 16) {
        h ^= (uint64_t)p[i] | ((uint64_t)p[i + 4] << 16) |
             ((uint64_t)p[i + 8] << 32) | ((uint64_t)p[i + 12] << 48);
        h *= 0x100000001b3ull;
    }
    return h;
}

struct VideoStats {
    uint32_t passes = 0;
    uint32_t total  = 0;
};

static VideoStats drive_video_suite(int n_tests) {
    VideoStats s;
    s.total = (uint32_t)n_tests;

    // Robust navigation strategy: always walk from the main menu to the
    // target video test. We press B twice to make sure we're at the main
    // menu (one B exits show(), another exits the viewer — superfluous Bs
    // at the main menu are harmless). Then we press A to enter the video
    // suite viewer, then DOWN `t` times to select the target test, then A
    // to enter its show(). This avoids state drift between iterations.

    std::vector<uint8_t> actual(kFrameBytes, 0);
    std::vector<uint8_t> expected(kFrameBytes, 0);

    const bool trace = getenv("VIDEO_NAV_TRACE") != nullptr;

    for (int t = 0; t < n_tests; ++t) {
        if (trace) fprintf(stderr, "[t=%d] entry suiteId=%d testId=%d\n",
                           t, active_suite_id(), active_test_id());
        // Force-exit whatever we're in until suiteId reads back as -1
        // (main-menu state). Pressing B from the main menu is a no-op, so
        // looping up to 4 times is safe and deterministic.
        for (int k = 0; k < 4; ++k) {
            if (active_suite_id() < 0) break;
            press_button(KEY_B, 2);
            run_frames(10, 0);
            if (trace) fprintf(stderr, "  B#%d suiteId=%d testId=%d\n",
                               k, active_suite_id(), active_test_id());
        }

        // Enter the video suite (already selected from outer loop nav).
        press_button(KEY_A, 2);
        run_frames(20, 0);
        if (trace) fprintf(stderr, "  after A(enter suite) suiteId=%d testId=%d\n",
                           active_suite_id(), active_test_id());

        // Advance the viewer's testIndex to `t` by pressing DOWN t times.
        for (int k = 0; k < t; ++k) {
            press_button(KEY_DOWN, 2);
            run_frames(10, 0);
        }
        if (trace) fprintf(stderr, "  after DOWN*%d suiteId=%d testId=%d\n",
                           t, active_suite_id(), active_test_id());

        fprintf(stderr, "    video[%d/%d] (suiteId=%d)...",
                t + 1, n_tests, active_suite_id());

        // Enter this test's show() — renders the "actual" image first.
        press_button(KEY_A, 2);
        run_frames(30, 0);
        if (trace) fprintf(stderr, "\n  after A(enter show) suiteId=%d testId=%d\n",
                           active_suite_id(), active_test_id());

        copy_framebuffer(actual.data());
        if (const char* d = getenv("VIDEO_DUMP_DIR")) {
            char p[512]; snprintf(p, sizeof(p), "%s/v%d_actual.ppm", d, t + 1);
            dump_frame_ppm(actual.data(), p);
        }

        // Toggle to the "expected" (software-drawn reference) image.
        press_button(KEY_A, 2);
        run_frames(30, 0);
        if (trace) fprintf(stderr, "  after A(toggle) suiteId=%d testId=%d\n",
                           active_suite_id(), active_test_id());

        copy_framebuffer(expected.data());
        if (const char* d = getenv("VIDEO_DUMP_DIR")) {
            char p[512]; snprintf(p, sizeof(p), "%s/v%d_expected.ppm", d, t + 1);
            dump_frame_ppm(expected.data(), p);
        }

        uint64_t ha = frame_hash(actual.data());
        uint64_t he = frame_hash(expected.data());
        bool pass = frame_equal(actual.data(), expected.data());
        if (pass) {
            ++s.passes;
            fprintf(stderr, " PASS (hash=%016llx)\n", (unsigned long long)ha);
        } else {
            // Count how many pixels differ in the compared (non-label) area.
            size_t diff = 0;
            for (size_t k = 0; k < kCompareBytes; k += 4) {
                if (actual[k] != expected[k] || actual[k+1] != expected[k+1] ||
                    actual[k+2] != expected[k+2] || actual[k+3] != expected[k+3])
                    ++diff;
            }
            fprintf(stderr,
                    " FAIL (%zu pixels differ, hA=%016llx hE=%016llx)\n",
                    diff, (unsigned long long)ha, (unsigned long long)he);
        }

        // Next iteration starts from main menu navigation, so no explicit
        // exit-from-show is needed — the leading B presses handle it.
    }
    return s;
}

// ---- SRAM log dump ---------------------------------------------------------

static void dump_sram_log(FILE* out) {
    // VBA-M stores both SRAM and Flash save data in flashSaveMemory.
    uint8_t* sram = flashSaveMemory;
    int sram_size = 0x10000;
    // Find a reasonable end point by looking for the last non-zero byte.
    int end = sram_size - 1;
    while (end > 0 && sram[end] == 0) --end;
    if (end <= 0) {
        fputs("---- SRAM log dump: empty ----\n", out);
        return;
    }
    fputs("---- SRAM log dump ----\n", out);
    fwrite(sram, 1, end + 1, out);
    fputs("\n---- end SRAM log ----\n", out);
}

// ---- Main ------------------------------------------------------------------

int main(int argc, char** argv) {
    const char* rom_path =
        (argc > 1) ? argv[1] : "/Users/andyvand/Downloads/suite-master/suite.gba";

    // Load the ROM file into a buffer.
    FILE* f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "suite_runner: cannot open %s\n", rom_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long rom_bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> rom_buf(rom_bytes);
    if (fread(rom_buf.data(), 1, rom_bytes, f) != (size_t)rom_bytes) {
        fprintf(stderr, "suite_runner: short read on %s\n", rom_path);
        fclose(f);
        return 1;
    }
    fclose(f);

    // Core wants SRAM so the ROM's savprintf() can store its test log.
    coreOptions.saveType = 2; // GBA_SAVE_SRAM
    coreOptions.useBios = 0;
    coreOptions.skipBios = true;

    if (!CPULoadRomData(rom_buf.data(), (int)rom_bytes)) {
        fprintf(stderr, "suite_runner: CPULoadRomData failed\n");
        return 1;
    }
    CPUInit("", false);
    SetSaveType(2);
    soundInit();
    CPUReset();
    emulating = 1;

    // Initialize the BGR555 → RGBA8888 lookup table so the renderer can
    // actually write meaningful pixels into g_pix. (Without this every
    // pixel resolves to systemColorMap32[...] == 0 and the video-suite
    // comparator trivially reports every test as matching.)
    systemColorDepth = 32;
    systemRedShift   = 3;   // 5-bit R → byte 0 (bits 3..7)
    systemGreenShift = 11;  // byte 1
    systemBlueShift  = 19;  // byte 2
    for (int i = 0; i < 0x10000; ++i) {
        const uint32_t r5 = (uint32_t)(i & 0x1F);
        const uint32_t g5 = (uint32_t)((i >> 5) & 0x1F);
        const uint32_t b5 = (uint32_t)((i >> 10) & 0x1F);
        systemColorMap32[i] =
            (r5 << systemRedShift) | (g5 << systemGreenShift) |
            (b5 << systemBlueShift) | 0xFF000000u /* opaque alpha */;
    }

    // Let the ROM boot past crt0 + splash (no input).
    run_frames(180, 0);

    if (!locate_active_info()) {
        fprintf(stderr,
                "suite_runner: could not locate activeTestInfo tag in IWRAM\n");
    } else {
        fprintf(stderr,
                "suite_runner: activeTestInfo at 0x%08x\n",
                g_active_info_addr);
    }

    // Cache the per-suite pass/total pointers from ROM now (they never move).
    SuiteCounters counters[kNumSuites];
    for (int i = 0; i < kNumSuites; ++i) {
        counters[i] = read_suite_counters(kSuites[i].struct_addr);
        fprintf(stderr,
                "suite_runner: %-14s  struct=%08x passes=%08x total=%08x nTests=%u\n",
                kSuites[i].name, kSuites[i].struct_addr,
                counters[i].passes_addr, counters[i].total_addr,
                counters[i].n_tests);
    }

    // Per-suite (passes, total) overrides used for suites whose counters
    // don't live in IWRAM (video) — we compute them via pixel comparison.
    uint32_t override_passes[kNumSuites] = {0};
    uint32_t override_total [kNumSuites] = {0};
    bool     have_override  [kNumSuites] = {false};

    // Drive the test UI: press A to enter each suite, wait for completion,
    // press B to return, press DOWN to advance selection.
    for (int i = 0; i < kNumSuites; ++i) {
        fprintf(stderr, "\n[%d/%d] running %s...\n",
                i + 1, kNumSuites, kSuites[i].name);

        press_button(KEY_A, 8);

        // The video suite has `run == NULL` — its counters point into ROM
        // at `constZero`, so we can't poll them. Drive it specially by
        // iterating each test's show() loop and pixel-comparing.
        const bool is_video = (counters[i].passes_addr >> 24) != 0x03;
        if (is_video) {
            VideoStats vs = drive_video_suite((int)counters[i].n_tests);
            override_passes[i] = vs.passes;
            override_total[i]  = vs.total;
            have_override[i]   = true;
            fprintf(stderr, "    %s: %u / %u\n",
                    kSuites[i].name, vs.passes, vs.total);
            // Exit the suite viewer back to main menu.
            press_button(KEY_B, 8);
            if (i + 1 < kNumSuites) press_button(KEY_DOWN, 6);
            continue;
        }

        // Wait for suite->run() to start (activeTestInfo.suiteId becomes >= 0
        // once runSuite() executes).
        int waited = 0;
        while (active_suite_id() < 0 && waited < 60) {
            run_frames(1, 0);
            ++waited;
        }

        // Poll pass/total until they stabilize. Allow up to 120 seconds.
        uint32_t last_passes = 0, last_total = 0;
        int stable = 0;
        for (int frame = 0; frame < 7200; ++frame) {
            run_frames(1, 0);
            uint32_t p = iwram_read32(counters[i].passes_addr);
            uint32_t t = iwram_read32(counters[i].total_addr);
            if (p == last_passes && t == last_total && t > 0) {
                if (++stable >= 180) break; // 3 seconds stable
            } else {
                stable = 0;
            }
            last_passes = p;
            last_total = t;
        }

        uint32_t passes = iwram_read32(counters[i].passes_addr);
        uint32_t total  = iwram_read32(counters[i].total_addr);
        fprintf(stderr, "    %s: %u / %u\n", kSuites[i].name, passes, total);

        // Return to main menu.
        press_button(KEY_B, 6);
        // Move selection to next suite.
        if (i + 1 < kNumSuites) {
            press_button(KEY_DOWN, 6);
        }
    }

    // Final report.
    puts("\n================ suite.gba results ================");
    uint32_t grand_pass = 0, grand_total = 0;
    for (int i = 0; i < kNumSuites; ++i) {
        uint32_t p, t;
        if (have_override[i]) {
            p = override_passes[i];
            t = override_total[i];
        } else {
            p = iwram_read32(counters[i].passes_addr);
            t = iwram_read32(counters[i].total_addr);
        }
        grand_pass += p;
        grand_total += t;
        printf("  %-14s  %5u / %-5u  (%s)\n",
               kSuites[i].name, p, t,
               (t > 0 && p == t) ? "PASS" : "FAIL");
    }
    printf("  %-14s  %5u / %-5u\n", "TOTAL", grand_pass, grand_total);
    puts("===================================================\n");
    fflush(stdout);

    // Dump the suite's own SRAM log.
    dump_sram_log(stdout);

    GBASystem.emuCleanUp();
    return (grand_total > 0 && grand_pass == grand_total) ? 0 : 2;
}
