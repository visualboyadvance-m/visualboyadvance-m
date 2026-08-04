// Two-process harness for debugging GBA game link sessions (Pokémon
// Emerald trade/battle) over the IPC and socket cable transports.
//
// Forks two full GBA cores, loads the same ROM with per-player battery
// files, establishes the requested link transport, then drives each core
// from a per-player input script while pacing both processes to the same
// wall-clock frame rate. Frames can be dumped as PNGs for inspection and
// the SIO register block is diff-traced per frame to stderr. For
// per-transfer visibility set VBAM_TRACE_CABLE=1 (core-side tracing in
// gbaLink.cpp) — the per-frame trace here is far too coarse for a game
// that clocks nine transfers per frame.
//
// This is a debugging tool, not a ctest: it needs a commercial ROM. It is
// only registered as a test when the VBAM_TEST_EMERALD_* cache variables
// point at real files (see CMakeLists.txt).
//
// Usage:
//   emerald_link_runner --mode ipc|socket --rom <path>
//                       --sav0 <path> --sav1 <path>
//                       --script0 <path> --script1 <path>
//                       [--dump <dir>] [--speed <x>] [--bios <path>]
//                       [--timeout <ms>] [--watchdog <s>] [--trace]
//                       [--watch NAME=0xADDR[:n]]...
//
// Script commands (one per line, '#' comments):
//   run N            run N frames with the currently-held keys
//   press KEYS N     4 idle frames, N frames with KEYS held, 4 idle
//   hold KEYS        set held-key mask ('none' clears)
//   release          clear held-key mask
//   dump NAME        write <dump>/p<id>_<frame>_NAME.png
//   savewrite NAME   write battery to <dump>/p<id>_NAME.sav (never touches
//                    the input .sav; use to persist a better start position)
//   speed X          change pacing multiplier (0 = unthrottled)
//   msg TEXT         log TEXT to stderr with the current frame number
//   waitpeer         pipe barrier with the peer (pre-link phases only!)
// After EOF the process dumps a final frame and exits.
//
// KEYS: comma-separated from A,B,SELECT,START,RIGHT,LEFT,UP,DOWN,R,L
//
// --watch NAME=0xADDR[:n] prints n bytes (default 4) of EWRAM/IWRAM every
// frame they change; addresses are GBA bus addresses (0x02...../0x03.....).

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#include "core/base/sound_driver.h"
#include "core/base/system.h"
#include "core/gba/gba.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaLink.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"

// ---- System-callback stubs --------------------------------------------------

static uint32_t g_joy_mask = 0;
static int g_player = 0;      // 0 = parent/master/server, 1 = child/slave
static long g_frame = 0;
static const char* g_dump_dir = ".";

struct CoreOptions coreOptions;

void systemMessage(int, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[P%d f%06ld] systemMessage: ", g_player, g_frame);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
void log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[P%d f%06ld] corelog: ", g_player, g_frame);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
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
void systemScreenMessage(const char* msg) {
    fprintf(stderr, "[P%d f%06ld] screenMessage: %s\n", g_player, g_frame, msg);
}
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

// Real GBA frame: 228 scanlines x 1232 ticks (same value suite_runner uses).
static constexpr int TICKS_PER_FRAME = 280896;

// Both processes run this binary; after fork() a failure must not unwind
// into the other role's cleanup, so every post-fork error path uses this.
static void die(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[P%d f%06ld] FATAL: ", g_player, g_frame);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    _exit(1);
}

// ---- Key names --------------------------------------------------------------

static const struct { const char* name; uint32_t bit; } kKeys[] = {
    { "A", 1u << 0 }, { "B", 1u << 1 }, { "SELECT", 1u << 2 },
    { "START", 1u << 3 }, { "RIGHT", 1u << 4 }, { "LEFT", 1u << 5 },
    { "UP", 1u << 6 }, { "DOWN", 1u << 7 }, { "R", 1u << 8 },
    { "L", 1u << 9 },
};

static uint32_t parse_keys(const char* s) {
    if (!strcasecmp(s, "none"))
        return 0;
    uint32_t mask = 0;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", s);
    for (char* tok = strtok(buf, ","); tok; tok = strtok(nullptr, ",")) {
        bool found = false;
        for (const auto& k : kKeys)
            if (!strcasecmp(tok, k.name)) {
                mask |= k.bit;
                found = true;
            }
        if (!found)
            die("unknown key '%s'", tok);
    }
    return mask;
}

// ---- PNG dump (2x nearest-neighbor scale for readable dialog text) ----------

static constexpr int kW = 240, kH = 160;

static void png_chunk(FILE* f, const char* type, const uint8_t* data, uint32_t len) {
    uint8_t hdr[8] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16),
                       (uint8_t)(len >> 8), (uint8_t)len,
                       (uint8_t)type[0], (uint8_t)type[1],
                       (uint8_t)type[2], (uint8_t)type[3] };
    fwrite(hdr, 1, 8, f);
    if (len)
        fwrite(data, 1, len, f);
    uLong crc = crc32(0, hdr + 4, 4);
    if (len)
        crc = crc32(crc, data, len);
    uint8_t cb[4] = { (uint8_t)(crc >> 24), (uint8_t)(crc >> 16),
                      (uint8_t)(crc >> 8), (uint8_t)crc };
    fwrite(cb, 1, 4, f);
}

static void dump_png(const char* path) {
    if (!g_pix)
        return;
    const int W = kW * 2, H = kH * 2;
    // Raw image: per-row filter byte + RGB triples.
    std::vector<uint8_t> raw((size_t)H * (1 + W * 3));
    const uint32_t* src = (const uint32_t*)g_pix + 241; // skip pad row
    for (int y = 0; y < H; ++y) {
        uint8_t* out = raw.data() + (size_t)y * (1 + W * 3);
        *out++ = 0; // filter: none
        const uint32_t* row = src + 241 * (y / 2);
        for (int x = 0; x < W; ++x) {
            uint32_t px = row[x / 2];
            *out++ = (uint8_t)(px);
            *out++ = (uint8_t)(px >> 8);
            *out++ = (uint8_t)(px >> 16);
        }
    }
    uLongf zlen = compressBound((uLong)raw.size());
    std::vector<uint8_t> z(zlen);
    if (compress2(z.data(), &zlen, raw.data(), (uLong)raw.size(), 6) != Z_OK)
        return;
    FILE* f = fopen(path, "wb");
    if (!f)
        return;
    static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = {
        (uint8_t)(W >> 24), (uint8_t)(W >> 16), (uint8_t)(W >> 8), (uint8_t)W,
        (uint8_t)(H >> 24), (uint8_t)(H >> 16), (uint8_t)(H >> 8), (uint8_t)H,
        8, 2 /* truecolor */, 0, 0, 0
    };
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z.data(), (uint32_t)zlen);
    png_chunk(f, "IEND", nullptr, 0);
    fclose(f);
    fprintf(stderr, "[P%d f%06ld] dumped %s\n", g_player, g_frame, path);
}

// ---- SIO / RAM tracing -------------------------------------------------------

static bool g_trace = false;

static uint16_t io16(uint32_t off) {
    return (uint16_t)(g_ioMem[off] | (g_ioMem[off + 1] << 8));
}

static void trace_sio() {
    static uint16_t last[8] = { 0xdead, 0xdead, 0xdead, 0xdead,
                                0xdead, 0xdead, 0xdead, 0xdead };
    // SIOMULTI0-3, SIOCNT, SIODATA8, RCNT
    uint16_t cur[8] = { io16(0x120), io16(0x122), io16(0x124), io16(0x126),
                        io16(0x128), io16(0x12a), io16(0x134), 0 };
    if (memcmp(cur, last, sizeof(cur)) != 0) {
        fprintf(stderr,
            "[P%d f%06ld] M0=%04x M1=%04x M2=%04x M3=%04x SIOCNT=%04x "
            "DATA8=%04x RCNT=%04x\n",
            g_player, g_frame, cur[0], cur[1], cur[2], cur[3], cur[4],
            cur[5], cur[6]);
        memcpy(last, cur, sizeof(cur));
    }
}

// Watched GBA RAM ranges: printed on any change, once per frame.
struct Watch {
    std::string name;
    uint32_t addr = 0;
    int len = 4;
    std::vector<uint8_t> last;
};
static std::vector<Watch> g_watches;

static const uint8_t* gba_ram_ptr(uint32_t addr, int len) {
    if (addr >= 0x02000000 && addr + len <= 0x02040000 && g_workRAM)
        return g_workRAM + (addr - 0x02000000);
    if (addr >= 0x03000000 && addr + len <= 0x03008000 && g_internalRAM)
        return g_internalRAM + (addr - 0x03000000);
    return nullptr;
}

static void check_watches() {
    for (auto& w : g_watches) {
        const uint8_t* p = gba_ram_ptr(w.addr, w.len);
        if (!p)
            continue;
        if (w.last.size() == (size_t)w.len && !memcmp(w.last.data(), p, w.len))
            continue;
        w.last.assign(p, p + w.len);
        char hex[3 * 64 + 1] = { 0 };
        for (int i = 0; i < w.len && i < 64; ++i)
            snprintf(hex + 3 * i, 4, "%02x ", p[i]);
        fprintf(stderr, "[P%d f%06ld] watch %s @%08x: %s\n", g_player,
            g_frame, w.name.c_str(), w.addr, hex);
    }
}

// ---- Watchdog ----------------------------------------------------------------

// A wedged run (deadlocked barrier, hung semaphore wait) is killed by
// SIGALRM; dump the screen first so the post-mortem has evidence. The
// handler is not async-signal-safe (fopen/zlib) but the process is about
// to die anyway and the evidence is the whole point of this tool.
static void watchdog_fire(int) {
    char out[512];
    snprintf(out, sizeof(out), "%s/p%d_%06ld_watchdog.png", g_dump_dir,
        g_player, g_frame);
    dump_png(out);
    if (g_ioMem)
        fprintf(stderr,
            "[P%d f%06ld] WATCHDOG: M0=%04x M1=%04x M2=%04x M3=%04x "
            "SIOCNT=%04x DATA8=%04x RCNT=%04x IF=%04x IE=%04x\n",
            g_player, g_frame, io16(0x120), io16(0x122), io16(0x124),
            io16(0x126), io16(0x128), io16(0x12a), io16(0x134),
            io16(0x202), io16(0x200));
    _exit(2);
}

// ---- Frame pacing / running --------------------------------------------------

static double g_speed = 3.0; // x realtime; 0 = unthrottled

static double now_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static double g_next_frame_at = 0;

static void run_frames(long n) {
    for (long i = 0; i < n; ++i) {
        if (g_speed > 0) {
            double now = now_seconds();
            if (g_next_frame_at <= 0)
                g_next_frame_at = now;
            if (now < g_next_frame_at)
                usleep((useconds_t)((g_next_frame_at - now) * 1e6));
            g_next_frame_at += 1.0 / (60.0 * g_speed);
            // Never build up a backlog after a blocking link wait.
            if (g_next_frame_at < now_seconds() - 0.25)
                g_next_frame_at = now_seconds();
        }
        GBASystem.emuMain(TICKS_PER_FRAME);
        ++g_frame;
        if (g_trace)
            trace_sio();
        check_watches();
    }
}

// ---- Barrier (pipe pair) ------------------------------------------------------

class Barrier {
  public:
    Barrier() {
        if (pipe(p2c_) != 0 || pipe(c2p_) != 0) {
            perror("pipe");
            _exit(1);
        }
    }
    // Call once right after fork(): fixes the role and closes the fd ends
    // this side doesn't use, so a dead peer turns "hang until watchdog"
    // into an immediate EOF/EPIPE.
    void set_child(bool is_child) {
        child_ = is_child;
        close(child_ ? p2c_[1] : p2c_[0]);
        close(child_ ? c2p_[0] : c2p_[1]);
    }
    void sync() {
        int wfd = child_ ? c2p_[1] : p2c_[1];
        int rfd = child_ ? p2c_[0] : c2p_[0];
        uint8_t byte = 0;
        if (write(wfd, &byte, 1) != 1 || read(rfd, &byte, 1) != 1)
            die("barrier peer vanished");
    }

    // End-of-script rendezvous that KEEPS EMULATING while waiting: a plain
    // sync() would park this process while its game is still linked, and
    // whichever side finished first in wall time would then CloseLink and
    // rip the session out from under the peer's still-running game (the
    // in-game "Communication error..." artifact at every run's tail).
    // run_frame is called until the peer's byte arrives.
    void sync_running(void (*run_frame)()) {
        int wfd = child_ ? c2p_[1] : p2c_[1];
        int rfd = child_ ? p2c_[0] : c2p_[0];
        uint8_t byte = 0;
        if (write(wfd, &byte, 1) != 1)
            die("barrier peer vanished");
        int flags = fcntl(rfd, F_GETFL, 0);
        fcntl(rfd, F_SETFL, flags | O_NONBLOCK);
        for (;;) {
            ssize_t n = read(rfd, &byte, 1);
            if (n == 1)
                break;
            if (n == 0)
                die("barrier peer vanished");
            run_frame();
        }
        fcntl(rfd, F_SETFL, flags);
    }

  private:
    int p2c_[2];
    int c2p_[2];
    bool child_ = false;
};

// ---- Main --------------------------------------------------------------------

struct Args {
    const char* mode = "ipc";
    const char* rom = nullptr;
    const char* sav[2] = { nullptr, nullptr };
    const char* script[2] = { nullptr, nullptr };
    const char* dump = ".";
    const char* bios = nullptr;
    int timeout_ms = 1000;
    unsigned watchdog_s = 900;
};

static void pump_connect(const char* what) {
    char msg[256] = { 0 };
    double deadline = now_seconds() + 30.0;
    for (;;) {
        ConnectionState st = ConnectLinkUpdate(msg, sizeof(msg));
        if (st == LINK_OK)
            return;
        if (st != LINK_NEEDS_UPDATE)
            die("%s connect failed: %d (%s)", what, (int)st, msg);
        if (now_seconds() > deadline)
            die("%s connect timed out (%s)", what, msg);
        usleep(20 * 1000);
    }
}

static void run_script(const Args& a, Barrier& b) {
    const char* path = a.script[g_player];
    FILE* f = fopen(path, "r");
    if (!f)
        die("cannot open script %s", path);
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;
        char cmd[64], arg1[256];
        long n = 0;
        if (sscanf(p, "%63s", cmd) != 1)
            continue;
        if (!strcmp(cmd, "run")) {
            if (sscanf(p, "%*s %ld", &n) == 1)
                run_frames(n);
        } else if (!strcmp(cmd, "press")) {
            if (sscanf(p, "%*s %255s %ld", arg1, &n) == 2) {
                g_joy_mask = 0;
                run_frames(4);
                g_joy_mask = parse_keys(arg1);
                run_frames(n);
                g_joy_mask = 0;
                run_frames(4);
            }
        } else if (!strcmp(cmd, "hold")) {
            if (sscanf(p, "%*s %255s", arg1) == 1)
                g_joy_mask = parse_keys(arg1);
        } else if (!strcmp(cmd, "release")) {
            g_joy_mask = 0;
        } else if (!strcmp(cmd, "dump")) {
            if (sscanf(p, "%*s %255s", arg1) == 1) {
                char out[512];
                snprintf(out, sizeof(out), "%s/p%d_%06ld_%s.png", a.dump,
                    g_player, g_frame, arg1);
                dump_png(out);
            }
        } else if (!strcmp(cmd, "savewrite")) {
            if (sscanf(p, "%*s %255s", arg1) == 1) {
                char out[512];
                snprintf(out, sizeof(out), "%s/p%d_%s.sav", a.dump,
                    g_player, arg1);
                if (CPUWriteBatteryFile(out))
                    fprintf(stderr, "[P%d f%06ld] battery written to %s\n",
                        g_player, g_frame, out);
                else
                    fprintf(stderr, "[P%d f%06ld] battery write FAILED: %s\n",
                        g_player, g_frame, out);
            }
        } else if (!strcmp(cmd, "speed")) {
            double s = 0;
            if (sscanf(p, "%*s %lf", &s) == 1) {
                g_speed = s;
                g_next_frame_at = 0;
            }
        } else if (!strcmp(cmd, "msg")) {
            char* nl = strchr(p, '\n');
            if (nl)
                *nl = 0;
            char* text = p + strlen("msg");
            while (*text == ' ' || *text == '\t')
                text++;
            fprintf(stderr, "[P%d f%06ld] script: %s\n", g_player, g_frame,
                text);
        } else if (!strcmp(cmd, "waitpeer")) {
            b.sync();
        } else {
            die("unknown script command '%s'", cmd);
        }
    }
    fclose(f);
    fprintf(stderr, "[P%d f%06ld] script finished\n", g_player, g_frame);
}

static void parse_watch(const char* spec) {
    // NAME=0xADDR[:n]
    Watch w;
    const char* eq = strchr(spec, '=');
    if (!eq) {
        fprintf(stderr, "bad --watch spec '%s' (want NAME=0xADDR[:n])\n", spec);
        exit(1);
    }
    w.name.assign(spec, eq - spec);
    char* end = nullptr;
    w.addr = (uint32_t)strtoul(eq + 1, &end, 0);
    if (end && *end == ':')
        w.len = atoi(end + 1);
    if (w.len < 1 || w.len > 64) {
        fprintf(stderr, "--watch %s: len must be 1..64\n", spec);
        exit(1);
    }
    if (!((w.addr >= 0x02000000 && w.addr < 0x02040000) ||
          (w.addr >= 0x03000000 && w.addr < 0x03008000))) {
        fprintf(stderr, "--watch %s: address must be EWRAM/IWRAM\n", spec);
        exit(1);
    }
    g_watches.push_back(std::move(w));
}

int main(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for %s\n", argv[i]);
                exit(1);
            }
            return argv[++i];
        };
        if (!strcmp(argv[i], "--mode")) a.mode = next();
        else if (!strcmp(argv[i], "--rom")) a.rom = next();
        else if (!strcmp(argv[i], "--sav0")) a.sav[0] = next();
        else if (!strcmp(argv[i], "--sav1")) a.sav[1] = next();
        else if (!strcmp(argv[i], "--script0")) a.script[0] = next();
        else if (!strcmp(argv[i], "--script1")) a.script[1] = next();
        else if (!strcmp(argv[i], "--dump")) a.dump = next();
        else if (!strcmp(argv[i], "--bios")) a.bios = next();
        else if (!strcmp(argv[i], "--speed")) g_speed = atof(next());
        else if (!strcmp(argv[i], "--timeout")) a.timeout_ms = atoi(next());
        else if (!strcmp(argv[i], "--watchdog")) a.watchdog_s = (unsigned)atoi(next());
        else if (!strcmp(argv[i], "--trace")) g_trace = true;
        else if (!strcmp(argv[i], "--watch")) parse_watch(next());
        else {
            fprintf(stderr, "unknown arg %s\n", argv[i]);
            return 1;
        }
    }
    if (!a.rom || !a.sav[0] || !a.sav[1] || !a.script[0] || !a.script[1]) {
        fprintf(stderr,
            "required: --rom --sav0 --sav1 --script0 --script1\n");
        return 1;
    }
    g_dump_dir = a.dump;
    bool socket_mode = !strcmp(a.mode, "socket");

    // Isolate the IPC namespace from any real emulator instances.
    char ns[16];
    snprintf(ns, sizeof(ns), "eld%d", (int)(getpid() % 100000));
    setenv("VBAM_LINK_NAMESPACE", ns, 1);

    Barrier b;
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    bool child = (pid == 0);
    b.set_child(child);
    g_player = child ? 1 : 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = watchdog_fire;
    sigaction(SIGALRM, &sa, nullptr);
    alarm(a.watchdog_s);

    // ---- Core bring-up ----
    FILE* rf = fopen(a.rom, "rb");
    if (!rf)
        die("cannot open ROM %s", a.rom);
    fseek(rf, 0, SEEK_END);
    long rom_bytes = ftell(rf);
    fseek(rf, 0, SEEK_SET);
    std::vector<char> rom_buf(rom_bytes);
    if (fread(rom_buf.data(), 1, rom_bytes, rf) != (size_t)rom_bytes)
        die("short ROM read");
    fclose(rf);

    coreOptions.saveType = 3; // flash
    coreOptions.useBios = a.bios ? 1 : 0;
    coreOptions.skipBios = true;
    coreOptions.rtcEnabled = 1;

    if (!CPULoadRomData(rom_buf.data(), (int)rom_bytes))
        die("CPULoadRomData failed");
    flashSetSize(0x20000);
    rtcEnable(true);
    if (a.bios)
        CPUInit(a.bios, true);
    else
        CPUInit("", false);
    SetSaveType(3);
    soundInit();
    CPUReset();
    if (!CPUReadBatteryFile(a.sav[g_player]))
        die("battery load failed: %s", a.sav[g_player]);
    fprintf(stderr, "[P%d] loaded ROM + battery %s\n", g_player,
        a.sav[g_player]);
    emulating = 1;

    systemColorDepth = 32;
    systemRedShift = 3;
    systemGreenShift = 11;
    systemBlueShift = 19;
    for (int i = 0; i < 0x10000; ++i) {
        const uint32_t r5 = (uint32_t)(i & 0x1F);
        const uint32_t g5 = (uint32_t)((i >> 5) & 0x1F);
        const uint32_t b5 = (uint32_t)((i >> 10) & 0x1F);
        systemColorMap32[i] = (r5 << systemRedShift) |
            (g5 << systemGreenShift) | (b5 << systemBlueShift) | 0xFF000000u;
    }

    // ---- Link bring-up ----
    SetLinkTimeout(a.timeout_ms);
    if (socket_mode) {
        if (!child) {
            EnableLinkServer(true, 1);
            ConnectionState st = InitLink(LINK_CABLE_SOCKET);
            if (st != LINK_NEEDS_UPDATE)
                die("server InitLink returned %d", (int)st);
            b.sync();
            pump_connect("server");
        } else {
            b.sync(); // wait for the server to be listening
            EnableLinkServer(false, 0);
            SetLinkServerHost("127.0.0.1");
            ConnectionState st = InitLink(LINK_CABLE_SOCKET);
            if (st != LINK_NEEDS_UPDATE)
                die("client InitLink returned %d", (int)st);
            pump_connect("client");
        }
    } else {
        // Deterministic role assignment: parent inits first.
        if (!child) {
            ConnectionState st = InitLink(LINK_CABLE_IPC);
            if (st != LINK_OK)
                die("InitLink returned %d", (int)st);
            b.sync();
        } else {
            b.sync();
            ConnectionState st = InitLink(LINK_CABLE_IPC);
            if (st != LINK_OK)
                die("InitLink returned %d", (int)st);
        }
    }
    fprintf(stderr, "[P%d] link up (player id %d, mode %s)\n", g_player,
        GetLinkPlayerId(), a.mode);
    b.sync();

    run_script(a, b);

    // Final screen state for post-mortem.
    char out[512];
    snprintf(out, sizeof(out), "%s/p%d_%06ld_final.png", a.dump, g_player,
        g_frame);
    dump_png(out);

    // Keep the session alive until the peer's script is also done.
    b.sync_running([] { run_frames(1); });

    CloseLink();
    if (child)
        _exit(0);
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[P0] child exited with status 0x%x\n", status);
        return 1;
    }
    fprintf(stderr, "[P0] both players completed their scripts\n");
    return 0;
}
