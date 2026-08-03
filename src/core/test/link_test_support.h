// Shared scaffolding for the two-process link tests (link_ipc_test and
// link_socket_runner): the system-callback stubs vbam-core expects its
// embedder to provide, pipe-based fork barriers, and check macros.
//
// Each test binary is a single translation unit, so defining the stubs in
// this header is safe.

#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/base/sound_driver.h"
#include "core/base/system.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaLink.h"

// ---- System-callback stubs (must be provided by the embedder) --------------

struct CoreOptions coreOptions;

// Record the last message so tests can assert on the error the core reports.
static char g_last_system_message[512];
void systemMessage(int, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_system_message, sizeof(g_last_system_message), fmt, args);
    va_end(args);
}
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

// ---- Test scaffolding -------------------------------------------------------

static const char* g_role = "parent";

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL [%s pid %d] %s:%d: %s — ", g_role,         \
                (int)getpid(), __FILE__, __LINE__, #cond);                   \
            fprintf(stderr, __VA_ARGS__);                                    \
            fprintf(stderr, "\n");                                           \
            _exit(1);                                                        \
        }                                                                    \
    } while (0)

// Bidirectional one-byte pipe barrier between a parent and one child.
// Construct before fork(); both sides then call the same methods.
class Barrier {
  public:
    Barrier() {
        if (pipe(p2c_) != 0 || pipe(c2p_) != 0) {
            perror("pipe");
            _exit(1);
        }
    }

    // Call once in each process right after fork().
    void set_child(bool is_child) { child_ = is_child; }

    // Exchange one byte; returns the peer's byte. Blocks until both sides
    // arrive, so it doubles as a rendezvous.
    uint8_t swap(uint8_t mine) {
        int wfd = child_ ? c2p_[1] : p2c_[1];
        int rfd = child_ ? p2c_[0] : c2p_[0];
        uint8_t theirs = 0xee;
        if (write(wfd, &mine, 1) != 1 || read(rfd, &theirs, 1) != 1) {
            fprintf(stderr, "FAIL [%s pid %d]: barrier peer vanished\n",
                g_role, (int)getpid());
            _exit(1);
        }
        return theirs;
    }

    void sync() { (void)swap(0); }

  private:
    int p2c_[2];
    int c2p_[2];
    bool child_ = false;
};

// Reap a child and propagate its failure.
static void expect_child_success(pid_t pid) {
    int status = 0;
    CHECK(waitpid(pid, &status, 0) == pid, "waitpid failed");
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "child exited with status 0x%x", status);
}

// Watchdog: any test that wedges is killed and reported by ctest.
static void arm_watchdog(unsigned seconds) {
    alarm(seconds);
}

// Allocate the I/O register block the link code reads and writes through
// g_ioMem. 0x400 covers every COMM_* register.
static void setup_fake_io() {
    g_ioMem = (uint8_t*)calloc(0x400, 1);
    CHECK(g_ioMem != nullptr, "calloc failed");
}

static uint16_t io_read16(uint32_t addr) {
    return (uint16_t)(g_ioMem[addr] | (g_ioMem[addr + 1] << 8));
}

static void io_write16(uint32_t addr, uint16_t value) {
    g_ioMem[addr] = (uint8_t)(value & 0xff);
    g_ioMem[addr + 1] = (uint8_t)(value >> 8);
}

static double now_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
