// Shared scaffolding for the two-process link tests (link_ipc_test and
// link_socket_runner): the system-callback stubs vbam-core expects its
// embedder to provide, pipe-based barriers between the two processes, and
// check macros.
//
// The second process comes from fork() on POSIX. Windows has no fork(), so
// there StartChild() runs this executable again, which main() hands to
// RunChildIfAsked(). link_ipc_test is built this way on both; the other
// runners still fork() directly and are POSIX-only.
//
// Each test binary is a single translation unit, so defining the stubs in
// this header is safe.

#pragma once

#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#endif

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

static inline int test_pid() {
#if defined(_WIN32)
    return _getpid();
#else
    return (int)getpid();
#endif
}

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL [%s pid %d] %s:%d: %s — ", g_role,         \
                test_pid(), __FILE__, __LINE__, #cond);                      \
            fprintf(stderr, __VA_ARGS__);                                    \
            fprintf(stderr, "\n");                                           \
            _exit(1);                                                        \
        }                                                                    \
    } while (0)

#if defined(_WIN32)
// Bidirectional one-byte pipe barrier between a parent and one child, on
// anonymous pipes. The parent constructs it before StartChild(), which
// hands the child its two ends on the command line; RunChildIfAsked()
// builds the child's side from them.
class Barrier {
  public:
    Barrier() {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        if (!CreatePipe(&p2c_[0], &p2c_[1], &sa, 0)
            || !CreatePipe(&c2p_[0], &c2p_[1], &sa, 0)) {
            fprintf(stderr, "FAIL: CreatePipe: %lu\n", GetLastError());
            _exit(1);
        }
        // Only the child's ends go to the child.
        SetHandleInformation(p2c_[1], HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(c2p_[0], HANDLE_FLAG_INHERIT, 0);
    }

    // The child's side, from the handles StartChild() passed it.
    Barrier(HANDLE read_end, HANDLE write_end) : child_(true) {
        p2c_[0] = read_end;
        c2p_[1] = write_end;
    }

    // The child's ends, as StartChild() puts them on its command line.
    std::string child_ends() const {
        char buf[64];
        snprintf(buf, sizeof(buf), "%llu %llu",
            (unsigned long long)(uintptr_t)p2c_[0],
            (unsigned long long)(uintptr_t)c2p_[1]);
        return buf;
    }

    // Once the child has its own copies: drop ours, so the child sees a
    // broken pipe if this process dies, and a later child does not inherit
    // them. `process` is the child, so a wait can tell it has died.
    void child_started(HANDLE process) {
        CloseHandle(p2c_[0]);
        CloseHandle(c2p_[1]);
        p2c_[0] = c2p_[1] = NULL;
        peer_ = process;
    }

    // Exchange one byte; returns the peer's byte. Blocks until both sides
    // arrive, so it doubles as a rendezvous. Pumps the link while it waits,
    // as the POSIX one does (see there).
    uint8_t swap(uint8_t mine) {
        HANDLE wr = child_ ? c2p_[1] : p2c_[1];
        HANDLE rd = child_ ? p2c_[0] : c2p_[0];
        DWORD n = 0;
        if (!WriteFile(wr, &mine, 1, &n, NULL) || n != 1)
            vanished();
        for (;;) {
            if (ready(rd))
                break;
            // A child that wrote its last byte and exited still counts:
            // look once more after seeing it gone.
            if (peer_ != NULL && WaitForSingleObject(peer_, 0) == WAIT_OBJECT_0) {
                if (ready(rd))
                    break;
                vanished();
            }
            if (GetLinkMode() == LINK_CABLE_IPC)
                LinkUpdate(4096);
            Sleep(1);
        }
        uint8_t theirs = 0xee;
        if (!ReadFile(rd, &theirs, 1, &n, NULL) || n != 1)
            vanished();
        return theirs;
    }

    void sync() { (void)swap(0); }

  private:
    bool ready(HANDLE rd) {
        DWORD avail = 0;
        if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL))
            vanished();
        return avail > 0;
    }

    [[noreturn]] void vanished() {
        fprintf(stderr, "FAIL [%s pid %d]: barrier peer vanished\n", g_role,
            test_pid());
        _exit(1);
    }

    HANDLE p2c_[2] = { NULL, NULL };
    HANDLE c2p_[2] = { NULL, NULL };
    HANDLE peer_ = NULL;
    bool child_ = false;
};
#else
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
        if (write(wfd, &mine, 1) != 1) {
            fprintf(stderr, "FAIL [%s pid %d]: barrier peer vanished\n",
                g_role, (int)getpid());
            _exit(1);
        }
        // Poll instead of blocking, pumping the link while we wait: a real
        // emulator's clock keeps advancing while it waits for anything, and
        // the hard lead-block releases when a live peer's clock catches up.
        // A frozen-clock blocking read here deadlocks against a peer
        // legitimately blocked at the lead cap (it waits for our clock; we
        // wait for its barrier write).
        for (;;) {
            struct pollfd pf;
            pf.fd = rfd;
            pf.events = POLLIN;
            pf.revents = 0;
            int r = poll(&pf, 1, 1);
            if (r > 0) {
                if (read(rfd, &theirs, 1) != 1) {
                    fprintf(stderr,
                        "FAIL [%s pid %d]: barrier peer vanished\n",
                        g_role, (int)getpid());
                    _exit(1);
                }
                break;
            }
            if (r < 0 && errno != EINTR) {
                fprintf(stderr, "FAIL [%s pid %d]: barrier poll error\n",
                    g_role, (int)getpid());
                _exit(1);
            }
            if (GetLinkMode() == LINK_CABLE_IPC)
                LinkUpdate(4096);
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
#endif

// ---- The second process -----------------------------------------------------

// What the child of a two-process test runs. Returning ends the child with
// success; failures _exit() through CHECK.
typedef void (*ChildMain)(Barrier& b);

struct ChildEntry {
    const char* name;
    ChildMain main;
};

#if defined(_WIN32)
struct Child {
    HANDLE process;
};

// Runs a child in a second process connected to this one by `b`: this
// executable again, told by `--child name` which ChildEntry to run. It
// starts from main(), not from here, so it has none of this process's state;
// the link namespace and the barrier's ends go with it on the command line.
static inline Child StartChild(Barrier& b, const char* name, ChildMain) {
    char exe[MAX_PATH];
    const DWORD len = GetModuleFileNameA(NULL, exe, sizeof(exe));
    CHECK(len > 0 && len < sizeof(exe), "GetModuleFileName failed");
    const char* ns = getenv("VBAM_LINK_NAMESPACE");
    std::string cmd = std::string("\"") + exe + "\" --child " + name +
        " --pipes " + b.child_ends() + " --namespace " + (ns && *ns ? ns : "-");
    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    CHECK(CreateProcessA(NULL, &cmd[0], NULL, NULL, TRUE, 0, NULL, NULL, &si,
              &pi),
        "CreateProcess failed: %lu", GetLastError());
    CloseHandle(pi.hThread);
    b.child_started(pi.hProcess);
    return Child{ pi.hProcess };
}

// Wait for a child and propagate its failure.
static inline void expect_child_success(const Child& c) {
    CHECK(WaitForSingleObject(c.process, INFINITE) == WAIT_OBJECT_0,
        "waiting for the child failed");
    DWORD code = 1;
    CHECK(GetExitCodeProcess(c.process, &code), "GetExitCodeProcess failed");
    CloseHandle(c.process);
    CHECK(code == 0, "child exited with status 0x%lx", code);
}

static inline void set_env(const char* name, const char* value) {
    // _putenv() rather than _putenv_s(), which Windows XP's msvcrt lacks.
    // It keeps no pointer to its argument.
    _putenv((std::string(name) + "=" + value).c_str());
}

// In a process StartChild() started, runs the child it names and exits;
// otherwise returns. main() calls it once the process is set up the way
// every test expects (I/O registers, watchdog), before anything else.
static inline void RunChildIfAsked(int argc, char** argv, const ChildEntry* children,
                                   size_t count) {
    const char* name = nullptr;
    const char* ns = nullptr;
    unsigned long long rd = 0, wr = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--child") == 0 && i + 1 < argc)
            name = argv[++i];
        else if (strcmp(argv[i], "--namespace") == 0 && i + 1 < argc)
            ns = argv[++i];
        else if (strcmp(argv[i], "--pipes") == 0 && i + 2 < argc) {
            rd = strtoull(argv[i + 1], nullptr, 10);
            wr = strtoull(argv[i + 2], nullptr, 10);
            i += 2;
        }
    }
    if (!name)
        return;
    g_role = name;
    CHECK(ns && rd && wr, "--child needs --pipes and --namespace");
    if (strcmp(ns, "-") != 0)
        set_env("VBAM_LINK_NAMESPACE", ns);
    for (size_t i = 0; i < count; i++)
        if (strcmp(children[i].name, name) == 0) {
            Barrier b((HANDLE)(uintptr_t)rd, (HANDLE)(uintptr_t)wr);
            children[i].main(b);
            _exit(0);
        }
    CHECK(false, "no child named %s", name);
}

static DWORD WINAPI watchdog_thread(LPVOID arg) {
    Sleep((DWORD)(uintptr_t)arg * 1000);
    fprintf(stderr, "FAIL [%s pid %d]: watchdog expired\n", g_role, test_pid());
    _exit(3);
}

// Watchdog: any test that wedges is killed and reported by ctest.
static void arm_watchdog(unsigned seconds) {
    HANDLE t = CreateThread(NULL, 0, watchdog_thread,
        (LPVOID)(uintptr_t)seconds, 0, NULL);
    if (t != NULL)
        CloseHandle(t);
}

static inline void sleep_us(unsigned us) {
    if (us >= 1000) {
        Sleep(us / 1000);
        return;
    }
    // Shorter than Sleep() goes: spin.
    const auto end =
        std::chrono::steady_clock::now() + std::chrono::microseconds(us);
    while (std::chrono::steady_clock::now() < end) {
    }
}
#else
struct Child {
    pid_t pid;
};

// Runs `fn` in a forked child connected to this process by `b`. The child
// starts with this process's state, as fork() leaves it.
static inline Child StartChild(Barrier& b, const char* name, ChildMain fn) {
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    b.set_child(pid == 0);
    if (pid == 0) {
        g_role = name;
        fn(b);
        _exit(0);
    }
    return Child{ pid };
}

static inline void expect_child_success(const Child& c) {
    expect_child_success(c.pid);
}

// Forked children never get here.
static inline void RunChildIfAsked(int, char**, const ChildEntry*, size_t) {}

// Watchdog: any test that wedges is killed and reported by ctest.
static void arm_watchdog(unsigned seconds) {
    alarm(seconds);
}

static inline void set_env(const char* name, const char* value) {
    setenv(name, value, 1);
}

static inline void sleep_us(unsigned us) {
    usleep(us);
}
#endif

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
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
