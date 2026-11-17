// Bridge integration test driver: the external program side.
//
// Loads lua_bridge_test.lua into the engine, then acts as the outside
// client the bridge exists for: connects over TCP, sends commands,
// and pumps the frame loop (ResumeScriptCoroutine + a synthetic
// CaptureFrame, standing in for systemDrawScreen). Verifies the
// round-trip, the joypad override, and a PNG screenshot, and exits
// non-zero on any failure.
//
// Environment:
//   VBAM_TEST_PORT  port the script listens on (default 18923)
//   VBAM_TEST_PNG   path the screenshot command should write
//                   (default ./bridge_shot.png)
//
// Usage: lua_bridge_runner [script.lua]
//
// The socket half is portable across POSIX and Windows (winsock):
// select() is used for the readable-wait since it exists everywhere.

#include "wx/lua/lua_engine.h"
#include "core/base/system.h"

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
   typedef SOCKET SocketFd;
   static const SocketFd kBadSocket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <unistd.h>
   typedef int SocketFd;
   static const SocketFd kBadSocket = -1;
#endif

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

// Frontend stubs that vbam-core expects the embedder to provide (same
// as lua_test_runner).
struct CoreOptions coreOptions;
bool pause_next = false;

namespace {

int g_failures = 0;

void PumpFrame(vbam::wx::LuaEngine& eng);  // below

void Check(bool cond, const std::string& what) {
    std::printf("%s %s\n", cond ? "[ok]  " : "[FAIL]", what.c_str());
    if (!cond) ++g_failures;
}

void CloseSock(SocketFd fd) {
#if defined(_WIN32)
    closesocket(fd);
#else
    close(fd);
#endif
}

void SleepMs(int ms) {
#if defined(_WIN32)
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

// Wait up to `ms` for the socket to become readable.
bool WaitReadable(SocketFd fd, int ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return select((int)fd + 1, &rfds, nullptr, nullptr, &tv) > 0;
}

// Returns kBadSocket on failure: a SOCKET on Win64 is a 64-bit handle,
// so this cannot be an int-returning function.
SocketFd Connect(int port) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        SocketFd fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == kBadSocket) return kBadSocket;
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((unsigned short)port);
        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0) return fd;
        CloseSock(fd);
        SleepMs(50);  // listen() comes up on the first resume
    }
    return kBadSocket;
}

bool SendLine(SocketFd fd, const std::string& s) {
    std::string line = s + "\n";
    const char* p = line.data();
    size_t left = line.size();
    while (left > 0) {
        int n = (int)send(fd, p, (int)left, 0);
        if (n <= 0) return false;
        p += n;
        left -= (size_t)n;
    }
    return true;
}

// Pump frames until a reply line arrives (or ~5 s pass).
bool RecvLine(vbam::wx::LuaEngine& eng, SocketFd fd, std::string* out) {
    for (int spin = 0; spin < 100; ++spin) {
        PumpFrame(eng);
        if (WaitReadable(fd, 50)) {
            char buf[4096];
            int n = (int)recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) return false;
            std::string data(buf, (size_t)n);
            size_t nl = data.find('\n');
            if (nl == std::string::npos) continue;
            *out = data.substr(0, nl);
            if (!out->empty() && out->back() == '\r') out->pop_back();
            return true;
        }
    }
    return false;
}

// One emulated frame: hand the engine a synthetic composed framebuffer
// in the core's g_pix layout (241-px pitch, guard row 0, visible rows
// 1..160), then resume the script's frameadvance loop. Red at visible
// (0,0), blue at visible (239,159).
void PumpFrame(vbam::wx::LuaEngine& eng) {
    constexpr int kW = 240, kH = 160, kPitchPx = 241;
    static std::vector<uint32_t> pix((size_t)kPitchPx * (kH + 1), 0);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            uint32_t r = (uint32_t)(x * 255 / (kW - 1));
            uint32_t g = (uint32_t)(y * 255 / (kH - 1));
            pix[(size_t)(y + 1) * kPitchPx + x] = (r << 16) | (g << 8);
        }
    }
    pix[(size_t)1 * kPitchPx + 0]            = 0x00ff0000u;  // red
    pix[(size_t)kH * kPitchPx + (kW - 1)]    = 0x000000ffu;  // blue
    eng.CaptureFrame(reinterpret_cast<const uint8_t*>(pix.data()),
                     kW, kH, kPitchPx * 4, 32, 16, 8, 0);
    eng.ResumeScriptCoroutine();
}

// FCEUX bit positions (A=0, B=1, start=3).
constexpr uint32_t kA = 1u << 0, kB = 1u << 1, kStart = 1u << 3;

#if defined(_WIN32)
struct WinsockInit {
    WinsockInit() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    ~WinsockInit() { WSACleanup(); }
};
#endif

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    static WinsockInit winsock;
#endif
    const char* script = (argc >= 2) ? argv[1] : "lua_bridge_test.lua";
    const int port = atoi(getenv("VBAM_TEST_PORT") ? getenv("VBAM_TEST_PORT") : "18923");
    const std::string png = getenv("VBAM_TEST_PNG") ? getenv("VBAM_TEST_PNG") : "bridge_shot.png";

    auto& eng = vbam::wx::LuaInstance();
    if (!eng.LoadFile(script)) {
        std::fprintf(stderr, "LoadFile(\"%s\") failed\n", script);
        return 2;
    }

    // First resume runs the script body up to its first
    // emu.frameadvance(), which is past bridge.listen().
    PumpFrame(eng);

    SocketFd fd = Connect(port);
    Check(fd != kBadSocket, "client connects to bridge port " + std::to_string(port));
    if (fd == kBadSocket) return 1;

    std::string reply;

    bool ok = SendLine(fd, "ping") && RecvLine(eng, fd, &reply);
    Check(ok && reply == "pong", "ping/pong round-trip (got '" + reply + "')");

    ok = SendLine(fd, "press A+B+start") && RecvLine(eng, fd, &reply);
    Check(ok && reply == "ok", "press A+B+start accepted");
    uint32_t mask = 0;
    const bool has_override = eng.GetJoypadOverride(0, &mask);
    char mask_msg[64];
    std::snprintf(mask_msg, sizeof(mask_msg),
                  "joypad override = A+B+start (0x%x)", mask);
    Check(has_override && mask == (kA | kB | kStart), mask_msg);

    ok = SendLine(fd, "release") && RecvLine(eng, fd, &reply);
    Check(ok && reply == "ok", "release accepted");
    Check(!eng.GetJoypadOverride(0, &mask), "override cleared after release");

    ok = SendLine(fd, "screenshot " + png) && RecvLine(eng, fd, &reply);
    Check(ok && reply == "ok " + png, "screenshot command (got '" + reply + "')");

    // PNG sanity: magic + IHDR dimensions 240x160.
    {
        FILE* f = fopen(png.c_str(), "rb");
        Check(f != nullptr, "screenshot file exists");
        if (f) {
            unsigned char hdr[26];
            size_t got = fread(hdr, 1, sizeof(hdr), f);
            fclose(f);
            Check(got == sizeof(hdr) && !std::memcmp(hdr + 1, "PNG", 3),
                  "PNG magic");
            uint32_t w = ((uint32_t)hdr[16] << 24) | ((uint32_t)hdr[17] << 16) |
                         ((uint32_t)hdr[18] << 8) | hdr[19];
            uint32_t h = ((uint32_t)hdr[20] << 24) | ((uint32_t)hdr[21] << 16) |
                         ((uint32_t)hdr[22] << 8) | hdr[23];
            Check(w == 240 && h == 160, "PNG is 240x160 (got " +
                                            std::to_string(w) + "x" +
                                            std::to_string(h) + ")");
        }
    }

    // loadrom plumbing: in this headless build the wx backend is not
    // installed, so emu.loadrom must refuse cleanly (nil + error)
    // rather than crash or silently succeed.
    ok = SendLine(fd, "loadrom /nonexistent/nope.gba") && RecvLine(eng, fd, &reply);
    Check(ok && reply.rfind("err ", 0) == 0,
          "loadrom reports a clean error headless (got '" + reply + "')");

    Check(SendLine(fd, "quit") && RecvLine(eng, fd, &reply) && reply == "bye",
          "quit handshake");

    CloseSock(fd);
    eng.Stop();

    std::printf("lua_bridge_runner: %s\n", g_failures ? "FAIL" : "PASS");
    return g_failures ? 1 : 0;
}
