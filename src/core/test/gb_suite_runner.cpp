// Headless test harness for Game Boy / Game Boy Color / Super Game Boy.
//
// Drives the VBA-M GB core through Blargg's gb-test-roms suite (or any
// directory of .gb / .gbc files). Each ROM is loaded, run for a bounded
// number of frames, and its result is captured via the GB serial port
// (Blargg's tests print "Passed" / "Failed #N" to SB/SC) — falling back
// to the screen-text fingerprint of dmg_sound / cgb_sound when serial
// stays empty.
//
// Usage: gb_suite_runner [--mode dmg|cgb|sgb|auto] [path/to/roms]
//   --mode auto (default): pick mode from ROM directory name
//                          (cgb_sound/* → cgb, oam_bug/* → dmg, etc.)
//                          falling back to ROM-header autodetect.
//   --mode dmg / cgb / sgb: force the hardware mode for every ROM.
// Default ROM path: /Users/andyvand/Downloads/gb-test-roms-master
//                   (recursive — every .gb / .gbc file is run).
//
// Build target: gb_suite_runner.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "core/base/system.h"
#include "core/base/sound_driver.h"
#include "core/gb/gb.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaSound.h"  // for soundInit()

// ---- System-callback stubs (must be provided by the embedder) --------------

static uint32_t g_joy_mask = 0;

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

// ---- Serial-port capture ---------------------------------------------------
//
// Blargg's tests write each character of their result text to SB (0xFF01) and
// then trigger a transfer by writing 0x81 to SC (0xFF02). The GB core invokes
// `gbSerialFunction(SB)` for every such transfer — we accumulate the bytes
// into a string and surface it as the "test output".

static std::string g_serial_log;
static bool g_verbose = false;
static bool g_dump_screen = false;
static uint8_t serial_capture(uint8_t b) {
    g_serial_log.push_back((char)b);
    if (g_verbose) {
        fprintf(stderr, "[serial 0x%02x '%c']\n", b,
                (b >= 0x20 && b < 0x7f) ? b : '.');
    }
    return 0xFF;
}

// ---- Result detection ------------------------------------------------------

enum class Verdict { Pass, Fail, Timeout, BadRom };

struct TestResult {
    std::string rom_path;
    std::string mode;
    Verdict verdict = Verdict::Timeout;
    std::string detail;   // first line of pass/fail or fingerprint summary
    int frames_run = 0;
};

// Read a single character from the GB BG tile map at (col,row), assuming the
// font tiles are mapped 1:1 with ASCII (Blargg's `shell.s` convention). The
// caller must have already validated that gbVram (CGB) or gbMemory (DMG) is
// initialized.
//
// LCDC bit 3 selects the BG tile map base: 0 → 0x9800, 1 → 0x9C00.
// Each row is 32 entries; the visible area is 20 cols × 18 rows.
//
// Tile map entries are interpreted as raw ASCII codes — characters outside
// the printable range collapse to space so the resulting string stays
// human-readable.
static char read_bg_char(int col, int row) {
    extern uint8_t* gbMemory;
    extern uint8_t* gbVram;
    uint8_t lcdc = gbMemory ? gbMemory[0xFF40] : 0;
    bool map_high = (lcdc & 0x08) != 0;
    int idx = (row & 0x1F) * 32 + (col & 0x1F);
    int map_base = map_high ? 0x1C00 : 0x1800;
    uint8_t tile = 0;
    if (gbCgbMode && gbVram) {
        // CGB has two VRAM banks; the tile map is in bank 0.
        tile = gbVram[map_base + idx];
    } else if (gbMemory) {
        tile = gbMemory[0x8000 + map_base + idx];
    }
    if (tile >= 0x20 && tile < 0x7F) return (char)tile;
    if (tile == 0) return ' ';
    return '.';
}

// Read the entire 32×32 BG tile map and concatenate into a single string
// with rows separated by '\n'. We read the full map (not just the visible
// 20×18 area) because Blargg's wave-channel tests scroll their output
// continuously — the result line ("Passed" / "Failed") often sits on a
// row that's outside the current SCY-scrolled viewport, so a 20×18 read
// would miss it and the runner would time out.
//
// Used to detect Blargg's screen-only result text (cgb_sound, dmg_sound,
// oam_bug, mem_timing-2, halt_bug, interrupt_time).
static std::string read_screen_text() {
    std::string out;
    out.reserve(33 * 32);
    for (int row = 0; row < 32; ++row) {
        for (int col = 0; col < 32; ++col)
            out.push_back(read_bg_char(col, row));
        out.push_back('\n');
    }
    return out;
}

// Returns true if the serial log contains a final-result marker.
// `result` is filled with the matched line.
static bool detect_serial_done(const std::string& log, std::string& result) {
    // Blargg tests typically end their output with one of:
    //   "Passed\n"            (single test)
    //   "Passed all tests\n"  (master roms)
    //   "Failed #N\n"         (single test, N is the failing sub-test)
    //   "Failed\n"            (some tests)
    // Some tests also print "Done.\n" or extra blank lines — we look for the
    // standard markers anywhere in the buffer.
    static const char* markers[] = {
        "Passed all tests", "Passed", "Failed", nullptr
    };
    for (int i = 0; markers[i]; ++i) {
        size_t pos = log.find(markers[i]);
        if (pos != std::string::npos) {
            // Capture the line containing the marker (up to but not including
            // the next '\n' or end of string).
            size_t line_end = log.find('\n', pos);
            if (line_end == std::string::npos)
                line_end = log.size();
            // Trim leading/trailing whitespace from the captured line.
            size_t line_start = log.rfind('\n', pos);
            line_start = (line_start == std::string::npos) ? 0 : line_start + 1;
            result.assign(log, line_start, line_end - line_start);
            return true;
        }
    }
    return false;
}

// ---- Hardware mode selection ----------------------------------------------

static const char* g_mode_force = "auto"; // dmg / cgb / sgb / auto

// gbEmulatorType values per gb.cpp:gbGetHardwareType:
//   0 = auto-detect from ROM header
//   1 = force CGB
//   2 = force SGB
//   3 = force DMG
//   4 = force GBA mode (treat CGB ROM as if running on GBA)
//   5 = force SGB2

static uint32_t pick_emulator_type(const std::string& rom_path) {
    if (std::strcmp(g_mode_force, "dmg") == 0) return 3;
    if (std::strcmp(g_mode_force, "cgb") == 0) return 1;
    if (std::strcmp(g_mode_force, "sgb") == 0) return 2;

    // auto: pick by directory name
    if (rom_path.find("cgb_sound")     != std::string::npos) return 1;  // CGB
    if (rom_path.find("interrupt_time")!= std::string::npos) return 1;  // CGB
    if (rom_path.find("dmg_sound")     != std::string::npos) return 3;  // DMG
    if (rom_path.find("oam_bug")       != std::string::npos) return 3;  // DMG
    // Everything else: 0 = auto-detect from ROM header.
    return 0;
}

static const char* mode_name(uint32_t et) {
    switch (et) {
        case 0: return "auto";
        case 1: return "cgb";
        case 2: return "sgb";
        case 3: return "dmg";
        case 4: return "gba";
        case 5: return "sgb2";
        default: return "?";
    }
}

// ---- Per-ROM driver --------------------------------------------------------

static void run_one_rom(const std::string& rom_path, TestResult& out) {
    out.rom_path = rom_path;
    out.verdict = Verdict::Timeout;
    out.detail.clear();
    out.frames_run = 0;

    uint32_t et = pick_emulator_type(rom_path);
    gbEmulatorType = et;
    out.mode = mode_name(et);

    g_serial_log.clear();
    gbSerialFunction = serial_capture;

    if (!gbLoadRom(rom_path.c_str())) {
        out.verdict = Verdict::BadRom;
        out.detail = "gbLoadRom() failed";
        return;
    }

    if (g_verbose) {
        fprintf(stderr, "[loaded ROM '%s', hardware type after load=%d]\n",
                rom_path.c_str(), gbHardware);
    }

    // Sound init (uses the NullSoundDriver), reset, then run frames until a
    // serial-done marker fires or the timeout elapses.
    soundInit();
    gbReset();
    emulating = 1;

    if (g_verbose) {
        fprintf(stderr, "[after gbReset: gbHardware=%d gbCgbMode=%d "
                        "gbSgbMode=%d emulating=%d gbSerialFunction=%p]\n",
                gbHardware, (int)gbCgbMode, (int)gbSgbMode, emulating,
                (void*)gbSerialFunction);
    }

    // 4194304 cycles/sec / ~70224 cycles/frame ≈ 60 fps.
    // Most Blargg tests finish in under 100 frames; the master ROMs
    // (cpu_instrs, dmg_sound) take ~2K. 4096 frames (~68 sec emulated) is
    // a comfortable budget for everything except a few wave-channel
    // sub-tests that hang on real-HW quirks.
    const int kMaxFrames = 4096;
    const int kCheckEvery = 4;         // check serial log every N frames
    const int kFrameTicks = 70224;     // ~one DMG frame's cycles

    bool done = false;
    int last_screen_change_frame = 0;
    std::string prev_screen;
    auto extract_match_line = [](const std::string& screen, size_t pos)
        -> std::string {
        size_t end = screen.find('\n', pos);
        if (end == std::string::npos) end = screen.size();
        size_t start = pos;
        while (start > 0 && screen[start - 1] != '\n') --start;
        while (start < pos && screen[start] == ' ') ++start;
        std::string line = "[screen] ";
        line.append(screen, start, end - start);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        return line;
    };
    for (int i = 0; i < kMaxFrames && !done; ++i) {
        GBSystem.emuMain(kFrameTicks);
        out.frames_run = i + 1;

        if ((i % kCheckEvery) == (kCheckEvery - 1)) {
            // 1) Serial-port detection (cpu_instrs, instr_timing, mem_timing).
            std::string result_line;
            if (detect_serial_done(g_serial_log, result_line)) {
                out.detail = result_line;
                if (result_line.find("Passed") != std::string::npos)
                    out.verdict = Verdict::Pass;
                else
                    out.verdict = Verdict::Fail;
                done = true;
                continue;
            }

            // 2) Screen-text detection. Two strategies:
            //    a) "stable screen" — the test sits in a halt loop after
            //       writing its result. Wait 16 frames of no change, then
            //       scan for "Passed"/"Failed".
            //    b) "actively-updating screen" — wave-channel quirk tests
            //       (cgb_sound/dmg_sound 10/12) keep redrawing wave RAM,
            //       so the screen never stabilizes. Scan for "Passed"/
            //       "Failed" eagerly even on changes — these tests append
            //       the result line at the end of their output, so once
            //       it appears we exit immediately.
            std::string screen = read_screen_text();
            if (screen != prev_screen) {
                last_screen_change_frame = i;
                prev_screen = screen;
            }
            bool screen_stable = (i - last_screen_change_frame >= 16);
            // Always check for "Passed"/"Failed" — eager exit avoids the
            // timeout on tests that keep redrawing their output buffer.
            size_t p_pass = screen.find("Passed");
            size_t p_fail = screen.find("Failed");
            if (p_pass != std::string::npos) {
                out.detail = extract_match_line(screen, p_pass);
                out.verdict = Verdict::Pass;
                done = true;
            } else if (p_fail != std::string::npos && screen_stable) {
                // Be more conservative on "Failed" — only trust it if the
                // screen has stabilized, since some tests display partial
                // failure indicators mid-run.
                out.detail = extract_match_line(screen, p_fail);
                out.verdict = Verdict::Fail;
                done = true;
            } else if (p_fail != std::string::npos) {
                // Saw "Failed" on an unstable screen — keep waiting in case
                // a later "Passed" supersedes it (rare). If the test runs
                // out of frames with only "Failed" visible, we fall back
                // to FAIL via the timeout path below.
            }
        }
    }

    if (!done) {
        // Timeout. If the screen has a "Failed" anywhere, treat as FAIL —
        // some wave-channel tests keep redrawing their output buffer so
        // we never reached the stable-screen path. Otherwise surface
        // whatever serial / screen text we have.
        std::string final_screen = read_screen_text();
        size_t p_fail = final_screen.find("Failed");
        if (p_fail != std::string::npos) {
            out.detail = "timeout-fail: ";
            size_t end = final_screen.find('\n', p_fail);
            if (end == std::string::npos) end = final_screen.size();
            size_t s = p_fail;
            while (s > 0 && final_screen[s - 1] != '\n') --s;
            while (s < p_fail && final_screen[s] == ' ') ++s;
            out.detail.append(final_screen, s, end - s);
            while (!out.detail.empty() && out.detail.back() == ' ')
                out.detail.pop_back();
            out.verdict = Verdict::Fail;
            done = true;
        }
    }
    if (!done) {
        if (!g_serial_log.empty()) {
            size_t pos = 0;
            while (pos < g_serial_log.size() && g_serial_log[pos] == '\n')
                ++pos;
            size_t end = g_serial_log.find('\n', pos);
            if (end == std::string::npos) end = g_serial_log.size();
            out.detail = "timeout: ";
            out.detail.append(g_serial_log, pos, end - pos);
        } else {
            // Fall back to the first non-empty screen line.
            std::string scr = read_screen_text();
            size_t p = 0;
            while (p < scr.size()) {
                size_t e = scr.find('\n', p);
                if (e == std::string::npos) e = scr.size();
                std::string line(scr, p, e - p);
                while (!line.empty() && line.back() == ' ') line.pop_back();
                size_t s = 0;
                while (s < line.size() && line[s] == ' ') ++s;
                if (s < line.size()) {
                    out.detail = "timeout: [screen] ";
                    out.detail.append(line, s, line.size() - s);
                    break;
                }
                p = e + 1;
            }
            if (out.detail.empty())
                out.detail = "timeout (no serial / blank screen)";
        }
    }

    if (g_dump_screen) {
        std::string scr = read_screen_text();
        fprintf(stderr, "---- final screen text for %s ----\n",
                rom_path.c_str());
        fputs(scr.c_str(), stderr);
        fprintf(stderr, "---- end screen ----\n");
    }

    gbCleanUp();
}

// ---- ROM enumeration -------------------------------------------------------

static bool ends_with_ci(const std::string& s, const char* suffix) {
    size_t n = std::strlen(suffix);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        char a = s[s.size() - n + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

static void collect_roms(const std::string& path, std::vector<std::string>& out) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return;
    if (S_ISREG(st.st_mode)) {
        if (ends_with_ci(path, ".gb") || ends_with_ci(path, ".gbc"))
            out.push_back(path);
        return;
    }
    if (!S_ISDIR(st.st_mode)) return;

    DIR* d = ::opendir(path.c_str());
    if (!d) return;
    struct dirent* e;
    std::vector<std::string> entries;
    while ((e = ::readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue;
        entries.push_back(e->d_name);
    }
    ::closedir(d);
    std::sort(entries.begin(), entries.end());

    for (const std::string& name : entries) {
        std::string child = path + "/" + name;
        collect_roms(child, out);
    }
}

// ---- Output ----------------------------------------------------------------

static const char* verdict_label(Verdict v) {
    switch (v) {
        case Verdict::Pass:    return "PASS";
        case Verdict::Fail:    return "FAIL";
        case Verdict::Timeout: return "TIMEOUT";
        case Verdict::BadRom:  return "BAD";
    }
    return "?";
}

// Strip the user-supplied prefix from the ROM path so the report is short.
static std::string trim_prefix(const std::string& s, const std::string& prefix) {
    if (s.size() > prefix.size() &&
        std::memcmp(s.data(), prefix.data(), prefix.size()) == 0) {
        std::string r = s.substr(prefix.size());
        if (!r.empty() && r[0] == '/') r.erase(0, 1);
        return r;
    }
    return s;
}

// ---- Main ------------------------------------------------------------------

int main(int argc, char** argv) {
    const char* roms_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--mode") == 0 && i + 1 < argc) {
            g_mode_force = argv[++i];
        } else if (std::strcmp(a, "--verbose") == 0) {
            g_verbose = true;
        } else if (std::strcmp(a, "--dump-screen") == 0) {
            g_dump_screen = true;
        } else if (!roms_path) {
            roms_path = a;
        }
    }
    if (!roms_path)
        roms_path = "/Users/andyvand/Downloads/gb-test-roms-master";

    std::vector<std::string> roms;
    collect_roms(roms_path, roms);
    if (roms.empty()) {
        fprintf(stderr, "gb_suite_runner: no .gb / .gbc files under %s\n",
                roms_path);
        return 1;
    }

    fprintf(stderr, "gb_suite_runner: mode=%s, roms_root=%s\n",
            g_mode_force, roms_path);
    fprintf(stderr, "gb_suite_runner: %zu ROMs to run\n\n", roms.size());

    // Initialize the GB color-map LUT. The serial-port-only Blargg tests
    // don't need correct pixels, but render code paths still index these
    // tables and would crash if left uninitialized.
    systemColorDepth = 32;
    systemRedShift   = 3;
    systemGreenShift = 11;
    systemBlueShift  = 19;
    for (int i = 0; i < 0x10000; ++i) {
        const uint32_t r5 = (uint32_t)(i & 0x1F);
        const uint32_t g5 = (uint32_t)((i >> 5) & 0x1F);
        const uint32_t b5 = (uint32_t)((i >> 10) & 0x1F);
        systemColorMap32[i] =
            (r5 << systemRedShift) | (g5 << systemGreenShift) |
            (b5 << systemBlueShift) | 0xFF000000u;
    }

    // Default GB greyscale palette (white → black, BGR555).
    systemGbPalette[ 0] = 0x7FFF;
    systemGbPalette[ 1] = 0x56B5;
    systemGbPalette[ 2] = 0x294A;
    systemGbPalette[ 3] = 0x0000;
    for (int i = 4; i < 24; ++i)
        systemGbPalette[i] = systemGbPalette[i & 3];

    int n_pass = 0, n_fail = 0, n_timeout = 0, n_bad = 0;
    std::vector<TestResult> results;
    results.reserve(roms.size());

    for (size_t i = 0; i < roms.size(); ++i) {
        const std::string& rom = roms[i];
        std::string label = trim_prefix(rom, roms_path);
        TestResult r;
        run_one_rom(rom, r);

        switch (r.verdict) {
            case Verdict::Pass:    ++n_pass;    break;
            case Verdict::Fail:    ++n_fail;    break;
            case Verdict::Timeout: ++n_timeout; break;
            case Verdict::BadRom:  ++n_bad;     break;
        }

        fprintf(stderr, "[%2zu/%2zu] %-7s %-4s %-50s %s\n",
                i + 1, roms.size(),
                verdict_label(r.verdict), r.mode.c_str(),
                label.c_str(),
                r.detail.c_str());

        results.push_back(std::move(r));
    }

    fprintf(stderr, "\n================ gb_suite_runner results ================\n");
    fprintf(stderr, "  PASS    : %d\n", n_pass);
    fprintf(stderr, "  FAIL    : %d\n", n_fail);
    fprintf(stderr, "  TIMEOUT : %d\n", n_timeout);
    fprintf(stderr, "  BAD ROM : %d\n", n_bad);
    fprintf(stderr, "  TOTAL   : %zu\n", results.size());
    fprintf(stderr, "==========================================================\n");

    // stdout: machine-readable summary (one line per ROM).
    printf("# verdict mode frames rom : detail\n");
    for (const TestResult& r : results) {
        std::string label = trim_prefix(r.rom_path, roms_path);
        printf("%-7s %-4s %5d %s : %s\n",
               verdict_label(r.verdict), r.mode.c_str(), r.frames_run,
               label.c_str(), r.detail.c_str());
    }

    return (n_fail == 0 && n_timeout == 0 && n_bad == 0) ? 0 : 2;
}
