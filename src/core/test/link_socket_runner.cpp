// Two-process harness for the LAN / TCP socket link.
//
// Usage: link_socket_runner --test <name>
//   handshake            Server then client on 127.0.0.1; both must reach
//                        LINK_OK and the client must be player 2.
//   handshake_retry      Client starts ~400 ms BEFORE the server listens;
//                        the connect-retry path must still converge.
//   cable                One 16-bit multiplayer exchange: master data
//                        0xCAFE and slave data 0xBEEF must cross.
//   gb                   8 GB serial byte swaps through gbStartLink (server
//                        master) and gbLinkUpdate (client slave).
//   disconnect           Client dies with no goodbye mid-session; server
//                        must reach LINK_DISCONNECTED within the timeout.
//   disconnect_goodbye   Client closes cleanly (goodbye frame); same
//                        expectation via the framed-goodbye path.
//
// The port is derived from the parent pid so parallel runs don't collide.

#include "link_test_support.h"

static void pump_connect(const char* what, double deadline_s) {
    char msg[256] = { 0 };
    for (;;) {
        ConnectionState st = ConnectLinkUpdate(msg, sizeof(msg));
        if (st == LINK_OK)
            return;
        CHECK(st == LINK_NEEDS_UPDATE, "%s: connect failed: %d (%s)", what,
            (int)st, msg);
        CHECK(now_seconds() < deadline_s, "%s: connect timed out (%s)", what,
            msg);
        // The wx dialog polls every 50 ms; the client's pre-established
        // path returns immediately, so pace it the same way here.
        usleep(20 * 1000);
    }
}

// Bring up a 2-player session. Parent = server/master, child = client/slave.
// server_delay_ms delays the server's listen to exercise the client's
// connect-retry path.
static void establish(Barrier& b, bool child, LinkMode mode,
    int server_delay_ms) {
    SetLinkTimeout(500);
    if (!child) {
        if (server_delay_ms > 0)
            usleep(server_delay_ms * 1000);
        EnableLinkServer(true, 1);
        ConnectionState st = InitLink(mode);
        CHECK(st == LINK_NEEDS_UPDATE, "server InitLink returned %d", (int)st);
        pump_connect("server", now_seconds() + 20.0);
    } else {
        EnableLinkServer(false, 0);
        CHECK(SetLinkServerHost("127.0.0.1"), "SetLinkServerHost failed");
        ConnectionState st = InitLink(mode);
        CHECK(st == LINK_NEEDS_UPDATE, "client InitLink returned %d", (int)st);
        pump_connect("client", now_seconds() + 20.0);
        CHECK(GetLinkPlayerId() == 1, "client is player %d, want 1",
            GetLinkPlayerId());
    }
    CHECK(GetLinkMode() == mode, "mode dropped after connect");
    b.sync(); // both sides connected
}

// ---- handshake --------------------------------------------------------------

static int run_handshake(int server_delay_ms) {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "client";

    establish(b, child, LINK_CABLE_SOCKET, server_delay_ms);

    b.sync();
    CloseLink();
    if (child)
        _exit(0);
    expect_child_success(pid);
    return 0;
}

// ---- cable ------------------------------------------------------------------

static int run_cable() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "client";

    setup_fake_io();
    establish(b, child, LINK_CABLE_SOCKET, 0);

    double deadline = now_seconds() + 15.0;
    if (!child) {
        io_write16(COMM_SIODATA8, 0xCAFE);
        // Keep starting transfers until the slave's word lands in
        // SIOMULTI1; each missed frame costs one bounded 50 ms retry.
        while (io_read16(COMM_SIOMULTI1) != 0xBEEF) {
            CHECK(GetLinkMode() == LINK_CABLE_SOCKET,
                "master: link dropped during exchange");
            CHECK(now_seconds() < deadline,
                "master timed out: SIOMULTI1=0x%04x",
                io_read16(COMM_SIOMULTI1));
            StartLink(0x2080); // MULTIPLAYER, start bit (no-op mid-transfer)
            LinkUpdate(80000); // crosses the trtimeend pacing threshold
        }
    } else {
        io_write16(COMM_SIODATA8, 0xBEEF);
        StartLink(0x2000); // MULTIPLAYER mode, slave
        // The master's word lands in SIOMULTI0 when the slave answers the
        // incoming frame (CheckConn -> Send).
        while (io_read16(COMM_SIOMULTI0) != 0xCAFE) {
            CHECK(GetLinkMode() == LINK_CABLE_SOCKET,
                "slave: link dropped during exchange");
            CHECK(now_seconds() < deadline,
                "slave timed out: SIOMULTI0=0x%04x",
                io_read16(COMM_SIOMULTI0));
            CheckLinkConnection();
            LinkUpdate(80000);
        }
    }

    b.sync(); // both sides verified before teardown
    CloseLink();
    if (child)
        _exit(0);
    expect_child_success(pid);
    return 0;
}

// ---- gb ---------------------------------------------------------------------

static int run_gb() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "client";

    establish(b, child, LINK_GAMEBOY_SOCKET, 0);
    SetLinkTimeout(2000);

    if (!child) {
        for (int i = 0; i < 8; i++) {
            // No retry: a re-send would desync the slave's pre-loaded
            // reply. The 2 s exchange timeout is the whole budget.
            uint8_t dat = gbStartLink((uint8_t)(0x40 + i));
            CHECK(dat == (uint8_t)(0x90 + i),
                "byte %d: master got 0x%02x, want 0x%02x", i, dat, 0x90 + i);
        }
        b.sync();
        CloseLink();
        expect_child_success(pid);
        return 0;
    }

    int received = 0;
    double deadline = now_seconds() + 20.0;
    while (received < 8) {
        CHECK(now_seconds() < deadline, "slave stuck after %d/8 bytes",
            received);
        uint16_t r = gbLinkUpdate((uint8_t)(0x90 + received), 1);
        if (r & 1) {
            uint8_t dat = (uint8_t)(r >> 8);
            if (dat == (uint8_t)(0x40 + received))
                received++;
        }
        usleep(1000);
    }
    b.sync();
    CloseLink();
    _exit(0);
}

// ---- disconnect -------------------------------------------------------------

static int run_disconnect(bool clean_goodbye) {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "client";

    setup_fake_io();
    establish(b, child, LINK_CABLE_SOCKET, 0);

    if (child) {
        if (clean_goodbye)
            CloseLink(); // sends the framed goodbye
        _exit(0); // hard drop otherwise: no goodbye, no FIN handshake help
    }

    expect_child_success(pid);

    // Drive the master's transfer loop; the drop must surface as a deferred
    // close and end in LINK_DISCONNECTED well within the retry budget.
    io_write16(COMM_SIODATA8, 0x1111);
    double deadline = now_seconds() + 10.0;
    while (GetLinkMode() != LINK_DISCONNECTED) {
        CHECK(now_seconds() < deadline,
            "server never noticed the %s disconnect",
            clean_goodbye ? "clean" : "hard");
        StartLink(0x2080);
        LinkUpdate(80000);
        CheckLinkConnection();
    }
    return 0;
}

// ---- main -------------------------------------------------------------------

int main(int argc, char** argv) {
    const char* test = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0 && i + 1 < argc)
            test = argv[++i];
    }
    if (!test) {
        fprintf(stderr,
            "usage: %s --test <handshake|handshake_retry|cable|gb|disconnect|"
            "disconnect_goodbye>\n",
            argv[0]);
        return 2;
    }

    // A pid-derived port keeps parallel ctest jobs and real emulator
    // sessions (default port 5738) out of each other's way. Set before
    // fork() so both processes agree on it.
    IP_LINK_PORT = (uint16_t)(40000 + (getpid() % 20000));
    IP_LINK_BIND_ADDRESS = "127.0.0.1";

    // A peer that dies mid-send must surface as a socket error, not kill
    // the process.
    signal(SIGPIPE, SIG_IGN);

    arm_watchdog(90);

    if (strcmp(test, "handshake") == 0)
        return run_handshake(0);
    if (strcmp(test, "handshake_retry") == 0)
        return run_handshake(400);
    if (strcmp(test, "cable") == 0)
        return run_cable();
    if (strcmp(test, "gb") == 0)
        return run_gb();
    if (strcmp(test, "disconnect") == 0)
        return run_disconnect(false);
    if (strcmp(test, "disconnect_goodbye") == 0)
        return run_disconnect(true);

    fprintf(stderr, "unknown test '%s'\n", test);
    return 2;
}
