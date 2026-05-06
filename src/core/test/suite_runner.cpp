// Headless runner for mGBA's test suite (suite.gba).
//
// Drives the emulator through every sub-suite by injecting synthetic keypad
// input, then reads IWRAM to report per-suite pass/total counts and dumps the
// SRAM log at the end.
//
// Usage: suite_runner [--bios <path>|-b <path>] [--hle] [path/to/suite.gba]
//   --bios <path>   Use a real GBA BIOS file (must be exactly 16384 bytes).
//                   Falls back to HLE BIOS if the file is missing/invalid.
//   --hle           Force HLE BIOS even if --bios / env / default-probe find
//                   a real BIOS file. Useful for HLE-vs-real comparison runs.
//   GBA_BIOS_FILE   Environment variable override for --bios.
// Default ROM path:  /Users/andyvand/Downloads/suite-master/suite.gba
// Default BIOS probe: /Users/andyvand/gba_bios.bin (used if no flag/env and
//                     the file exists, and --hle was not specified).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

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

// If CMake auto-built mGBA's suite.gba from upstream and successfully
// pulled the TestSuite struct addresses out of suite.elf via objdump,
// it dropped a header with the build-matched table. Use it when
// available so the runner doesn't drift against whichever commit /
// toolchain produced the .gba blob; otherwise fall back to the
// originally-reverse-engineered addresses below.
#if defined(VBAM_HAS_SUITE_ADDRESSES) && __has_include("suite_addresses.h")
#include "suite_addresses.h"
static const SuiteDesc* kSuites = reinterpret_cast<const SuiteDesc*>(kSuitesAuto);
static constexpr int kNumSuites = sizeof(kSuitesAuto) / sizeof(kSuitesAuto[0]);
#else
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
#endif

// Execution order: short names looked up in kSuites/resolved[] at runtime.
// Distinct from menu order because some suites have side effects that
// pollute later ones:
//   - memory must run AFTER dma. The Memory test writes through the OAM
//     mirror at 0x07000400+ and never restores it; subsequent DMA tests
//     that read OAM see corrupt values and fail.
// Comment any reorder so the reason survives. Names must match kSuites[].
static const char* const kExecutionOrderNames[] = {
    // dma + misc-edge run before any test that leaves state dirty.
    // misc-edge's dmaPrefetch is sub-cycle-phase-sensitive: any test that
    // leaves a timer running (timers, timer-irq, sio-timing) or that
    // perturbs the cumulative HBlank phase shifts the loop into a basin
    // that catches a different DMA-fire residue, producing a wrong but
    // deterministic break value. Real HW and NanoBoyAdvance exhibit the
    // same fragility — fix is environmental, not in any one emulator.
    "dma",            // must run before memory (memory pollutes OAM mirror)
    "misc-edge",      // must run before timers/sio-timing pollute cycle phase
    "memory",
    "io-read",
    "timing",
    "timers",
    "timer-irq",
    "shifter",
    "carry",
    "multiply-long",
    "bios-math",
    "sio-read",
    "sio-timing",
    "video",
};
static constexpr int kExecutionOrderN =
    sizeof(kExecutionOrderNames) / sizeof(kExecutionOrderNames[0]);

// Canonical short names indexed in the same order as kSuites[] / kSuitesAuto[].
// Used for the execution-order lookup so it works regardless of whether
// kSuites is the hardcoded short-name table (#else branch above) or the
// auto-generated kSuitesAuto (which carries the LONG display names like
// "Memory tests", "DMA tests"). Without this table the auto path would
// silently fall back to ROM-menu order because every short-name match
// would fail.
static const char* const kCanonicalShortNames[] = {
    "memory",        // 0
    "io-read",       // 1
    "timing",        // 2
    "timers",        // 3
    "timer-irq",     // 4
    "shifter",       // 5
    "carry",         // 6
    "multiply-long", // 7
    "bios-math",     // 8
    "dma",           // 9
    "sio-read",      // 10
    "sio-timing",    // 11
    "misc-edge",     // 12
    "video",         // 13
};
static_assert(sizeof(kCanonicalShortNames) / sizeof(kCanonicalShortNames[0])
              == 14,
              "kCanonicalShortNames must have 14 entries to match kSuites");

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

// ---- Dynamic suite-struct discovery ---------------------------------------
//
// The hardcoded `kSuites[]` addresses above were captured against one
// specific build of suite.gba. Different upstream commits (or different
// toolchains) relink the suite ELF at different addresses, so the numbers
// silently go stale — `read_suite_counters` then returns garbage at
// struct+20/+24, the `is_video` heuristic at the top of the run loop
// (`(passes_addr >> 24) != 0x03`) misclassifies every suite as video, the
// wrong navigation path runs, and the runner ends up looping on the memory
// suite forever (the Ubuntu CI 30-min timeout).
//
// Fix: walk the loaded ROM at runtime looking for a 28-byte block that
// matches the TestSuite struct layout. Anchor on the suite-name string
// (each suite has a distinct one-word name in ROM). We require the
// neighbouring fields to look right too — function pointers in ROM, a
// small nTests, passes/total in IWRAM (or in ROM for the video suite,
// whose counters point at a constant zero in .rodata).
//
// The order returned matches the upstream main.c `suites[]` array, so
// pressing DOWN N times from the top of the menu still lands on suite
// index N (which the run loop relies on).

// Match the *display* names that the suite stores in TestSuite.name (these
// are what `strings suite.gba` shows). The order must match kSuites[] above
// — that's the order pressing DOWN walks through.
static const char* const kCanonicalSuiteNames[] = {
    "Memory tests",
    "I/O read tests",
    "Timing tests",
    "Timer count-up tests",
    "Timer IRQ tests",
    "Shifter tests",
    "Carry tests",
    "Multiply long tests",
    "BIOS math tests",
    "DMA tests",
    "SIO register R/W tests",
    "SIO timing tests",
    "Misc. edge case tests",
    "Video tests"
};
static constexpr int kCanonicalNumSuites =
    sizeof(kCanonicalSuiteNames) / sizeof(kCanonicalSuiteNames[0]);

namespace {
struct DiscoveredSuite {
    const char* name = nullptr;
    uint32_t struct_addr = 0;
};

// Read 4 bytes from a ROM file offset (g_rom is little-endian).
static inline uint32_t rom_off_read32(uint32_t off) {
    return (uint32_t)g_rom[off]
         | ((uint32_t)g_rom[off + 1] << 8)
         | ((uint32_t)g_rom[off + 2] << 16)
         | ((uint32_t)g_rom[off + 3] << 24);
}

// One pass over ROM. For every 4-byte-aligned offset, treat the word as a
// candidate `name` field and check whether the surrounding 28 bytes look
// like a valid TestSuite struct. The first plausible struct per suite name
// wins; later matches (e.g. an unrelated string ref pointing at the same
// name) are dropped because we already filled that slot.
static std::vector<DiscoveredSuite> discover_suites_in_rom() {
    std::vector<DiscoveredSuite> found(kCanonicalNumSuites);
    if (!g_rom) return found;
    const uint32_t romSize = (uint32_t)gbaGetRomSize();
    if (romSize < 32) return found;

    for (uint32_t off = 0; off + 28 <= romSize; off += 4) {
        // Field +0: ROM pointer to the suite name.
        uint32_t w0 = rom_off_read32(off);
        uint8_t w0_top = (uint8_t)(w0 >> 24);
        if (w0_top < 0x08 || w0_top > 0x0D) continue;
        uint32_t name_off = (w0 - 0x08000000u) & 0x01FFFFFFu;
        if (name_off >= romSize) continue;

        int slot = -1;
        for (int k = 0; k < kCanonicalNumSuites; ++k) {
            const char* n = kCanonicalSuiteNames[k];
            size_t nlen = std::strlen(n);
            if (name_off + nlen + 1 > romSize) continue;
            // Compare including trailing NUL so we don't match e.g. "timing"
            // against the start of "timings" or "timing-stuff".
            if (std::memcmp(g_rom + name_off, n, nlen + 1) == 0) {
                slot = k;
                break;
            }
        }
        if (slot < 0) continue;
        if (found[slot].struct_addr) continue; // first hit wins

        // Field +4: run() pointer. ROM (Thumb-bit ignored) or NULL (video).
        uint32_t w_run = rom_off_read32(off + 4);
        if (w_run != 0) {
            uint8_t t = (uint8_t)(w_run >> 24);
            if (t < 0x08 || t > 0x0D) continue;
        }
        // Field +16: nTests. Suites are well under 4096 cases each.
        uint32_t nTests = rom_off_read32(off + 16);
        if (nTests == 0 || nTests > 4096) continue;
        // Field +20: passes pointer. IWRAM (0x03) for normal suites; ROM
        // (constZero in .rodata) for the video suite.
        uint32_t pAddr = rom_off_read32(off + 20);
        uint8_t pt = (uint8_t)(pAddr >> 24);
        bool pIWRAM = (pt == 0x03);
        bool pROM   = (pt >= 0x08 && pt <= 0x0D);
        if (!pIWRAM && !pROM) continue;
        // Field +24: total pointer. Same constraints as passes.
        uint32_t tAddr = rom_off_read32(off + 24);
        uint8_t tt = (uint8_t)(tAddr >> 24);
        bool tIWRAM = (tt == 0x03);
        bool tROM   = (tt >= 0x08 && tt <= 0x0D);
        if (!tIWRAM && !tROM) continue;

        found[slot].name = kCanonicalSuiteNames[slot];
        found[slot].struct_addr = 0x08000000u + off;
    }
    return found;
}
} // namespace

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

// SRAM log dump mode. The suite ROM's savprintf() writes printable ASCII
// into flashSaveMemory, but only into the regions a given test actually
// touched — gaps remain at 0xFF (flash-erase) and sometimes 0x00 (zero
// init). Naively fwriting the whole buffer streams those non-UTF-8 bytes
// into the CI log, where they render as `���` garbage. We always strip
// non-printable bytes first; the modes below control how much of the
// remaining ASCII we emit.
enum class SramDumpMode {
    kFull,           // Every printable line (verbose; for local debugging).
    kFailuresOnly,   // Only lines containing "FAIL" — keeps CI logs short.
    kNone,           // Skip entirely.
};

static void dump_sram_log(FILE* out, SramDumpMode mode) {
    if (mode == SramDumpMode::kNone) return;

    // Recover the printable text from flashSaveMemory. Keep ASCII printable
    // (0x20–0x7E) plus newline and tab; drop everything else (0x00 / 0xFF
    // init/erase patterns and any other control bytes that would render
    // as invalid UTF-8 in the CI log). This makes the output safe to
    // print regardless of which SRAM regions the suite happened to touch.
    static constexpr int kSramSize = 0x8000;
    const uint8_t* sram = flashSaveMemory;
    std::string text;
    text.reserve(kSramSize);
    for (int i = 0; i < kSramSize; ++i) {
        uint8_t c = sram[i];
        if (c == '\n' || c == '\t' || (c >= 0x20 && c <= 0x7E)) {
            text.push_back(static_cast<char>(c));
        }
    }
    // Strip trailing whitespace so we don't emit a tail of blank lines.
    while (!text.empty() &&
           (text.back() == '\n' || text.back() == '\t' || text.back() == ' ')) {
        text.pop_back();
    }
    if (text.empty()) {
        fputs("---- SRAM log: empty ----\n", out);
        return;
    }

    if (mode == SramDumpMode::kFull) {
        fputs("---- SRAM log ----\n", out);
        fputs(text.c_str(), out);
        fputc('\n', out);
        fputs("---- end SRAM log ----\n", out);
        return;
    }

    // kFailuresOnly: emit just the lines that look like a failed sub-test.
    // mGBA-suite's savprintf output uses "FAIL" as its failure marker;
    // matching it case-sensitively keeps "PASS" lines out without dragging
    // in unrelated prose. If the upstream suite ROM ever changes its
    // marker, widen the predicate here.
    fputs("---- failed sub-tests ----\n", out);
    size_t shown = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        size_t line_end = (nl == std::string::npos) ? text.size() : nl;
        std::string line = text.substr(pos, line_end - pos);
        if (line.find("FAIL") != std::string::npos) {
            fputs(line.c_str(), out);
            fputc('\n', out);
            ++shown;
        }
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    if (shown == 0) {
        fputs("(no FAIL lines in SRAM log)\n", out);
    }
    fputs("---- end failed sub-tests ----\n", out);
}

// ---- Main ------------------------------------------------------------------

int main(int argc, char** argv) {
    // Parse args. Recognized: --bios <path>, -b <path>, --hle. The first
    // remaining positional arg is the ROM path.
    const char* rom_path = nullptr;
    const char* bios_arg = nullptr;
    bool force_hle = false;
    int min_pass = -1;            // CI baseline; see gba.suite test in CMake.
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if ((std::strcmp(a, "--bios") == 0 || std::strcmp(a, "-b") == 0) &&
            i + 1 < argc) {
            bios_arg = argv[++i];
        } else if (std::strcmp(a, "--hle") == 0) {
            force_hle = true;
        } else if (std::strcmp(a, "--min-pass") == 0 && i + 1 < argc) {
            min_pass = std::atoi(argv[++i]);
        } else if (!rom_path) {
            rom_path = a;
        }
    }
    if (!rom_path)
        rom_path = "/Users/andyvand/Downloads/suite-master/suite.gba";

    // Resolve BIOS source: --hle wins outright; else --bios > env
    // GBA_BIOS_FILE > default-probe. A path is "good" only if it exists and
    // is exactly 16384 bytes.
    auto probe_bios = [](const char* p) -> bool {
        if (!p || !*p) return false;
        std::error_code ec;
        const auto path = std::filesystem::path(p);
        return std::filesystem::is_regular_file(path, ec) &&
               std::filesystem::file_size(path, ec) == 16384 &&
               !ec;
    };
    const char* bios_path = nullptr;
    if (force_hle) {
        if (bios_arg)
            fprintf(stderr,
                    "suite_runner: --hle overrides --bios %s\n",
                    bios_arg);
    } else {
        if (bios_arg) {
            if (probe_bios(bios_arg))
                bios_path = bios_arg;
            else
                fprintf(stderr,
                        "suite_runner: --bios %s rejected (missing or not "
                        "16384 bytes)\n",
                        bios_arg);
        }
        if (!bios_path) {
            const char* env = std::getenv("GBA_BIOS_FILE");
            if (env && *env) {
                if (probe_bios(env))
                    bios_path = env;
                else
                    fprintf(stderr,
                            "suite_runner: GBA_BIOS_FILE=%s rejected (missing "
                            "or not 16384 bytes)\n",
                            env);
            }
        }
        if (!bios_path) {
            // Default-probe: silent fallback if not present.
            const char* def = "/Users/andyvand/gba_bios.bin";
            if (probe_bios(def))
                bios_path = def;
        }
    }

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
    coreOptions.useBios = bios_path ? 1 : 0;
    coreOptions.skipBios = true; // start at cart entry; real BIOS still
                                 // services SWI/IRQ vectors.

    if (!CPULoadRomData(rom_buf.data(), (int)rom_bytes)) {
        fprintf(stderr, "suite_runner: CPULoadRomData failed\n");
        return 1;
    }
    if (bios_path) {
        fprintf(stderr, "suite_runner: BIOS = real (%s, 16384 bytes)\n",
                bios_path);
        CPUInit(bios_path, true);
        // CPUInit may reset useBios to 0 on load failure; re-check.
        if (!coreOptions.useBios) {
            fprintf(stderr,
                    "suite_runner: real BIOS load failed inside CPUInit; "
                    "continuing with HLE\n");
        }
    } else {
        fprintf(stderr, "suite_runner: BIOS = HLE (myROM[])\n");
        CPUInit("", false);
    }
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

    // Resolve TestSuite struct addresses. Prefer dynamic discovery (which
    // walks the loaded ROM and matches the canonical suite-name list against
    // candidate TestSuite struct shapes) over the hardcoded `kSuites[]`
    // table, because the latter goes stale every time upstream relinks
    // suite.gba. Per-slot fallback to the hardcoded value if discovery
    // missed a particular suite.
    struct ResolvedSuite { const char* name; uint32_t struct_addr; };
    ResolvedSuite resolved[kNumSuites];
    int discovered_count = 0;
    {
        auto discovered = discover_suites_in_rom();
        // kCanonicalSuiteNames must enumerate the same suites in the same
        // order as kSuites for the indices to agree.
        for (int i = 0; i < kNumSuites; ++i) {
            if (discovered[i].struct_addr) {
                resolved[i].name = discovered[i].name;
                resolved[i].struct_addr = discovered[i].struct_addr;
                ++discovered_count;
            } else {
                resolved[i].name = kSuites[i].name;
                resolved[i].struct_addr = kSuites[i].struct_addr;
            }
        }
        fprintf(stderr,
                "suite_runner: discovered %d/%d TestSuite addresses in ROM "
                "(remaining slots use hardcoded fallback)\n",
                discovered_count, kNumSuites);
    }

    // Cache the per-suite pass/total pointers from ROM now (they never move).
    SuiteCounters counters[kNumSuites];
    for (int i = 0; i < kNumSuites; ++i) {
        counters[i] = read_suite_counters(resolved[i].struct_addr);
        fprintf(stderr,
                "suite_runner: %-14s  struct=%08x passes=%08x total=%08x nTests=%u\n",
                resolved[i].name, resolved[i].struct_addr,
                counters[i].passes_addr, counters[i].total_addr,
                counters[i].n_tests);
    }

    // Per-suite (passes, total) overrides used for suites whose counters
    // don't live in IWRAM (video) — we compute them via pixel comparison.
    uint32_t override_passes[kNumSuites] = {0};
    uint32_t override_total [kNumSuites] = {0};
    bool     have_override  [kNumSuites] = {false};

    // Build the actual execution order: indices into resolved[]/counters[]
    // looked up from kExecutionOrderNames[]. A name not present in the
    // resolved table (because the auto-discovered header didn't include
    // it) gets skipped — the suite would have no struct address to drive
    // anyway. Suites in resolved[] that aren't named in the execution
    // list get appended at the end so we never silently drop a suite.
    int order[kNumSuites];
    int order_n = 0;
    {
        bool used[kNumSuites] = {false};
        for (int k = 0; k < kExecutionOrderN && order_n < kNumSuites; ++k) {
            for (int i = 0; i < kNumSuites; ++i) {
                // Match against kCanonicalShortNames[i] — guaranteed to be
                // the short name regardless of whether kSuites is the
                // hardcoded table (also short) or kSuitesAuto (long
                // display names like "DMA tests"). kExecutionOrderNames is
                // keyed on the short form. Fall back to kSuites[i].name
                // only if i is somehow out of range, which it isn't given
                // the static_assert above.
                const char* canon =
                    (i < (int)(sizeof(kCanonicalShortNames) /
                               sizeof(kCanonicalShortNames[0])))
                        ? kCanonicalShortNames[i]
                        : kSuites[i].name;
                if (!used[i] && canon &&
                    std::strcmp(canon, kExecutionOrderNames[k]) == 0) {
                    order[order_n++] = i;
                    used[i] = true;
                    break;
                }
            }
        }
        for (int i = 0; i < kNumSuites; ++i) {
            if (!used[i]) {
                order[order_n++] = i;
                const char* canon =
                    (i < (int)(sizeof(kCanonicalShortNames) /
                               sizeof(kCanonicalShortNames[0])))
                        ? kCanonicalShortNames[i]
                        : (kSuites[i].name ? kSuites[i].name : "(unnamed)");
                fprintf(stderr,
                        "suite_runner: %s not in kExecutionOrderNames, "
                        "appending at end\n",
                        canon);
            }
        }
    }

    // Drive the test UI in execution order, not menu order. Menu cursor
    // starts at index 0; we navigate to each target by pressing DOWN
    // (target - cursor + N) mod N times. The suite menu wraps (main.c
    // does `suiteIndex %= nSuites`) so DOWN-only navigation reaches any
    // index regardless of starting position.
    // Allow the show-mode probe to run from a fresh-boot state by
    // skipping the suite-level loop. Set VBAM_PROBE_ONLY=1 to mimic the
    // user-reported flow: open ROM, navigate to misc-edge, run, press A
    // on H-blank bit start, observe.
    bool probe_only = std::getenv("VBAM_PROBE_ONLY") != nullptr;
    int cursor = 0;
    for (int o = 0; o < order_n && !probe_only; ++o) {
        int i = order[o];

        int steps = ((i - cursor) % kNumSuites + kNumSuites) % kNumSuites;
        for (int s = 0; s < steps; ++s) {
            press_button(KEY_DOWN, 6);
        }
        cursor = i;

        fprintf(stderr, "\n[%d/%d] running %s (menu pos %d)...\n",
                o + 1, order_n, resolved[i].name, i);

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
                    resolved[i].name, vs.passes, vs.total);
            // Exit the suite viewer back to main menu.
            press_button(KEY_B, 8);
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
        fprintf(stderr, "    %s: %u / %u\n", resolved[i].name, passes, total);

        // Return to main menu. After a polled suite finishes, the ROM is
        // sitting at the per-suite results screen; one B-press takes us
        // back to the suite list. Wait until activeTestInfo.suiteId
        // resets to -1 so the cursor is stable before the next iteration
        // navigates to its target — without this, on slow paths the
        // navigation presses arrive while the suite is still tearing
        // down and get consumed by something else.
        press_button(KEY_B, 6);
        int back_wait = 0;
        while (active_suite_id() >= 0 && back_wait < 60) {
            run_frames(1, 0);
            ++back_wait;
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
               resolved[i].name, p, t,
               (t > 0 && p == t) ? "PASS" : "FAIL");
    }
    printf("  %-14s  %5u / %-5u\n", "TOTAL", grand_pass, grand_total);
    puts("===================================================\n");
    fflush(stdout);

    // Dump the suite's own SRAM log. In CI mode (a min_pass floor was
    // supplied via --min-pass) we emit only the failed sub-test lines so
    // a passing run still surfaces useful diagnostics without flooding
    // the log with the binary tail of the SRAM buffer. Without a floor
    // the runner is being invoked manually, so emit the full filtered
    // log. Set VBAM_SRAM_DUMP=full / =failures / =none to override.
    SramDumpMode dump_mode =
        (min_pass >= 0) ? SramDumpMode::kFailuresOnly : SramDumpMode::kFull;
    if (const char* m = std::getenv("VBAM_SRAM_DUMP")) {
        if (std::strcmp(m, "full") == 0)         dump_mode = SramDumpMode::kFull;
        else if (std::strcmp(m, "failures") == 0) dump_mode = SramDumpMode::kFailuresOnly;
        else if (std::strcmp(m, "none") == 0)     dump_mode = SramDumpMode::kNone;
    }
    dump_sram_log(stdout, dump_mode);

    GBASystem.emuCleanUp();
    if (min_pass >= 0) {
        if ((int)grand_pass >= min_pass) {
            fprintf(stderr,
                    "[ci] PASS %u >= floor %d — OK\n",
                    grand_pass, min_pass);
            return 0;
        }
        fprintf(stderr,
                "[ci] PASS %u < floor %d — REGRESSION\n",
                grand_pass, min_pass);
        return 2;
    }
    return (grand_total > 0 && grand_pass == grand_total) ? 0 : 2;
}