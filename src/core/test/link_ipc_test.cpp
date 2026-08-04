// Two-process harness for the same-machine IPC link (POSIX shared memory +
// named semaphores + flock liveness files).
//
// Usage: link_ipc_test --test <name>
//   role_assignment   25 rounds of two instances racing InitLink; exactly
//                     one must become machine 0 and one machine 1.
//   stale_recovery    A creator dies without CloseLink (SIGKILL-equivalent);
//                     the next instance must sweep the corpse and become
//                     machine 0 rather than joining a dead session. Also
//                     checks last-one-out unlinks the shm object.
//   gb_exchange       8 GB serial byte swaps, gbStartLinkIPC (master) vs
//                     gbLinkUpdateIPC (slave) — the Pokémon-trade path.
//   gb_third_instance A third GB-mode instance must be rejected with
//                     LINK_ERROR (the GB protocol indexes with 1 - linkid).
//   gba_cable_2p      One full 2-player 16-bit multiplayer transfer through
//                     StartLink/LinkUpdate; both sides must see
//                     SIOMULTI0/1 = 0x1234/0x5678. The most timing-coupled
//                     scenario; registered under its own ctest name so the
//                     deterministic tests above stay the gate.
//   gp_exchange       General-Purpose (RCNT GPIO) mode: driven pin levels
//                     cross the link in both directions, undriven lines
//                     read pull-up 1s, an SI falling edge raises the serial
//                     IRQ, and leaving GP mode doesn't disturb a subsequent
//                     multiplayer transfer.
//
// Every run sets a pid-derived VBAM_LINK_NAMESPACE before touching the link,
// isolating it from real emulator instances and parallel CI jobs.

#include "link_test_support.h"

#include <fcntl.h>
#include <sys/mman.h>

static char g_namespace[16];

static bool is_slot0() {
    return strcmp(MakeInstanceFilename("t.sav"), "t.sav") == 0;
}

static bool is_slot1() {
    return strcmp(MakeInstanceFilename("t.sav"), "t-2.sav") == 0;
}

// ---- role_assignment --------------------------------------------------------

static int run_role_assignment() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "child";

    SetLinkTimeout(500);

    for (int round = 0; round < 25; round++) {
        b.sync(); // race the two InitLink calls as closely as possible
        ConnectionState st = InitLink(LINK_CABLE_IPC);
        CHECK(st == LINK_OK, "round %d: InitLink returned %d", round, (int)st);
        uint8_t my_slot = is_slot0() ? 0 : (is_slot1() ? 1 : 0xff);
        CHECK(my_slot != 0xff, "round %d: unexpected instance filename", round);
        uint8_t peer_slot = b.swap(my_slot);
        CHECK(my_slot != peer_slot,
            "round %d: both instances claimed slot %d", round, my_slot);
        b.sync(); // both verified before either closes
        CloseLink();
        b.sync(); // both closed before the next round starts
    }

    if (child)
        _exit(0);
    expect_child_success(pid);
    return 0;
}

// ---- stale_recovery ---------------------------------------------------------

static int run_stale_recovery() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);

    if (child) {
        g_role = "child";
        SetLinkTimeout(500);
        ConnectionState st = InitLink(LINK_CABLE_IPC);
        CHECK(st == LINK_OK, "creator InitLink returned %d", (int)st);
        CHECK(is_slot0(), "creator did not get slot 0");
        b.sync();
        // Die without CloseLink: shm + semaphores stay behind, the flock
        // liveness lock is dropped by the kernel.
        _exit(0);
    }

    b.sync();
    expect_child_success(pid);

    // The corpse must be swept: we become the creator of a fresh session,
    // not slave #2 of a dead one (which is how the link used to wedge
    // permanently after any crash on macOS).
    SetLinkTimeout(500);
    ConnectionState st = InitLink(LINK_CABLE_IPC);
    CHECK(st == LINK_OK, "InitLink after crash returned %d", (int)st);
    CHECK(is_slot0(), "joined the dead session instead of sweeping it");
    CloseLink();

    // Last one out must unlink the shm object.
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/VBA link memory%s", g_namespace);
    int fd = shm_open(shm_name, O_RDWR, 0);
    if (fd >= 0)
        close(fd);
    CHECK(fd < 0, "shm object %s still exists after last CloseLink", shm_name);
    return 0;
}

// ---- gb_exchange ------------------------------------------------------------

static int run_gb_exchange() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "child";

    SetLinkTimeout(2000);

    if (!child) {
        // Creator = master (internal clock).
        ConnectionState st = InitLink(LINK_GAMEBOY_IPC);
        CHECK(st == LINK_OK, "master InitLink returned %d", (int)st);
        CHECK(is_slot0(), "master did not get slot 0");
        gbInitLinkIPC();
        b.sync(); // joiner attached
        b.sync(); // both initialized, start exchanging

        for (int i = 0; i < 8; i++) {
            uint8_t dat = gbStartLinkIPC((uint8_t)(0x40 + i));
            CHECK(dat == (uint8_t)(0x90 + i),
                "byte %d: master got 0x%02x, want 0x%02x", i, dat, 0x90 + i);
        }
        b.sync(); // slave done
        CloseLink();
        expect_child_success(pid);
        return 0;
    }

    b.sync(); // creator attached first
    ConnectionState st = InitLink(LINK_GAMEBOY_IPC);
    CHECK(st == LINK_OK, "slave InitLink returned %d", (int)st);
    CHECK(is_slot1(), "slave did not get slot 1");
    gbInitLinkIPC();
    b.sync();

    int received = 0;
    double deadline = now_seconds() + 15.0;
    while (received < 8) {
        CHECK(now_seconds() < deadline,
            "slave stuck after %d/8 bytes", received);
        // Pre-load the reply for the byte we are about to receive, exactly
        // like GB hardware exchanges bytes simultaneously.
        uint16_t r = gbLinkUpdateIPC((uint8_t)(0x90 + received), 1);
        if (r & 1) {
            uint8_t dat = (uint8_t)(r >> 8);
            CHECK(dat == (uint8_t)(0x40 + received),
                "byte %d: slave got 0x%02x, want 0x%02x", received, dat,
                0x40 + received);
            received++;
        }
    }
    b.sync();
    CloseLink();
    _exit(0);
}

// ---- gb_third_instance ------------------------------------------------------

static int run_gb_third_instance() {
    Barrier b1; // parent <-> first joiner
    pid_t pid1 = fork();
    CHECK(pid1 >= 0, "fork failed");
    b1.set_child(pid1 == 0);

    if (pid1 == 0) {
        g_role = "joiner";
        SetLinkTimeout(500);
        b1.sync(); // creator attached
        ConnectionState st = InitLink(LINK_GAMEBOY_IPC);
        CHECK(st == LINK_OK, "second instance InitLink returned %d", (int)st);
        CHECK(is_slot1(), "second instance did not get slot 1");
        b1.sync(); // attached; parent may spawn the third instance
        b1.sync(); // third instance verified; tear down
        CloseLink();
        _exit(0);
    }

    SetLinkTimeout(500);
    ConnectionState st = InitLink(LINK_GAMEBOY_IPC);
    CHECK(st == LINK_OK, "creator InitLink returned %d", (int)st);
    b1.sync();
    b1.sync(); // joiner attached

    Barrier b2; // parent <-> third instance
    pid_t pid2 = fork();
    CHECK(pid2 >= 0, "fork failed");
    b2.set_child(pid2 == 0);

    if (pid2 == 0) {
        g_role = "third";
        SetLinkTimeout(500);
        ConnectionState st3 = InitLink(LINK_GAMEBOY_IPC);
        CHECK(st3 == LINK_ERROR,
            "third GB instance was accepted (returned %d); the GB protocol "
            "indexes linkcmd/linksync with 1 - linkid and would corrupt "
            "memory", (int)st3);
        _exit(0);
    }

    expect_child_success(pid2);
    b1.sync(); // release the joiner
    CloseLink();
    expect_child_success(pid1);
    return 0;
}

// ---- gba_cable_2p -----------------------------------------------------------

static int run_gba_cable_2p() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "child";

    setup_fake_io();
    SetLinkTimeout(1000);

    if (!child) {
        ConnectionState st = InitLink(LINK_CABLE_IPC);
        CHECK(st == LINK_OK, "master InitLink returned %d", (int)st);
        CHECK(is_slot0(), "master did not get slot 0");
        b.sync(); // slave attached
        b.sync(); // slave is pumping its update loop

        // Give the slave a head start into its LinkUpdate loop; the
        // per-transfer handshake waits are bounded by linktimeout.
        usleep(50 * 1000);
        io_write16(COMM_SIODATA8, 0x1234);
        StartLink(0x2080); // MULTIPLAYER, start bit, 9600 baud

        double deadline = now_seconds() + 10.0;
        while (io_read16(COMM_SIOMULTI0) != 0x1234
            || io_read16(COMM_SIOMULTI1) != 0x5678) {
            CHECK(now_seconds() < deadline,
                "master timed out: SIOMULTI0=0x%04x SIOMULTI1=0x%04x",
                io_read16(COMM_SIOMULTI0), io_read16(COMM_SIOMULTI1));
            LinkUpdate(4096);
        }
        b.sync(); // both sides verified
        CloseLink();
        expect_child_success(pid);
        return 0;
    }

    b.sync(); // creator attached first
    ConnectionState st = InitLink(LINK_CABLE_IPC);
    CHECK(st == LINK_OK, "slave InitLink returned %d", (int)st);
    CHECK(is_slot1(), "slave did not get slot 1");
    io_write16(COMM_SIODATA8, 0x5678);
    StartLink(0x2000); // MULTIPLAYER mode, no start bit (slave)
    b.sync();

    double deadline = now_seconds() + 10.0;
    while (io_read16(COMM_SIOMULTI0) != 0x1234
        || io_read16(COMM_SIOMULTI1) != 0x5678) {
        CHECK(now_seconds() < deadline,
            "slave timed out: SIOMULTI0=0x%04x SIOMULTI1=0x%04x",
            io_read16(COMM_SIOMULTI0), io_read16(COMM_SIOMULTI1));
        LinkUpdate(4096);
    }
    b.sync();
    CloseLink();
    _exit(0);
}

// ---- idle_overflow ----------------------------------------------------------

// linktime is a signed 32-bit tick counter that only transfer activity
// resets; ~128 s of emulated idle wraps it negative. The slave's start gate
// (linktime >= linkmem->lastlinktime) was then unsatisfiable for the next
// ~128 s, and the old overflow clamp was gated on numtransfers != 0, so a
// session's FIRST transfer after a long idle found a dead cable (Pokémon at
// the Cable Club). Regression: pump > 2^31 idle ticks through both sides,
// then assert a transfer still completes.
static int run_idle_overflow() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "child";

    setup_fake_io();
    SetLinkTimeout(1000);

    if (!child) {
        ConnectionState st = InitLink(LINK_CABLE_IPC);
        CHECK(st == LINK_OK, "master InitLink returned %d", (int)st);
        b.sync(); // slave attached
        // 3 * 2^30 ticks of idle: linktime wraps negative after the second.
        for (int i = 0; i < 3; i++)
            LinkUpdate(0x40000000);
        b.sync(); // both sides idled past the overflow
        b.sync(); // slave is pumping its update loop

        usleep(50 * 1000);
        io_write16(COMM_SIODATA8, 0x1234);
        StartLink(0x2080);

        double deadline = now_seconds() + 10.0;
        while (io_read16(COMM_SIOMULTI0) != 0x1234
            || io_read16(COMM_SIOMULTI1) != 0x5678) {
            CHECK(now_seconds() < deadline,
                "master timed out after idle overflow: "
                "SIOMULTI0=0x%04x SIOMULTI1=0x%04x",
                io_read16(COMM_SIOMULTI0), io_read16(COMM_SIOMULTI1));
            LinkUpdate(4096);
        }
        b.sync();
        CloseLink();
        expect_child_success(pid);
        return 0;
    }

    b.sync(); // creator attached first
    ConnectionState st = InitLink(LINK_CABLE_IPC);
    CHECK(st == LINK_OK, "slave InitLink returned %d", (int)st);
    io_write16(COMM_SIODATA8, 0x5678);
    StartLink(0x2000);
    for (int i = 0; i < 3; i++)
        LinkUpdate(0x40000000);
    b.sync();
    b.sync();

    double deadline = now_seconds() + 10.0;
    while (io_read16(COMM_SIOMULTI0) != 0x1234
        || io_read16(COMM_SIOMULTI1) != 0x5678) {
        CHECK(now_seconds() < deadline,
            "slave timed out after idle overflow: "
            "SIOMULTI0=0x%04x SIOMULTI1=0x%04x",
            io_read16(COMM_SIOMULTI0), io_read16(COMM_SIOMULTI1));
        LinkUpdate(4096);
        // Pace to roughly real time: an unpaced loop advances emulated
        // ticks ~100x faster than a real emulator and would climb out of
        // the negative-linktime window before the master's wall-clock
        // timeout, hiding the very bug this test guards against.
        usleep(200);
    }
    b.sync();
    CloseLink();
    _exit(0);
}

// ---- gp_exchange ------------------------------------------------------------

// RCNT General-Purpose mode, as driven below: 0x8000 selects GP mode; data
// bits SC/SD/SI/SO are 0-3; direction bits (1 = output) are 4-7; bit 8
// enables the serial IRQ on an SI falling edge. The cable crosses SO and
// SI, so one side's driven SO is the other side's SI level; undriven lines
// read pull-up 1s.

static void gp_wait_nibble(const char* who, uint16_t want) {
    double deadline = now_seconds() + 10.0;
    while ((io_read16(COMM_RCNT) & 0xF) != want) {
        CHECK(now_seconds() < deadline,
            "%s: RCNT data nibble stuck at 0x%x, want 0x%x", who,
            io_read16(COMM_RCNT) & 0xF, want);
        LinkUpdate(4096);
    }
}

static int run_gp_exchange() {
    Barrier b;
    pid_t pid = fork();
    CHECK(pid >= 0, "fork failed");
    bool child = (pid == 0);
    b.set_child(child);
    if (child)
        g_role = "child";

    setup_fake_io();
    SetLinkTimeout(1000);

    if (!child) {
        ConnectionState st = InitLink(LINK_CABLE_IPC);
        CHECK(st == LINK_OK, "master InitLink returned %d", (int)st);
        CHECK(is_slot0(), "master did not get slot 0");
        b.sync(); // slave attached

        // (a) Drive SO low; our own inputs read pull-up 1s (the all-input
        // slave drives nothing) and our SO output reads back its latch.
        StartGPLink(0x8080); // GP, SO output, SO = 0
        gp_wait_nibble("master", 0x7);
        b.sync(); // slave saw SI = 0 (nibble 0xB)

        // (b) Raise SO; the slave's SI must follow.
        StartGPLink(0x8088);
        b.sync(); // slave saw nibble 0xF

        // (c) Swap directions: the slave drives, we read SI = 0.
        StartGPLink(0x8000); // all inputs
        b.sync(); // slave now drives SO low
        gp_wait_nibble("master", 0xB);
        b.sync(); // direction swap verified

        // (d) SI falling-edge IRQ: the slave re-armed as all-input with the
        // SI IRQ enabled (its SI reads pull-up 1); driving our SO low is
        // the falling edge that must raise its serial IRQ.
        b.sync(); // slave latched SI = 1 with the IRQ enabled
        StartGPLink(0x8080);
        b.sync(); // slave observed IF bit 7

        // (e) Leave GP mode and run one multiplayer transfer; GP state must
        // not leak into it (mirrors gba_cable_2p).
        StartGPLink(0);
        b.sync(); // both left GP mode; slave armed its transfer reply
        usleep(50 * 1000); // slave head start into its LinkUpdate loop
        io_write16(COMM_SIODATA8, 0x1234);
        StartLink(0x2080); // MULTIPLAYER, start bit, 9600 baud

        double deadline = now_seconds() + 10.0;
        while (io_read16(COMM_SIOMULTI0) != 0x1234
            || io_read16(COMM_SIOMULTI1) != 0x5678) {
            CHECK(now_seconds() < deadline,
                "master timed out after GP: SIOMULTI0=0x%04x SIOMULTI1=0x%04x",
                io_read16(COMM_SIOMULTI0), io_read16(COMM_SIOMULTI1));
            LinkUpdate(4096);
        }
        b.sync(); // both sides verified
        CloseLink();
        expect_child_success(pid);
        return 0;
    }

    b.sync(); // creator attached first
    ConnectionState st = InitLink(LINK_CABLE_IPC);
    CHECK(st == LINK_OK, "slave InitLink returned %d", (int)st);
    CHECK(is_slot1(), "slave did not get slot 1");

    // (a) All inputs: SI must follow the master's driven SO (low), the
    // other three lines read pull-up 1s.
    StartGPLink(0x8000);
    gp_wait_nibble("slave", 0xB);
    b.sync();

    // (b) The master raised SO.
    gp_wait_nibble("slave", 0xF);
    b.sync();

    // (c) Swap: drive our SO low, the master reads SI = 0.
    StartGPLink(0x8080);
    b.sync();
    b.sync(); // master verified nibble 0xB

    // (d) Arm the SI IRQ as all-input (SI reads 1: the master stopped
    // driving in (c)... it is still all-input from (c)); latch the high
    // level, then wait for the master's falling edge to raise IF bit 7.
    StartGPLink(0x8100); // GP, all inputs, SI-IRQ enable
    gp_wait_nibble("slave", 0xF); // SI = 1 latched by the update path
    CHECK((io_read16(IO_REG_IF) & 0x80) == 0, "IF.7 set before the edge");
    b.sync(); // tell the master to drive SO low
    double deadline = now_seconds() + 10.0;
    while ((io_read16(IO_REG_IF) & 0x80) == 0) {
        CHECK(now_seconds() < deadline,
            "slave: SI falling edge never raised the serial IRQ (RCNT=0x%04x)",
            io_read16(COMM_RCNT));
        LinkUpdate(4096);
    }
    CHECK((io_read16(COMM_RCNT) & 4) == 0, "IRQ raised but SI still reads 1");
    b.sync();

    // (e) Multiplayer transfer after GP.
    StartGPLink(0);
    io_write16(COMM_SIODATA8, 0x5678);
    StartLink(0x2000); // MULTIPLAYER mode, no start bit (slave)
    b.sync();

    deadline = now_seconds() + 10.0;
    while (io_read16(COMM_SIOMULTI0) != 0x1234
        || io_read16(COMM_SIOMULTI1) != 0x5678) {
        CHECK(now_seconds() < deadline,
            "slave timed out after GP: SIOMULTI0=0x%04x SIOMULTI1=0x%04x",
            io_read16(COMM_SIOMULTI0), io_read16(COMM_SIOMULTI1));
        LinkUpdate(4096);
    }
    b.sync();
    CloseLink();
    _exit(0);
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
            "usage: %s --test <role_assignment|stale_recovery|gb_exchange|"
            "gb_third_instance|gba_cable_2p|gp_exchange>\n",
            argv[0]);
        return 2;
    }

    // Isolate this run's shm/semaphore/lock-file names from real emulator
    // instances and from parallel CI jobs. Must happen before the first
    // link call; the core latches the namespace on first use.
    snprintf(g_namespace, sizeof(g_namespace), "t%x", (unsigned)getpid());
    setenv("VBAM_LINK_NAMESPACE", g_namespace, 1);

    arm_watchdog(90);

    if (strcmp(test, "role_assignment") == 0)
        return run_role_assignment();
    if (strcmp(test, "stale_recovery") == 0)
        return run_stale_recovery();
    if (strcmp(test, "gb_exchange") == 0)
        return run_gb_exchange();
    if (strcmp(test, "gb_third_instance") == 0)
        return run_gb_third_instance();
    if (strcmp(test, "gba_cable_2p") == 0)
        return run_gba_cable_2p();
    if (strcmp(test, "idle_overflow") == 0)
        return run_idle_overflow();
    if (strcmp(test, "gp_exchange") == 0)
        return run_gp_exchange();

    fprintf(stderr, "unknown test '%s'\n", test);
    return 2;
}
