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
//   bind_conflict        Server bind failures: a busy port must report
//                        "already in use", and a non-local bind address
//                        must warn and fall back to all interfaces.
//   gp                   General-Purpose (RCNT GPIO) mode over the socket:
//                        driven pin levels cross in both directions,
//                        undriven lines read pull-up 1s, and an SI falling
//                        edge raises the slave's serial IRQ.
//
// The port is derived from the parent pid so parallel runs don't collide.

#include "link_test_support.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

// ---- slave_commit -----------------------------------------------------------

// The slave must complete a transfer (busy bit cleared, SIOMULTI written,
// serial IRQ flagged) at the hardware transfer time, WITHOUT the master
// starting another transfer. The old code parked the slave's commit inside
// Recv() until the master's NEXT start frame arrived, so the last transfer
// before any pause in the master's stream hung uncommitted and every
// master-side lull became 50 ms emulation stalls accumulating toward a
// bogus "Link timeout." (Pokémon's burst-per-frame protocol trips both.)
static int run_slave_commit() {
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
        b.sync(); // slave is in multi mode and pumping
        StartLink(0x2080); // exactly ONE transfer
        while (io_read16(COMM_SIOMULTI1) != 0xBEEF) {
            CHECK(GetLinkMode() == LINK_CABLE_SOCKET,
                "master: link dropped during exchange");
            CHECK(now_seconds() < deadline,
                "master timed out: SIOMULTI1=0x%04x",
                io_read16(COMM_SIOMULTI1));
            LinkUpdate(80000);
        }
        // No further StartLink: the slave must commit on its own clock.
        b.sync(); // slave verified its commit
        CloseLink();
        expect_child_success(pid);
        return 0;
    }

    io_write16(COMM_SIODATA8, 0xBEEF);
    StartLink(0x6000); // MULTIPLAYER mode + IRQ enable, slave
    b.sync();
    // The commit writes SIOMULTI1 and clears the busy bit; both must land
    // without a second master frame.
    while (io_read16(COMM_SIOMULTI1) != 0xBEEF
        || (io_read16(COMM_SIOCNT) & 0x80) != 0) {
        CHECK(GetLinkMode() == LINK_CABLE_SOCKET,
            "slave: link dropped during exchange");
        CHECK(now_seconds() < deadline,
            "slave commit still pending: SIOCNT=0x%04x SIOMULTI1=0x%04x",
            io_read16(COMM_SIOCNT), io_read16(COMM_SIOMULTI1));
        CheckLinkConnection();
        LinkUpdate(80000);
    }
    CHECK(io_read16(COMM_SIOMULTI0) == 0xCAFE,
        "slave: SIOMULTI0=0x%04x, want 0xCAFE", io_read16(COMM_SIOMULTI0));
    // With SIOCNT bit 14 set the completion must flag the serial IRQ (IE is
    // zero in this harness, so the flag just sticks in IF).
    CHECK((io_read16(0x202) & 0x80) != 0,
        "slave: serial IRQ not flagged (IF=0x%04x)", io_read16(0x202));
    // Absent slots read back 0xffff, never stale zeros: gen-3 games scan
    // all four slots and treat anything else as "no partner".
    CHECK(io_read16(COMM_SIOMULTI2) == 0xffff
            && io_read16(COMM_SIOMULTI3) == 0xffff,
        "slave: absent slots read 0x%04x/0x%04x, want 0xffff",
        io_read16(COMM_SIOMULTI2), io_read16(COMM_SIOMULTI3));
    b.sync();
    CloseLink();
    _exit(0);
}

// ---- idle_overflow ----------------------------------------------------------

// linktime wraps negative after ~128 s of emulated idle; the slave's send
// gate (linktime >= transfer_start_time_from_master) was then unsatisfiable
// and the master starved into "Link timeout." Regression: idle both sides
// past the wrap, then assert an exchange still completes.
static int run_idle_overflow() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "client";

    setup_fake_io();
    establish(b, child, LINK_CABLE_SOCKET, 0);

    if (child)
        StartLink(0x2000); // multi mode before idling
    // 3 * 2^30 ticks of idle: linktime wraps negative after the second.
    for (int i = 0; i < 3; i++)
        LinkUpdate(0x40000000);
    b.sync(); // both sides idled past the overflow

    double deadline = now_seconds() + 15.0;
    if (!child) {
        io_write16(COMM_SIODATA8, 0xCAFE);
        while (io_read16(COMM_SIOMULTI1) != 0xBEEF) {
            CHECK(GetLinkMode() == LINK_CABLE_SOCKET,
                "master: link dropped after idle overflow");
            CHECK(now_seconds() < deadline,
                "master timed out after idle overflow: SIOMULTI1=0x%04x",
                io_read16(COMM_SIOMULTI1));
            StartLink(0x2080);
            LinkUpdate(80000);
        }
    } else {
        io_write16(COMM_SIODATA8, 0xBEEF);
        while (io_read16(COMM_SIOMULTI0) != 0xCAFE) {
            CHECK(GetLinkMode() == LINK_CABLE_SOCKET,
                "slave: link dropped after idle overflow");
            CHECK(now_seconds() < deadline,
                "slave timed out after idle overflow: SIOMULTI0=0x%04x",
                io_read16(COMM_SIOMULTI0));
            CheckLinkConnection();
            LinkUpdate(80000);
            // Pace to roughly real time: an unpaced loop advances emulated
            // ticks ~100x faster than a real emulator and would climb out
            // of the negative-linktime window before the master's
            // wall-clock timeout, hiding the very bug this test guards
            // against.
            usleep(4000);
        }
    }

    b.sync();
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

// ---- gp ---------------------------------------------------------------------

// RCNT General-Purpose mode, as driven below: 0x8000 selects GP mode; data
// bits SC/SD/SI/SO are 0-3; direction bits (1 = output) are 4-7; bit 8
// enables the serial IRQ on an SI falling edge. The cable crosses SO and
// SI; undriven lines read pull-up 1s. LinkUpdate(80000) crosses the GP
// receive-poll throttle (~TICKS_PER_FRAME / 4) every iteration.

static void gp_wait_nibble(const char* who, uint16_t want) {
    double deadline = now_seconds() + 15.0;
    while ((io_read16(COMM_RCNT) & 0xF) != want) {
        CHECK(GetLinkMode() == LINK_CABLE_SOCKET,
            "%s: link dropped waiting for nibble 0x%x", who, want);
        CHECK(now_seconds() < deadline,
            "%s: RCNT data nibble stuck at 0x%x, want 0x%x", who,
            io_read16(COMM_RCNT) & 0xF, want);
        CheckLinkConnection();
        LinkUpdate(80000);
    }
}

static int run_gp() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "client";

    setup_fake_io();
    establish(b, child, LINK_CABLE_SOCKET, 0);

    if (!child) {
        // (a) Drive SO low; our own inputs read pull-up 1s (the all-input
        // client drives nothing) and our SO output reads back its latch.
        StartGPLink(0x8080); // GP, SO output, SO = 0
        gp_wait_nibble("server", 0x7);
        b.sync(); // client saw SI = 0 (nibble 0xB)

        // (b) Raise SO; the client's SI must follow.
        StartGPLink(0x8088);
        b.sync(); // client saw nibble 0xF

        // (c) Swap directions: the client drives, we read SI = 0.
        StartGPLink(0x8000); // all inputs
        b.sync(); // client now drives SO low
        gp_wait_nibble("server", 0xB);
        b.sync(); // direction swap verified

        // (d) SI falling-edge IRQ on the client: it re-armed as all-input
        // with the SI IRQ enabled (SI reads pull-up 1); driving our SO low
        // is the falling edge that must raise its serial IRQ.
        b.sync(); // client latched SI = 1 with the IRQ enabled
        StartGPLink(0x8080);
        b.sync(); // client observed IF bit 7

        CloseLink();
        expect_child_success(pid);
        return 0;
    }

    // (a) All inputs: SI must follow the server's driven SO (low), the
    // other three lines read pull-up 1s.
    StartGPLink(0x8000);
    gp_wait_nibble("client", 0xB);
    b.sync();

    // (b) The server raised SO.
    gp_wait_nibble("client", 0xF);
    b.sync();

    // (c) Swap: drive our SO low, the server reads SI = 0.
    StartGPLink(0x8080);
    b.sync();
    b.sync(); // server verified nibble 0xB

    // (d) Arm the SI IRQ as all-input (the server is all-input from (c),
    // so SI reads 1), latch the high level, then wait for the falling edge.
    StartGPLink(0x8100); // GP, all inputs, SI-IRQ enable
    gp_wait_nibble("client", 0xF);
    CHECK((io_read16(IO_REG_IF) & 0x80) == 0, "IF.7 set before the edge");
    b.sync(); // tell the server to drive SO low
    double deadline = now_seconds() + 15.0;
    while ((io_read16(IO_REG_IF) & 0x80) == 0) {
        CHECK(GetLinkMode() == LINK_CABLE_SOCKET,
            "client: link dropped waiting for the SI IRQ");
        CHECK(now_seconds() < deadline,
            "client: SI falling edge never raised the serial IRQ (RCNT=0x%04x)",
            io_read16(COMM_RCNT));
        CheckLinkConnection();
        LinkUpdate(80000);
    }
    CHECK((io_read16(COMM_RCNT) & 4) == 0, "IRQ raised but SI still reads 1");
    b.sync();

    CloseLink();
    _exit(0);
}

// ---- bind_conflict ----------------------------------------------------------

static int run_bind_conflict() {
    // Occupy the link port with a plain listener.
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0, "socket failed: %s", strerror(errno));
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(IP_LINK_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(fd, (struct sockaddr*)&sa, sizeof(sa)) == 0, "bind failed: %s",
        strerror(errno));
    CHECK(listen(fd, 1) == 0, "listen failed: %s", strerror(errno));

    g_last_system_message[0] = 0;
    EnableLinkServer(true, 1);
    ConnectionState st = InitLink(LINK_CABLE_SOCKET);
    CHECK(st == LINK_ERROR, "InitLink on a busy port returned %d", (int)st);
    CHECK(strstr(g_last_system_message, "already in use") != nullptr,
        "unexpected message: \"%s\"", g_last_system_message);
    close(fd);

    // A non-local bind address must warn and fall back to all interfaces.
    IP_LINK_PORT++;
    IP_LINK_BIND_ADDRESS = "203.0.113.1"; // TEST-NET-3, never a local address
    g_last_system_message[0] = 0;
    st = InitLink(LINK_CABLE_SOCKET);
    CHECK(st == LINK_NEEDS_UPDATE,
        "InitLink with a non-local bind address returned %d", (int)st);
    CHECK(strstr(g_last_system_message, "not a local address") != nullptr,
        "unexpected message: \"%s\"", g_last_system_message);
    CloseLink();
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
            "disconnect_goodbye|bind_conflict|gp>\n",
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
    if (strcmp(test, "slave_commit") == 0)
        return run_slave_commit();
    if (strcmp(test, "idle_overflow") == 0)
        return run_idle_overflow();
    if (strcmp(test, "gb") == 0)
        return run_gb();
    if (strcmp(test, "disconnect") == 0)
        return run_disconnect(false);
    if (strcmp(test, "disconnect_goodbye") == 0)
        return run_disconnect(true);
    if (strcmp(test, "bind_conflict") == 0)
        return run_bind_conflict();
    if (strcmp(test, "gp") == 0)
        return run_gp();

    fprintf(stderr, "unknown test '%s'\n", test);
    return 2;
}
