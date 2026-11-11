// `bridge` namespace — a tiny TCP bridge so external programs can
// drive the emulator through a Lua script.
//
//   bridge.listen([port=8765]) → true/false
//       Start a TCP server on 127.0.0.1. One client at a time; after
//       a client disconnects the server goes back to accepting.
//   bridge.close()              stop the server, drop the client
//   bridge.connected() → bool   is a client attached right now
//   bridge.receive() → s|nil    pop one '\n'-terminated line the
//                               client sent (nil when the queue is
//                               empty); non-blocking
//   bridge.send(s) → bool       write s + '\n' to the client
//
// The intended shape is an automation loop inside the script:
//
//   bridge.listen(8765)
//   while true do
//       local msg = bridge.receive()
//       if msg then handle(msg) end
//       emu.frameadvance()
//   end
//
// `receive()` is polled once per frame by the script, so commands take
// effect at frame granularity — no locks are ever held across Lua
// calls. The server thread only ever touches Lua-side state through
// the mutex-guarded line queue.
//
// The server binds loopback only; this is a local automation hook,
// not a network service.

#include "wx/lua/lua_engine.h"
#include "wx/lua/lua_internal.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET BridgeSocket;
static const BridgeSocket kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int BridgeSocket;
static const BridgeSocket kInvalidSocket = -1;
#endif

namespace vbam {
namespace wx {
namespace {

// ---------------------------------------------------------------------------
// BridgeServer — one static instance owned by the lua_api_bridge
// translation unit. The engine tears it down via
// LuaApi_ShutdownBridge() from LuaEngine::Stop() so a reloaded script
// cannot leak a listening socket or a thread.
// ---------------------------------------------------------------------------
class BridgeServer {
public:
    ~BridgeServer() { Stop(); }

    bool Listen(int port) {
        Stop();

#if defined(_WIN32)
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        winsock_up_ = true;
#endif

        BridgeSocket ls = socket(AF_INET, SOCK_STREAM, 0);
        if (ls == kInvalidSocket) {
#if defined(_WIN32)
            if (winsock_up_) { WSACleanup(); winsock_up_ = false; }
#endif
            return false;
        }

        int one = 1;
        setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((unsigned short)port);
        if (bind(ls, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(ls, 1) != 0) {
            CloseSocket(ls);
#if defined(_WIN32)
            if (winsock_up_) { WSACleanup(); winsock_up_ = false; }
#endif
            return false;
        }

        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            listen_sock_ = ls;
            stopping_ = false;
        }
        thread_ = std::thread([this] { Run(); });
        return true;
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            if (listen_sock_ == kInvalidSocket && !thread_.joinable()) return;
            stopping_ = true;
            if (listen_sock_ != kInvalidSocket) {
                CloseSocketLocked(listen_sock_);
                listen_sock_ = kInvalidSocket;
            }
            if (client_sock_ != kInvalidSocket) {
                CloseSocketLocked(client_sock_);
                client_sock_ = kInvalidSocket;
            }
        }
        // Must not hold state_mutex_ here: the server thread takes it
        // on its way out (after the closed sockets error out its
        // accept/recv), so locking across join would deadlock.
        if (thread_.joinable()) thread_.join();

#if defined(_WIN32)
        if (winsock_up_) { WSACleanup(); winsock_up_ = false; }
#endif
    }

    bool Connected() {
        std::lock_guard<std::mutex> lk(state_mutex_);
        return client_sock_ != kInvalidSocket;
    }

    // Pop one received line, or return false when the queue is empty.
    bool Receive(std::string* out) {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        if (lines_.empty()) return false;
        *out = std::move(lines_.front());
        lines_.pop_front();
        return true;
    }

    bool Send(const std::string& s) {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (client_sock_ == kInvalidSocket) return false;
        std::string line = s;
        line.push_back('\n');
        const char* p = line.data();
        size_t left = line.size();
        while (left > 0) {
            int n = (int)send(client_sock_, p, (int)left, 0);
            if (n <= 0) return false;
            p += n;
            left -= (size_t)n;
        }
        return true;
    }

private:
    // Wake up any blocking accept()/recv()/send() on the far side
    // first: on Linux a plain close() of an fd does NOT interrupt a
    // syscall already blocked on it, so the server thread would sleep
    // in accept() forever and Stop()'s join would deadlock.
    // shutdown() makes the pending call return immediately.
    static void CloseSocket(BridgeSocket s) {
#if defined(_WIN32)
        shutdown(s, SD_BOTH);
        closesocket(s);
#else
        shutdown(s, SHUT_RDWR);
        close(s);
#endif
    }
    void CloseSocketLocked(BridgeSocket s) { CloseSocket(s); }

    // Accept loop + per-client read loop. Runs on its own thread; the
    // only cross-thread contact is the line queue and the two socket
    // fds, both under their mutex.
    void Run() {
        for (;;) {
            // Accept (blocking; a closed listen socket errors out).
            BridgeSocket ls;
            {
                std::lock_guard<std::mutex> lk(state_mutex_);
                if (stopping_ || listen_sock_ == kInvalidSocket) return;
                ls = listen_sock_;
            }
            sockaddr_in peer;
#if defined(_WIN32)
            int peer_len = sizeof(peer);
#else
            socklen_t peer_len = sizeof(peer);
#endif
            BridgeSocket cs = accept(ls, (sockaddr*)&peer, &peer_len);
            {
                std::lock_guard<std::mutex> lk(state_mutex_);
                if (cs == kInvalidSocket || stopping_) return;
                client_sock_ = cs;
            }

            // Read lines until the client goes away, then drop it and
            // go back to accepting (so a controller can reconnect).
            char buf[4096];
            std::string pending;
            for (;;) {
                int n = (int)recv(cs, buf, sizeof(buf), 0);
                if (n <= 0) break;
                pending.append(buf, (size_t)n);
                size_t nl;
                while ((nl = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, nl);
                    pending.erase(0, nl + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) continue;
                    std::lock_guard<std::mutex> qk(queue_mutex_);
                    lines_.push_back(std::move(line));
                }
            }
            {
                std::lock_guard<std::mutex> lk(state_mutex_);
                if (stopping_) return;
                if (client_sock_ != kInvalidSocket) {
                    CloseSocketLocked(client_sock_);
                    client_sock_ = kInvalidSocket;
                }
            }
        }
    }

    std::thread thread_;
    std::mutex state_mutex_;   // guards listen_sock_ / client_sock_ / stopping_
    BridgeSocket listen_sock_ = kInvalidSocket;
    BridgeSocket client_sock_ = kInvalidSocket;
    bool stopping_ = false;
#if defined(_WIN32)
    bool winsock_up_ = false;
#endif

    std::mutex queue_mutex_;   // guards lines_
    std::deque<std::string> lines_;
};

BridgeServer& Server() {
    static BridgeServer s_server;
    return s_server;
}

constexpr int kDefaultPort = 8765;

int l_listen(lua_State* L) {
    int port = kDefaultPort;
    if (lua_gettop(L) >= 1 && !lua_isnoneornil(L, 1))
        port = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, Server().Listen(port) ? 1 : 0);
    return 1;
}

int l_close(lua_State*) {
    Server().Stop();
    return 0;
}

int l_connected(lua_State* L) {
    lua_pushboolean(L, Server().Connected() ? 1 : 0);
    return 1;
}

int l_receive(lua_State* L) {
    std::string line;
    if (Server().Receive(&line)) {
        lua_pushlstring(L, line.data(), line.size());
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

int l_send(lua_State* L) {
    size_t len;
    const char* s = luaL_checklstring(L, 1, &len);
    lua_pushboolean(L, Server().Send(std::string(s, len)) ? 1 : 0);
    return 1;
}

const luaL_Reg kBridgeFns[] = {
    {"listen",    l_listen},
    {"close",     l_close},
    {"connected", l_connected},
    {"receive",   l_receive},
    {"send",      l_send},
    {nullptr, nullptr},
};

}  // namespace

void LuaApi_InstallBridge(lua_State* L, LuaEngine* /*engine*/) {
    luaL_newlib(L, kBridgeFns);
    lua_setglobal(L, "bridge");
}

void LuaApi_ShutdownBridge() {
    Server().Stop();
}

}  // namespace wx
}  // namespace vbam
