// Headless libretro-host runner for mGBA's test suite (suite.gba).
//
// Unlike suite_runner (which links vbam-core directly), this harness
// dlopens the vbam_libretro core and drives it exclusively through the
// libretro API — the same code path RetroArch uses. The libretro build
// compiles its own copy of the emulator sources with libretro-specific
// defines (__LIBRETRO__, NO_LINK, TILED_RENDERING, ...), so accuracy
// regressions can hide there even when the desktop core passes.
//
// Suite state is read through the RETRO_ENVIRONMENT_SET_MEMORY_MAPS
// descriptors the core registers (IWRAM / EWRAM / ROM pointers); video
// tests are compared on the RGB565 frames delivered via video_cb.
//
// Usage: libretro_suite_runner [--min-pass N] <path/to/core> [path/to/suite.gba]

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dlfcn.h>

#include <libretro.h>

// ---- Core entry points (resolved from the dlopened core) -------------------

static void (*p_retro_set_environment)(retro_environment_t);
static void (*p_retro_set_video_refresh)(retro_video_refresh_t);
static void (*p_retro_set_audio_sample)(retro_audio_sample_t);
static void (*p_retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
static void (*p_retro_set_input_poll)(retro_input_poll_t);
static void (*p_retro_set_input_state)(retro_input_state_t);
static void (*p_retro_init)(void);
static void (*p_retro_deinit)(void);
static bool (*p_retro_load_game)(const struct retro_game_info*);
static void (*p_retro_unload_game)(void);
static void (*p_retro_run)(void);
static void (*p_retro_set_controller_port_device)(unsigned, unsigned);
static void* (*p_retro_get_memory_data)(unsigned);
static size_t (*p_retro_get_memory_size)(unsigned);

template <typename T>
static bool resolve(void* handle, const char* name, T& fn) {
    fn = reinterpret_cast<T>(dlsym(handle, name));
    if (!fn) {
        fprintf(stderr, "libretro_suite_runner: missing symbol %s\n", name);
        return false;
    }
    return true;
}

// ---- Host state -------------------------------------------------------------

static uint32_t g_joy_mask = 0; // GBA key order: bit0=A 1=B 2=SEL 3=START
                                // 4=RIGHT 5=LEFT 6=UP 7=DOWN 8=R 9=L

static const uint8_t* g_iwram = nullptr; // 0x03000000, 32 KiB
static const uint8_t* g_ewram = nullptr; // 0x02000000, 256 KiB
static const uint8_t* g_rom   = nullptr; // 0x08000000
static uint32_t g_rom_size = 0;          // actual loaded ROM byte count

static char g_system_dir[1024] = ".";

static enum retro_pixel_format g_pixfmt = RETRO_PIXEL_FORMAT_0RGB1555;

// Latest frame delivered by video_cb, tightly packed.
static constexpr int kGbaPixW = 240;
static constexpr int kGbaPixH = 160;
static std::vector<uint8_t> g_frame;
static unsigned g_frame_w = 0, g_frame_h = 0, g_frame_bpp = 0;

// ---- libretro callbacks ------------------------------------------------------

static void host_log(enum retro_log_level level, const char* fmt, ...) {
    (void)level;
    if (!getenv("LIBRETRO_HOST_VERBOSE"))
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static bool env_cb(unsigned cmd, void* data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<struct retro_log_callback*>(data)->log = host_log;
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = g_system_dir;
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        g_pixfmt = *static_cast<enum retro_pixel_format*>(data);
        return true;
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: {
        const struct retro_memory_map* map =
            static_cast<const struct retro_memory_map*>(data);
        for (unsigned i = 0; i < map->num_descriptors; ++i) {
            const struct retro_memory_descriptor& d = map->descriptors[i];
            if (!d.ptr)
                continue;
            switch (d.start) {
            case 0x03000000u:
                g_iwram = static_cast<const uint8_t*>(d.ptr);
                break;
            case 0x02000000u:
                g_ewram = static_cast<const uint8_t*>(d.ptr);
                break;
            case 0x08000000u:
                g_rom = static_cast<const uint8_t*>(d.ptr);
                break;
            default:
                break;
            }
        }
        return true;
    }
    default:
        return false;
    }
}

static void video_cb(const void* data, unsigned width, unsigned height,
                     size_t pitch) {
    if (!data)
        return; // frame dupe
    unsigned bpp = 2; // both 1555 and 565 are 16-bit; XRGB8888 is 32
    if (g_pixfmt == RETRO_PIXEL_FORMAT_XRGB8888)
        bpp = 4;
    g_frame_w = width;
    g_frame_h = height;
    g_frame_bpp = bpp;
    g_frame.resize((size_t)width * height * bpp);
    const uint8_t* src = static_cast<const uint8_t*>(data);
    for (unsigned y = 0; y < height; ++y)
        std::memcpy(g_frame.data() + (size_t)y * width * bpp,
                    src + (size_t)y * pitch, (size_t)width * bpp);
}

static void audio_sample_cb(int16_t, int16_t) {}
static size_t audio_sample_batch_cb(const int16_t*, size_t frames) {
    return frames;
}
static void input_poll_cb(void) {}

static int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                              unsigned id) {
    (void)index;
    if (port != 0 || device != RETRO_DEVICE_JOYPAD)
        return 0;
    switch (id) {
    case RETRO_DEVICE_ID_JOYPAD_A:      return (g_joy_mask >> 0) & 1;
    case RETRO_DEVICE_ID_JOYPAD_B:      return (g_joy_mask >> 1) & 1;
    case RETRO_DEVICE_ID_JOYPAD_SELECT: return (g_joy_mask >> 2) & 1;
    case RETRO_DEVICE_ID_JOYPAD_START:  return (g_joy_mask >> 3) & 1;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return (g_joy_mask >> 4) & 1;
    case RETRO_DEVICE_ID_JOYPAD_LEFT:   return (g_joy_mask >> 5) & 1;
    case RETRO_DEVICE_ID_JOYPAD_UP:     return (g_joy_mask >> 6) & 1;
    case RETRO_DEVICE_ID_JOYPAD_DOWN:   return (g_joy_mask >> 7) & 1;
    case RETRO_DEVICE_ID_JOYPAD_R:      return (g_joy_mask >> 8) & 1;
    case RETRO_DEVICE_ID_JOYPAD_L:      return (g_joy_mask >> 9) & 1;
    default:                            return 0;
    }
}

// ---- GBA joypad bits ---------------------------------------------------------

static constexpr uint32_t KEY_A    = 1u << 0;
static constexpr uint32_t KEY_B    = 1u << 1;
static constexpr uint32_t KEY_DOWN = 1u << 7;

// ---- Memory helpers ----------------------------------------------------------
//
// Same GBA-address-space-safe dispatch as suite_runner, but through the
// pointers captured from the core's memory-map descriptors.

static uint32_t mem_read32(uint32_t addr) {
    switch (addr >> 24) {
    case 0x02: {
        uint32_t off = (addr - 0x02000000u) & 0x3FFFFu;
        if (!g_ewram) return 0;
        return (uint32_t)g_ewram[off] | ((uint32_t)g_ewram[off + 1] << 8) |
               ((uint32_t)g_ewram[off + 2] << 16) |
               ((uint32_t)g_ewram[off + 3] << 24);
    }
    case 0x03: {
        uint32_t off = (addr - 0x03000000u) & 0x7FFFu;
        if (!g_iwram) return 0;
        return (uint32_t)g_iwram[off] | ((uint32_t)g_iwram[off + 1] << 8) |
               ((uint32_t)g_iwram[off + 2] << 16) |
               ((uint32_t)g_iwram[off + 3] << 24);
    }
    case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x0C: case 0x0D: {
        uint32_t off = (addr - 0x08000000u) & 0x01FFFFFFu;
        if (!g_rom || off + 4 > g_rom_size) return 0;
        return (uint32_t)g_rom[off] | ((uint32_t)g_rom[off + 1] << 8) |
               ((uint32_t)g_rom[off + 2] << 16) |
               ((uint32_t)g_rom[off + 3] << 24);
    }
    default:
        return 0;
    }
}

static uint32_t iwram_read32(uint32_t addr) { return mem_read32(addr); }
static uint32_t rom_read32(uint32_t addr)   { return mem_read32(addr); }

static uint8_t iwram_read_u8(uint32_t addr) {
    if (!g_iwram) return 0;
    return g_iwram[(addr - 0x03000000u) & 0x7FFFu];
}

// ---- Suite table (same layout knowledge as suite_runner) ---------------------

struct SuiteDesc {
    const char* name;
    uint32_t struct_addr;
};

// Fallback matching the suite.gba shipped in src/core/test (gcc13 build).
// Normally unused: discover_suites_in_rom() resolves all 14 structs from
// the loaded ROM at runtime.
static const SuiteDesc kSuites[] = {
    {"memory",        0x0803b6a8},
    {"io-read",       0x0803ae04},
    {"timing",        0x080420d4},
    {"timers",        0x080414f0},
    {"timer-irq",     0x080413d8},
    {"shifter",       0x080405bc},
    {"carry",         0x0802bc74},
    {"multiply-long", 0x0803fc10},
    {"bios-math",     0x0802ab0c},
    {"dma",           0x0802c168},
    {"sio-read",      0x08040d80},
    {"sio-timing",    0x0804133c},
    {"misc-edge",     0x0803f8dc},
    {"video",         0x08045104},
};
static constexpr int kNumSuites = sizeof(kSuites) / sizeof(kSuites[0]);

// Execution order — see suite_runner.cpp for the rationale (dma and
// misc-edge must run before suites that leave timers running or pollute
// the OAM mirror / cumulative HBlank phase).
static const char* const kExecutionOrderNames[] = {
    "dma",
    "misc-edge",
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

// TestSuite struct offsets: +16 nTests, +20 passes*, +24 totalResults*.
struct SuiteCounters {
    uint32_t passes_addr;
    uint32_t total_addr;
    uint32_t n_tests;
};

static SuiteCounters read_suite_counters(uint32_t struct_addr) {
    SuiteCounters c;
    c.n_tests     = rom_read32(struct_addr + 16);
    c.passes_addr = rom_read32(struct_addr + 20);
    c.total_addr  = rom_read32(struct_addr + 24);
    return c;
}

// ---- Dynamic suite-struct discovery (ported from suite_runner) ---------------

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

static const char* const kCanonicalShortNames[] = {
    "memory", "io-read", "timing", "timers", "timer-irq", "shifter",
    "carry", "multiply-long", "bios-math", "dma", "sio-read",
    "sio-timing", "misc-edge", "video",
};

namespace {
struct DiscoveredSuite {
    const char* name = nullptr;
    uint32_t struct_addr = 0;
};

static inline uint32_t rom_off_read32(uint32_t off) {
    return (uint32_t)g_rom[off] | ((uint32_t)g_rom[off + 1] << 8) |
           ((uint32_t)g_rom[off + 2] << 16) | ((uint32_t)g_rom[off + 3] << 24);
}

static std::vector<DiscoveredSuite> discover_suites_in_rom() {
    std::vector<DiscoveredSuite> found(kCanonicalNumSuites);
    if (!g_rom) return found;
    const uint32_t romSize = g_rom_size;
    if (romSize < 32) return found;

    for (uint32_t off = 0; off + 28 <= romSize; off += 4) {
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
            if (std::memcmp(g_rom + name_off, n, nlen + 1) == 0) {
                slot = k;
                break;
            }
        }
        if (slot < 0) continue;
        if (found[slot].struct_addr) continue;

        uint32_t w_run = rom_off_read32(off + 4);
        if (w_run != 0) {
            uint8_t t = (uint8_t)(w_run >> 24);
            if (t < 0x08 || t > 0x0D) continue;
        }
        uint32_t nTests = rom_off_read32(off + 16);
        if (nTests == 0 || nTests > 4096) continue;
        uint32_t pAddr = rom_off_read32(off + 20);
        uint8_t pt = (uint8_t)(pAddr >> 24);
        bool pIWRAM = (pt == 0x03);
        bool pROM   = (pt >= 0x08 && pt <= 0x0D);
        if (!pIWRAM && !pROM) continue;
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

// ---- Emulation driver --------------------------------------------------------

static void run_frames(int n, uint32_t joy = 0) {
    g_joy_mask = joy;
    for (int i = 0; i < n; ++i)
        p_retro_run();
}

static void press_button(uint32_t mask, int hold_frames = 6) {
    run_frames(4, 0);
    run_frames(hold_frames, mask);
    run_frames(4, 0);
}

// ---- activeTestInfo (see suite_runner.cpp for layout details) -----------------

static uint32_t g_active_info_addr = 0;

static uint64_t rom_fnv1a64() {
    if (!g_rom) return 0;
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < g_rom_size; ++i) {
        h ^= g_rom[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

static bool locate_active_info() {
    struct KnownBuild { uint64_t fnv; uint32_t addr; };
    static const KnownBuild kBuilds[] = {
        { 0x33be6dfef0f617d5ull, 0x030000a8u },
        { 0x5c0226907bbefae2ull, 0x030000acu },
    };
    static const uint32_t kCandidates[] = { 0x030000a8, 0x030000ac };

    const uint64_t fnv = rom_fnv1a64();
    for (const KnownBuild& b : kBuilds) {
        if (b.fnv == fnv) {
            g_active_info_addr = b.addr;
            if (iwram_read32(g_active_info_addr) == 0x6f666e49u)
                return true;
            fprintf(stderr,
                    "libretro_suite_runner: known ROM (fnv=%016llx) but no "
                    "'Info' tag at %08x\n",
                    (unsigned long long)fnv, g_active_info_addr);
            g_active_info_addr = 0;
            return false;
        }
    }
    for (uint32_t addr : kCandidates) {
        if (iwram_read32(addr) == 0x6f666e49u) {
            g_active_info_addr = addr;
            fprintf(stderr,
                    "libretro_suite_runner: unknown suite build (fnv=%016llx), "
                    "activeTestInfo located by tag at %08x\n",
                    (unsigned long long)fnv, addr);
            return true;
        }
    }
    fprintf(stderr,
            "libretro_suite_runner: unknown suite build (fnv=%016llx), "
            "activeTestInfo not found; video-suite driving unreliable\n",
            (unsigned long long)fnv);
    g_active_info_addr = 0;
    return false;
}

static int32_t active_suite_id() {
    if (!g_active_info_addr) return -2;
    return (int32_t)(int8_t)iwram_read_u8(g_active_info_addr + 7);
}

static int32_t active_test_id() {
    if (!g_active_info_addr) return -2;
    return (int32_t)(int8_t)iwram_read_u8(g_active_info_addr + 6);
}

// ---- Video-suite driver --------------------------------------------------------
//
// Same strategy as suite_runner, but snapshots come from video_cb frames
// (whatever pixel format the core negotiated) instead of g_pix. The
// Actual/Expected label sprite sits at OBJ_Y(148); exclude rows >= 148.

static constexpr int kLabelTopY = 148;

static size_t compare_bytes() {
    return (size_t)kGbaPixW * kLabelTopY * g_frame_bpp;
}

static void copy_framebuffer(std::vector<uint8_t>& dst) {
    if (g_frame.empty() || g_frame_w != kGbaPixW || g_frame_h != kGbaPixH) {
        dst.assign((size_t)kGbaPixW * kGbaPixH * (g_frame_bpp ? g_frame_bpp : 2),
                   0);
        return;
    }
    dst = g_frame;
}

static void dump_frame_ppm(const std::vector<uint8_t>& px, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", kGbaPixW, kGbaPixH);
    for (int y = 0; y < kGbaPixH; ++y) {
        for (int x = 0; x < kGbaPixW; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            if (g_frame_bpp == 2) {
                uint16_t v;
                std::memcpy(&v, px.data() + 2 * ((size_t)y * kGbaPixW + x), 2);
                // RGB565
                r = (uint8_t)(((v >> 11) & 0x1F) << 3);
                g = (uint8_t)(((v >> 5) & 0x3F) << 2);
                b = (uint8_t)((v & 0x1F) << 3);
            } else if (g_frame_bpp == 4) {
                const uint8_t* p = px.data() + 4 * ((size_t)y * kGbaPixW + x);
                b = p[0]; g = p[1]; r = p[2];
            }
            fputc(r, f); fputc(g, f); fputc(b, f);
        }
    }
    fclose(f);
}

static uint64_t frame_hash(const std::vector<uint8_t>& p) {
    uint64_t h = 0xcbf29ce484222325ull;
    const size_t n = compare_bytes();
    for (size_t i = 0; i + 16 <= n; i += 16) {
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

    std::vector<uint8_t> actual, expected;
    const bool trace = getenv("VIDEO_NAV_TRACE") != nullptr;

    for (int t = 0; t < n_tests; ++t) {
        if (trace) fprintf(stderr, "[t=%d] entry suiteId=%d testId=%d\n",
                           t, active_suite_id(), active_test_id());
        for (int k = 0; k < 4; ++k) {
            if (active_suite_id() < 0) break;
            press_button(KEY_B, 2);
            run_frames(10, 0);
        }

        press_button(KEY_A, 2);
        run_frames(20, 0);

        for (int k = 0; k < t; ++k) {
            press_button(KEY_DOWN, 2);
            run_frames(10, 0);
        }

        fprintf(stderr, "    video[%d/%d] (suiteId=%d)...",
                t + 1, n_tests, active_suite_id());

        press_button(KEY_A, 2);
        run_frames(30, 0);
        copy_framebuffer(actual);
        if (const char* d = getenv("VIDEO_DUMP_DIR")) {
            char p[512]; snprintf(p, sizeof(p), "%s/lr_v%d_actual.ppm", d, t + 1);
            dump_frame_ppm(actual, p);
        }

        press_button(KEY_A, 2);
        run_frames(30, 0);
        copy_framebuffer(expected);
        if (const char* d = getenv("VIDEO_DUMP_DIR")) {
            char p[512]; snprintf(p, sizeof(p), "%s/lr_v%d_expected.ppm", d, t + 1);
            dump_frame_ppm(expected, p);
        }

        const size_t nb = compare_bytes();
        bool pass = actual.size() >= nb && expected.size() >= nb &&
                    std::memcmp(actual.data(), expected.data(), nb) == 0;
        uint64_t ha = frame_hash(actual);
        uint64_t he = frame_hash(expected);
        if (pass) {
            ++s.passes;
            fprintf(stderr, " PASS (hash=%016llx)\n", (unsigned long long)ha);
        } else {
            size_t diff = 0;
            const unsigned bpp = g_frame_bpp ? g_frame_bpp : 2;
            for (size_t k = 0; k + bpp <= nb; k += bpp) {
                if (std::memcmp(actual.data() + k, expected.data() + k, bpp))
                    ++diff;
            }
            fprintf(stderr,
                    " FAIL (%zu pixels differ, hA=%016llx hE=%016llx)\n",
                    diff, (unsigned long long)ha, (unsigned long long)he);
        }
    }
    return s;
}

// ---- SRAM log dump -------------------------------------------------------------

enum class SramDumpMode { kFull, kFailuresOnly, kNone };

static void dump_sram_log(FILE* out, SramDumpMode mode) {
    if (mode == SramDumpMode::kNone) return;

    const uint8_t* sram =
        static_cast<const uint8_t*>(p_retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
    size_t sram_size = p_retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (!sram || !sram_size) {
        fputs("---- SRAM log: unavailable ----\n", out);
        return;
    }
    if (sram_size > 0x8000) sram_size = 0x8000;

    std::string text;
    text.reserve(sram_size);
    for (size_t i = 0; i < sram_size; ++i) {
        uint8_t c = sram[i];
        if (c == '\n' || c == '\t' || (c >= 0x20 && c <= 0x7E))
            text.push_back(static_cast<char>(c));
    }
    while (!text.empty() &&
           (text.back() == '\n' || text.back() == '\t' || text.back() == ' '))
        text.pop_back();
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
    if (shown == 0)
        fputs("(no FAIL lines in SRAM log)\n", out);
    fputs("---- end failed sub-tests ----\n", out);
}

// ---- Main -----------------------------------------------------------------------

int main(int argc, char** argv) {
    const char* core_path = nullptr;
    const char* rom_path = nullptr;
    int min_pass = -1;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--min-pass") == 0 && i + 1 < argc) {
            min_pass = std::atoi(argv[++i]);
        } else if (!core_path) {
            core_path = a;
        } else if (!rom_path) {
            rom_path = a;
        }
    }
    if (!core_path) {
        fprintf(stderr,
                "usage: libretro_suite_runner [--min-pass N] <core> [suite.gba]\n");
        return 1;
    }
    if (!rom_path)
        rom_path = "/Users/andyvand/Downloads/suite-master/suite.gba";

    void* core = dlopen(core_path, RTLD_NOW | RTLD_LOCAL);
    if (!core) {
        fprintf(stderr, "libretro_suite_runner: dlopen(%s) failed: %s\n",
                core_path, dlerror());
        return 1;
    }
    if (!resolve(core, "retro_set_environment", p_retro_set_environment) ||
        !resolve(core, "retro_set_video_refresh", p_retro_set_video_refresh) ||
        !resolve(core, "retro_set_audio_sample", p_retro_set_audio_sample) ||
        !resolve(core, "retro_set_audio_sample_batch",
                 p_retro_set_audio_sample_batch) ||
        !resolve(core, "retro_set_input_poll", p_retro_set_input_poll) ||
        !resolve(core, "retro_set_input_state", p_retro_set_input_state) ||
        !resolve(core, "retro_init", p_retro_init) ||
        !resolve(core, "retro_deinit", p_retro_deinit) ||
        !resolve(core, "retro_load_game", p_retro_load_game) ||
        !resolve(core, "retro_unload_game", p_retro_unload_game) ||
        !resolve(core, "retro_run", p_retro_run) ||
        !resolve(core, "retro_set_controller_port_device",
                 p_retro_set_controller_port_device) ||
        !resolve(core, "retro_get_memory_data", p_retro_get_memory_data) ||
        !resolve(core, "retro_get_memory_size", p_retro_get_memory_size)) {
        return 1;
    }

    // Load the ROM file.
    FILE* f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "libretro_suite_runner: cannot open %s\n", rom_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long rom_bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> rom_buf(rom_bytes);
    if (fread(rom_buf.data(), 1, rom_bytes, f) != (size_t)rom_bytes) {
        fprintf(stderr, "libretro_suite_runner: short read on %s\n", rom_path);
        fclose(f);
        return 1;
    }
    fclose(f);
    g_rom_size = (uint32_t)rom_bytes;

    p_retro_set_environment(env_cb);
    p_retro_set_video_refresh(video_cb);
    p_retro_set_audio_sample(audio_sample_cb);
    p_retro_set_audio_sample_batch(audio_sample_batch_cb);
    p_retro_set_input_poll(input_poll_cb);
    p_retro_set_input_state(input_state_cb);

    p_retro_init();

    struct retro_game_info game;
    std::memset(&game, 0, sizeof(game));
    game.path = rom_path;
    game.data = rom_buf.data();
    game.size = (size_t)rom_bytes;
    if (!p_retro_load_game(&game)) {
        fprintf(stderr, "libretro_suite_runner: retro_load_game failed\n");
        return 1;
    }
    p_retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    if (!g_iwram || !g_rom) {
        fprintf(stderr,
                "libretro_suite_runner: core did not provide IWRAM/ROM memory "
                "descriptors (SET_MEMORY_MAPS)\n");
        return 1;
    }
    fprintf(stderr, "libretro_suite_runner: core=%s pixfmt=%d\n",
            core_path, (int)g_pixfmt);

    // Boot past crt0 + splash.
    run_frames(180, 0);

    if (!locate_active_info()) {
        fprintf(stderr,
                "libretro_suite_runner: could not locate activeTestInfo tag\n");
    } else {
        fprintf(stderr, "libretro_suite_runner: activeTestInfo at 0x%08x\n",
                g_active_info_addr);
    }

    // Resolve TestSuite struct addresses (dynamic discovery, hardcoded
    // fallback per slot).
    struct ResolvedSuite { const char* name; uint32_t struct_addr; };
    ResolvedSuite resolved[kNumSuites];
    {
        auto discovered = discover_suites_in_rom();
        int discovered_count = 0;
        for (int i = 0; i < kNumSuites; ++i) {
            if (discovered[i].struct_addr) {
                resolved[i].name = kCanonicalShortNames[i];
                resolved[i].struct_addr = discovered[i].struct_addr;
                ++discovered_count;
            } else {
                resolved[i].name = kSuites[i].name;
                resolved[i].struct_addr = kSuites[i].struct_addr;
            }
        }
        fprintf(stderr,
                "libretro_suite_runner: discovered %d/%d TestSuite addresses\n",
                discovered_count, kNumSuites);
    }

    SuiteCounters counters[kNumSuites];
    for (int i = 0; i < kNumSuites; ++i) {
        counters[i] = read_suite_counters(resolved[i].struct_addr);
        fprintf(stderr,
                "libretro_suite_runner: %-14s struct=%08x passes=%08x "
                "total=%08x nTests=%u\n",
                resolved[i].name, resolved[i].struct_addr,
                counters[i].passes_addr, counters[i].total_addr,
                counters[i].n_tests);
    }

    uint32_t override_passes[kNumSuites] = {0};
    uint32_t override_total [kNumSuites] = {0};
    bool     have_override  [kNumSuites] = {false};

    int order[kNumSuites];
    int order_n = 0;
    {
        bool used[kNumSuites] = {false};
        for (int k = 0; k < kExecutionOrderN && order_n < kNumSuites; ++k) {
            for (int i = 0; i < kNumSuites; ++i) {
                if (!used[i] && std::strcmp(kCanonicalShortNames[i],
                                            kExecutionOrderNames[k]) == 0) {
                    order[order_n++] = i;
                    used[i] = true;
                    break;
                }
            }
        }
        for (int i = 0; i < kNumSuites; ++i)
            if (!used[i]) order[order_n++] = i;
    }

    int cursor = 0;
    for (int o = 0; o < order_n; ++o) {
        int i = order[o];

        int steps = ((i - cursor) % kNumSuites + kNumSuites) % kNumSuites;
        for (int s = 0; s < steps; ++s)
            press_button(KEY_DOWN, 6);
        cursor = i;

        fprintf(stderr, "\n[%d/%d] running %s (menu pos %d)...\n",
                o + 1, order_n, resolved[i].name, i);

        press_button(KEY_A, 8);

        const bool is_video = (counters[i].passes_addr >> 24) != 0x03;
        if (is_video) {
            VideoStats vs = drive_video_suite((int)counters[i].n_tests);
            override_passes[i] = vs.passes;
            override_total[i]  = vs.total;
            have_override[i]   = true;
            fprintf(stderr, "    %s: %u / %u\n",
                    resolved[i].name, vs.passes, vs.total);
            press_button(KEY_B, 8);
            continue;
        }

        int waited = 0;
        while (active_suite_id() < 0 && waited < 60) {
            run_frames(1, 0);
            ++waited;
        }

        uint32_t last_passes = 0, last_total = 0;
        int stable = 0;
        for (int frame = 0; frame < 7200; ++frame) {
            run_frames(1, 0);
            uint32_t p = iwram_read32(counters[i].passes_addr);
            uint32_t t = iwram_read32(counters[i].total_addr);
            if (p == last_passes && t == last_total && t > 0) {
                if (++stable >= 180) break;
            } else {
                stable = 0;
            }
            last_passes = p;
            last_total = t;
        }

        uint32_t passes = iwram_read32(counters[i].passes_addr);
        uint32_t total  = iwram_read32(counters[i].total_addr);
        fprintf(stderr, "    %s: %u / %u\n", resolved[i].name, passes, total);

        press_button(KEY_B, 6);
        int back_wait = 0;
        while (active_suite_id() >= 0 && back_wait < 60) {
            run_frames(1, 0);
            ++back_wait;
        }
    }

    puts("\n============ suite.gba results (libretro) ============");
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
    puts("======================================================\n");
    fflush(stdout);

    SramDumpMode dump_mode =
        (min_pass >= 0) ? SramDumpMode::kFailuresOnly : SramDumpMode::kFull;
    if (const char* m = std::getenv("VBAM_SRAM_DUMP")) {
        if (std::strcmp(m, "full") == 0)          dump_mode = SramDumpMode::kFull;
        else if (std::strcmp(m, "failures") == 0) dump_mode = SramDumpMode::kFailuresOnly;
        else if (std::strcmp(m, "none") == 0)     dump_mode = SramDumpMode::kNone;
    }
    dump_sram_log(stdout, dump_mode);

    p_retro_unload_game();
    p_retro_deinit();

    if (min_pass >= 0) {
        if ((int)grand_pass >= min_pass) {
            fprintf(stderr, "[ci] PASS %u >= floor %d — OK\n",
                    grand_pass, min_pass);
            return 0;
        }
        fprintf(stderr, "[ci] PASS %u < floor %d — REGRESSION\n",
                grand_pass, min_pass);
        return 2;
    }
    return (grand_total > 0 && grand_pass == grand_total) ? 0 : 2;
}
