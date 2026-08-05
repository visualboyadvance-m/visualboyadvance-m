#include "core/gba/gbaLink.h"

// This file was written by denopqrihg
// with major changes by tjm

#if defined(NO_LINK)
#error "This file should not be compiled with NO_LINK."
#endif  // defined(NO_LINK)

#if defined(_WIN32)

#include <winsock2.h>
#include <Windows.h>
// timeBeginPeriod/timeEndPeriod; not pulled in by Windows.h under
// WIN32_LEAN_AND_MEAN (which the MSVC toolchain file defines globally).
#include <mmsystem.h>

#else  // !defined(_WIN32)

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <semaphore.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__ANDROID__)
// Bionic has no POSIX shared memory at all (there is no shm_open()/
// shm_unlink()), and its named semaphores are declared but unimplemented:
// <semaphore.h> carries the comment "These aren't actually implemented."
// above sem_open()/sem_close()/sem_unlink(), which always fail with ENOSYS.
// The Android IPC backend further down replaces both with a file-backed
// shared mapping holding process-shared unnamed semaphores.
#include <limits.h>
#endif  // defined(__ANDROID__)

#endif  // defined(_WIN32)

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../../../third_party/sfml/include/SFML/Network.hpp"

#include <libintl.h>
#define _(x) gettext(x)

#include "core/base/message.h"
#include "core/base/port.h"
#include "core/gba/gba.h"
#include "core/gba/gbaCpu.h"
#include "core/gba/internal/gbaSockClient.h"

#ifdef _MSC_VER
#if __STDC_WANT_SECURE_LIB__
#define snprintf sprintf_s
#else
#define snprintf _snprintf
#endif
#endif

#ifdef UPDATE_REG
#undef UPDATE_REG
#endif
#define UPDATE_REG(address, value) WRITE16LE(((uint16_t*)&g_ioMem[address]), value)

static int vbaid = 0;
const char* MakeInstanceFilename(const char* Input)
{
    if (vbaid == 0) {
        return Input;
    }

    static char* result = NULL;
    if (result != NULL) {
        free(result);
    }

    const size_t len = strlen(Input) + 16;
    result = (char*)malloc(len);
    const char* p = strrchr(Input, '.');
    if (p != NULL)
        snprintf(result, len, "%.*s-%d.%s", (int)(p - Input), Input, vbaid + 1, p + 1);
    else
        snprintf(result, len, "%s-%d", Input, vbaid + 1);
    return result;
}

enum {
    SENDING = 0,
    RECEIVING = 1
};

enum siocnt_lo_32bit {
    SIO_INT_CLOCK = 0x0001,
    SIO_INT_CLOCK_SEL_2MHZ = 0x0002,
    SIO_TRANS_FLAG_RECV_ENABLE = 0x0004,
    SIO_TRANS_FLAG_SEND_DISABLE = 0x0008,
    SIO_TRANS_START = 0x0080,
    SIO_TRANS_32BIT = 0x1000,
    SIO_IRQ_ENABLE = 0x4000
};

// If disabled, gba core won't call any (non-joybus) link functions
bool gba_link_enabled = false;

bool speedhack = true;

// Transfer-start doorbell, one per slot (see LinkAheadThrottleStep and the
// ring in StartCableIPC). The master publishes a new transfer by bumping
// linkmem->numtransfers, which peers only ever POLL -- there is no wakeup at
// transfer start (the linksync tokens signal data readiness later in the
// exchange). A peer napping in the ahead-throttle is therefore deaf to a
// transfer it is needed for until its nap quantum expires: >= 1 ms on
// Windows (Sleep(1)), which at a transfer-dense screen (FFTA link menus,
// ~27 transfers/frame) gates the master's rendezvous ~14-16x per frame --
// the measured 60->37 fps collapse that sets in once audio pacing engages.
// The doorbell makes the nap interruptible: the master rings every live
// peer when it publishes a transfer, and the throttle nap waits on the
// bell instead of sleeping blind. Best-effort: creation failure just
// leaves the old blind sleep.
#if (defined __WIN32__ || defined _WIN32)
static HANDLE link_doorbell[4];
#else
[[maybe_unused]] static sem_t* link_doorbell[4];
#endif
static int link_doorbell_self = -1;

#define LOCAL_LINK_NAME "VBA link memory"

#include <stdint.h>

uint16_t IP_LINK_PORT = 5738;

std::string IP_LINK_BIND_ADDRESS = "*";

// Directory holding the on-disk files backing the IPC (same-machine) link:
// the whole shared mapping on Android (see AndroidLinkShmPath()) and the
// flock(2) liveness/init lock files on other POSIX systems (see
// LinkLockFilePath()). A frontend that knows a better location (an Android
// app's own cache dir, say) can set this before InitLink(); when it is
// empty a location is derived automatically.
std::string LOCAL_LINK_DIR;

// The byte a peer sends to announce it is leaving. inbuffer/outbuffer are
// plain char, which is *unsigned* on ARM -- so on Android/AArch64 comparing
// a received byte against -32 is always false and a clean disconnect would
// never be recognized. Compare the byte value instead of the char.
static const uint8_t kLinkGoodbyeByte = 0xe0;  // (uint8_t)-32

// The byte tagging a General-Purpose-mode (RCNT GPIO) state frame on the
// cable socket protocol: 4 bytes {4, kLinkGpByte, RCNT lo, RCNT hi}.
// Length 4 is already accepted by every frame-length check, and 0xe1 is
// distinct from every other live byte-1 value (tspeed 0-3, linkid << 2
// in {4, 8, 12}, and the goodbye marker above).
static const uint8_t kLinkGpByte = 0xe1;

#if defined(_WIN32)

// linksync[] are named SEMAPHORES on every platform: the cable path posts
// counted tokens (ReleaseSemaphore with trgbas - 1) for up to 4 players.
// The RFU and GB IPC paths, however, signal them through the event-style
// SetEvent/ResetEvent names, matching the POSIX sem_t shims below. Win32's
// real SetEvent()/ResetEvent() fail with ERROR_INVALID_HANDLE on a
// semaphore handle -- silently -- so on Windows every one of those peer
// wake-ups was lost and the other side's WaitForSingleObject always ran to
// its full timeout: the link crawled at timer-tick speed while the POSIX
// builds ran normally. Route the names onto semaphore operations instead.
static void LinkSemSignal(HANDLE s)
{
    // Fails harmlessly at the max count (4): "event already set".
    ReleaseSemaphore(s, 1, NULL);
}

static void LinkSemDrain(HANDLE s)
{
    while (WaitForSingleObject(s, 0) == WAIT_OBJECT_0) {
    }
}

#define SetEvent LinkSemSignal
#define ResetEvent LinkSemDrain

// The link paths sleep and wait in ~1 ms units: Sleep(1) in the ahead
// throttle, 1 ms semaphore timeouts in the RFU/GB lockstep. At the default
// ~15.6 ms Windows scheduler tick each of those quantizes up an order of
// magnitude, so a wait that should cost 1 ms costs a whole tick and the
// linked game slows to a crawl. Hold the 1 ms multimedia timer resolution
// for the life of the link session (matches the 0.2 ms nanosleep polls the
// POSIX builds get natively).
static bool link_timer_period_raised = false;

static void LinkRaiseTimerResolution()
{
    if (!link_timer_period_raised)
        link_timer_period_raised = (timeBeginPeriod(1) == TIMERR_NOERROR);
}

static void LinkRestoreTimerResolution()
{
    if (link_timer_period_raised) {
        timeEndPeriod(1);
        link_timer_period_raised = false;
    }
}

#else  // !defined(_WIN32)

#define ReleaseSemaphore(sem, nrel, orel) \
    do {                                  \
        for (int i = 0; i < nrel; i++)    \
            sem_post(sem);                \
    } while (0)
#define WAIT_TIMEOUT -1

static uint32_t GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// The GB/RFU IPC paths use the named link semaphores as event-style wakeups.
static void SetEvent(sem_t* s)
{
    sem_post(s);
}

static void ResetEvent(sem_t* s)
{
    while (sem_trywait(s) == 0)
        ;
}

#ifdef HAVE_SEM_TIMEDWAIT

// Android hands out CLOCK_REALTIME steps at will (NTP, the telephony stack,
// the user), and a stepped wall clock makes a CLOCK_REALTIME sem_timedwait()
// either return instantly or hang far past its deadline -- which for the link
// means spurious comm errors or a wedged emulator. Bionic's monotonic variant
// avoids that; it has existed since API 28.
#if defined(__ANDROID__) && __ANDROID_API__ >= 28
#define VBAM_LINK_TIMEDWAIT_CLOCK CLOCK_MONOTONIC
#define vbam_link_sem_timedwait   sem_timedwait_monotonic_np
#else
#define VBAM_LINK_TIMEDWAIT_CLOCK CLOCK_REALTIME
#define vbam_link_sem_timedwait   sem_timedwait
#endif

static int WaitForSingleObject(sem_t* s, int t)
{
    if (t <= 0)
        return sem_trywait(s) ? WAIT_TIMEOUT : 0;

    struct timespec ts;
    clock_gettime(VBAM_LINK_TIMEDWAIT_CLOCK, &ts);
    ts.tv_sec += t / 1000;
    ts.tv_nsec += (t % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    do {
        if (!vbam_link_sem_timedwait(s, &ts))
            return 0;
    } while (errno == EINTR);
    return WAIT_TIMEOUT;
}

#else
// macOS has no sem_timedwait(); emulate it with the same bounded
// sem_trywait + short-sleep poll LinkMemLock uses below. The previous
// emulation parked in sem_wait() with a SIGALRM/setitimer interrupt,
// which swapped a process-global signal handler and ITIMER_REAL on
// every call -- unsafe with threads (the signal can be delivered to a
// thread that is not in sem_wait(), leaving this one parked past its
// deadline) and hostile to any other itimer user in the process.
static int WaitForSingleObject(sem_t* s, int t)
{
    if (sem_trywait(s) == 0)
        return 0;
    if (t <= 0)
        return WAIT_TIMEOUT;

    const uint32_t start = GetTickCount();
    do {
        struct timespec ts = { 0, 200000 }; // 0.2 ms, as in LinkMemLock
        nanosleep(&ts, NULL);
        if (sem_trywait(s) == 0)
            return 0;
    } while ((int)(GetTickCount() - start) < t);
    return WAIT_TIMEOUT;
}
#endif
#endif

#define UNSUPPORTED -1
#define MULTIPLAYER 0
#define NORMAL8 1
#define NORMAL32 2
#define UART 3
#define JOYBUS 4
#define GP 5

static int GetSIOMode(uint16_t, uint16_t);
static void GpSocketRcntWritten(uint16_t value);
static ConnectionState InitSocket();
static void StartCableSocket(uint16_t siocnt);
static ConnectionState ConnectUpdateSocket(char* const message, size_t size);
static void UpdateCableSocket(int ticks);
static void CloseSocket();

const uint64_t TICKS_PER_FRAME = TICKS_PER_SECOND / 60;
const uint64_t BITS_PER_SECOND = 115200;
const uint64_t BYTES_PER_SECOND = BITS_PER_SECOND / 8;

static constexpr uint8_t kRfuBroadcastPayloadWords = 6;

static uint32_t lastjoybusupdate = 0;
static uint32_t nextjoybusupdate = 0;
static uint32_t lastcommand = 0;
static bool booted = false;

static ConnectionState JoyBusConnect();
static void JoyBusUpdate(int ticks);
static void JoyBusShutdown();

static ConnectionState ConnectUpdateRFUSocket(char* const message, size_t size);
static void StartRFUSocket(uint16_t siocnt);
bool LinkRFUUpdateSocket();
static void UpdateRFUSocket(int ticks);

#define RFU_INIT 0
#define RFU_COMM 1
#define RFU_SEND 2
#define RFU_RECV 3

typedef struct {
    uint16_t linkdata[5];
    uint16_t linkcmd[4];
    uint16_t numtransfers;
    int32_t lastlinktime;
    uint8_t numgbas; //# of GBAs (max vbaid value plus 1), used in Single computer
    uint8_t trgbas;
    uint8_t linkflags;

    uint8_t rfu_proto[5]; // 0=UDP-like, 1=TCP-like protocols to see whether the data important or not (may or may not be received successfully by the other side)
    uint16_t rfu_qid[5];
    int32_t rfu_q[5];
    uint32_t rfu_signal[5];
    uint8_t rfu_is_host[5]; //request to join
    //uint8_t rfu_joined[5]; //bool //currenlty joined
    uint16_t rfu_reqid[5]; //id to join
    uint16_t rfu_clientidx[5]; //only used by clients
    int32_t rfu_linktime[5];
    uint32_t rfu_broadcastdata[5][kRfuBroadcastPayloadWords + 1]; //for 0x16/0x1d/0x1e?
    uint32_t rfu_gdata[5]; //for 0x17/0x19?/0x1e?
    int32_t rfu_state[5]; //0=none, 1=waiting for ACK
    uint8_t rfu_listfront[5];
    uint8_t rfu_listback[5];
    rfu_datarec rfu_datalist[5][256];

    /*uint16_t rfu_qidlist[5][256];
	uint16_t rfu_qlist[5][256];
	uint32_t rfu_datalist[5][256][255];
	uint32_t rfu_timelist[5][256];*/

    // Committed RCNT of each slot, published on every CPU write so peers in
    // General-Purpose mode can read our driven pin levels. Zero (the
    // creator's memset / zero-fill default) means "not in GP mode, drives
    // nothing", which is exactly right for a slot that never wrote RCNT.
    // Appended last so all existing field offsets are unchanged; sizeof
    // still grows, so mismatched old/new builds refuse to share a session
    // (the segment size / AndroidLinkShm layout checks catch it).
    uint16_t gp_rcnt[5];

    // Each slot's emulated-CPU tick counter (16.78 MHz), published on every
    // LinkUpdate. This is what paces the IPC link at CPU rate: while the
    // link is hot (a transfer happened within the last few frames), an
    // instance whose clock runs more than a couple of frames ahead of a
    // live peer's briefly sleeps instead of emulating onward, so two GUI
    // processes advance in lockstep the way two real GBAs on one cable do.
    // Single-writer per slot, wrapping uint32 arithmetic. Appended last:
    // field offsets are unchanged, sizeof grows, so mismatched old/new
    // builds refuse to share a session (layout check above).
    uint32_t core_clock[4];
} LINKDATA;

class RFUServer {
    [[maybe_unused]] int numbytes;
    sf::SocketSelector fdset;
    [[maybe_unused]] int counter;
    [[maybe_unused]] int done;
    uint8_t current_host;

public:
    sf::TcpSocket tcpsocket[5];
    sf::IpAddress udpaddr[5] = { sf::IpAddress{0}, sf::IpAddress{0}, sf::IpAddress{0}, sf::IpAddress{0}, sf::IpAddress{0} };
    RFUServer(void);
    sf::Packet& Serialize(sf::Packet& packet, int slave);
    void DeSerialize(sf::Packet& packet, int slave);
    void Send(void);
    void Recv(void);
};

class RFUClient {
    sf::SocketSelector fdset;
    int numbytes;

public:
    sf::IpAddress serveraddr{0};
    unsigned short serverport;
    bool transferring;
    RFUClient(void);
    void Send(void);
    void Recv(void);
    sf::Packet& Serialize(sf::Packet& packet);
    void DeSerialize(sf::Packet& packet);
    void CheckConn(void);
};

// RFU crap (except for numtransfers note...should probably check that out)
[[maybe_unused]] static LINKDATA* linkmem = NULL;
static LINKDATA rfu_data;
static uint8_t rfu_cmd, rfu_qsend, rfu_qrecv_broadcast_data_len;
static int rfu_state, rfu_polarity, rfu_counter, rfu_masterq;
// numtransfers seems to be used interchangeably with linkmem->numtransfers
// in rfu code; probably a bug?
static int rfu_transfer_end;
// in local comm, setting this keeps slaves from trying to communicate even
// when master isn't
static uint16_t numtransfers = 0;

// time until next broadcast
static int rfu_last_broadcast_time;

static uint32_t rfu_masterdata[255];
bool rfu_enabled = false;
bool rfu_initialized = false;
bool rfu_waiting = false;
uint8_t rfu_qsend2, rfu_cmd2, rfu_lastcmd, rfu_lastcmd2;
uint16_t rfu_id, rfu_idx;
static int gbaid = 0;
static int gbaidx = 0;
bool rfu_ishost, rfu_cansend;
int rfu_lasttime;
uint32_t rfu_buf;
uint16_t PrevVAL = 0;
uint32_t PrevCOM = 0, PrevDAT = 0;
uint8_t rfu_numclients = 0;
uint8_t rfu_curclient = 0;
uint32_t rfu_clientlist[5];

static RFUServer rfu_server;
static RFUClient rfu_client;

uint8_t gbSIO_SC = 0;
bool EmuReseted = true;
bool LinkIsWaiting = false;
bool LinkFirstTime = true;

static ConnectionState InitIPC();
static void StartCableIPC(uint16_t siocnt);
static void ReconnectCableIPC();
static void UpdateCableIPC(int ticks);
static void StartRFU(uint16_t siocnt);
static void UpdateRFUIPC(int ticks);
static void CloseIPC();

struct LinkDriver {
    typedef ConnectionState(ConnectFunc)();
    typedef ConnectionState(ConnectUpdateFunc)(char* const message, size_t size);
    typedef void(StartFunc)(uint16_t siocnt);
    typedef void(UpdateFunc)(int ticks);
    typedef void(CloseFunc)();

    LinkMode mode;
    ConnectFunc* connect;
    ConnectUpdateFunc* connectUpdate;
    StartFunc* start;
    UpdateFunc* update;
    CloseFunc* close;
    bool uses_socket;
};

static const LinkDriver* linkDriver = NULL;
static ConnectionState gba_connection_state = LINK_OK;

static int linktime = 0;

static GBASockClient* dol = NULL;
static sf::IpAddress joybusHostAddr = sf::IpAddress::LocalHost;

static const LinkDriver linkDrivers[] = {
    { LINK_CABLE_IPC, InitIPC, NULL, StartCableIPC, UpdateCableIPC, CloseIPC, false },
    { LINK_RFU_IPC, InitIPC, NULL, StartRFU, UpdateRFUIPC, CloseIPC, false },
    { LINK_GAMEBOY_IPC, InitIPC, NULL, NULL, NULL, CloseIPC, false },
    { LINK_CABLE_SOCKET, InitSocket, ConnectUpdateSocket, StartCableSocket, UpdateCableSocket, CloseSocket, true },
    { LINK_RFU_SOCKET, InitSocket, ConnectUpdateRFUSocket, StartRFUSocket, UpdateRFUSocket, CloseSocket, true },
    { LINK_GAMECUBE_DOLPHIN, JoyBusConnect, NULL, NULL, JoyBusUpdate, JoyBusShutdown, false },
    { LINK_GAMEBOY_SOCKET, InitSocket, ConnectUpdateSocket, NULL, NULL, CloseSocket, true },
};

enum {
    JOY_CMD_RESET = 0xff,
    JOY_CMD_STATUS = 0x00,
    JOY_CMD_READ = 0x14,
    JOY_CMD_WRITE = 0x15
};

typedef struct {
    sf::TcpSocket tcpsocket;
    sf::TcpListener tcplistener;
    uint16_t numslaves;
    int connectedSlaves;
    bool server;
    // speedhack toggle from EnableSpeedHacks(); currently write-only, kept
    // because it is part of the public link configuration surface.
    bool speed;
} LANLINKDATA;

class CableServer {
    sf::SocketSelector fdset;
    //timeval udptimeout;
    char inbuffer[256], outbuffer[256];
    int32_t* intinbuffer;
    uint16_t* uint16_tinbuffer;
    int32_t* intoutbuffer;
    uint16_t* uint16_toutbuffer;
    [[maybe_unused]] int done;

public:
    sf::TcpSocket tcpsocket[4];
    sf::IpAddress udpaddr[4] = { sf::IpAddress{0}, sf::IpAddress{0}, sf::IpAddress{0}, sf::IpAddress{0} };
    // replies still owed to us from exchanges that timed out
    int gb_pending;
    CableServer(void);
    void Send(void);
    bool Recv(void);
    void SendGB(void);
    bool RecvGB(int timeout_ms);
    bool ExchangeGB(uint8_t b, int timeout_ms);
};

class CableClient {
    sf::SocketSelector fdset;
    char inbuffer[256], outbuffer[256];
    int32_t* intinbuffer;
    uint16_t* uint16_tinbuffer;
    int32_t* intoutbuffer;
    uint16_t* uint16_toutbuffer;
    [[maybe_unused]] int numbytes;

public:
    sf::IpAddress serveraddr{0};
    unsigned short serverport;
    bool transferring;
    // replies still owed to us from exchanges that timed out
    int gb_pending;
    CableClient(void);
    void Send(void);
    bool Recv(void);
    void SendGB(void);
    bool RecvGB(int timeout_ms);
    bool ExchangeGB(uint8_t b, int timeout_ms);
    void CheckConn(void);
};

// Wall-clock budget for peer waits (IPC semaphore waits, socket recv retry
// accumulation). The wx GUI overrides this via SetLinkTimeout (default 500);
// 500 here too, so embedders that don't call SetLinkTimeout (the SDL
// frontend, harnesses) get a survivable wait instead of the old 1 ms, which
// dropped a healthy peer on nearly every in-game transfer.
static int linktimeout = 500;
static LANLINKDATA lanlink;
static uint16_t cable_data[4];
// Add extra byte to suppress warning.
static uint8_t cable_gb_data[5];
static CableServer ls;
static CableClient lc;

// time to end of single GBA's transfer, in 16.78 MHz clock ticks
// first index is GBA #
[[maybe_unused]] static const int trtimedata[4][4] = {
    // 9600 38400 57600 115200
    { 34080, 8520, 5680, 2840 },
    { 65536, 16384, 10923, 5461 },
    { 99609, 24903, 16602, 8301 },
    { 133692, 33423, 22282, 11141 }
};

// time to end of transfer
// for 3 slaves, this is time to transfer machine 4
// for < 3 slaves, this is time to transfer last machine + time to detect lack
// of start bit from next slave
// first index is (# of slaves) - 1
static const int trtimeend[3][4] = {
    // 9600 38400 57600 115200
    { 72527, 18132, 12088, 6044 },
    { 106608, 26652, 17768, 8884 },
    { 133692, 33423, 22282, 11141 }
};

// The slave paces each transfer start to the master's published clock so
// exchanges land at hardware-plausible emulated times. If the slave's clock
// is further behind than this, the clocks are desynced (an instance was
// paused or backgrounded, or the idle-overflow clamp fired on one side
// only); waiting out the difference just serves dead air while the peer
// waits for our reply, so resync and start immediately instead.
static const int kMaxLinkClockLagTicks = 10 * (TICKS_PER_SECOND / 60);

// Loose lockstep in the other direction: real linked GBAs share one wall
// clock, but two emulator processes don't, and the master pays all the
// blocking costs of the link (semaphore waits, socket receives), so the
// slave's emulated clock tends to run AHEAD of the master's transfer
// stream. A slave that gets several frames ahead piles up vblanks without
// serial IRQs and the game declares the link dead (Pokémon errors after 10
// quiet vblanks, mid-trade). Once the slave is more than this far ahead of
// the last transfer, it briefly sleeps instead of emulating onward --
// bounded by a per-gap wall budget so a master that legitimately stopped
// transferring (scene fade, menu, or gone entirely) can only slow the
// slave down for a moment, never freeze it.
static const int kMaxLinkClockAheadTicks = 3 * (TICKS_PER_SECOND / 60);

// CPU-rate lockstep for the IPC link (see LINKDATA::core_clock): while the
// link is hot -- own clock within kHotWindowTicks of the last transfer --
// an instance more than kMaxLinkLeadTicks of emulated time ahead of a live
// peer sleeps briefly instead of emulating onward. Outside the hot window
// (single-player sections, a peer that has not loaded a ROM yet) both
// instances free-run.
static const int32_t kMaxLinkLeadTicks = 2 * (TICKS_PER_SECOND / 60);
static const int32_t kHotWindowTicks = 8 * (TICKS_PER_SECOND / 60);
static uint32_t last_hot_own_clock = 0;
// Leads are measured relative to the moment the link became hot, NOT from
// process start: instances legitimately diverge by minutes of emulated
// time while the link is cold (one player boots or fast-forwards long
// before the other reaches the Cable Club), and that pre-existing offset
// is the game's business to synchronize, not ours. Pacing only equalizes
// the RATE of progress while transfers are flowing.
static bool link_was_hot = false;
static uint32_t hot_base_own = 0;
static uint32_t hot_base_peer[4] = { 0, 0, 0, 0 };
// Generous: the throttle must ride out multi-second peer stalls (occluded
// window, coalesced timers), not just
// scheduler jitter. It only engages while this side is already several
// frames ahead of the transfer stream, and it disengages the moment the
// master publishes the next transfer, so a master that legitimately
// paused transfers costs at most one budget's worth of slowdown before
// the slave runs free again.
static const int kAheadThrottleBudgetUs = 10000000;
static int ahead_throttle_budget_us = kAheadThrottleBudgetUs;

// How often an in-flight IPC wait re-verifies that a stalled peer's
// process still exists (see LinkPeerAlive further down). A GUI emulator
// process can stop emulating INDEFINITELY for reasons that have nothing to
// do with the game -- an occluded window, timer coalescing, a dragged
// window, GPU contention, an Android app in the background -- and a real
// cable never times out, so there is deliberately NO wall-clock ceiling on
// the wait itself. linktimeout is the probe granularity of the wait, not a
// drop deadline; a peer that left cleanly cleared its flag and is dropped
// immediately, one whose process died with the flag still set is caught by
// the periodic liveness probe.
static const int kPeerAliveProbeMs = 2000;

// Wait for a linksync token during an in-flight IPC transfer. Returns
// false when the wait should be abandoned (peer left the session, local
// close requested, or the peer's process died). peer_mask selects
// the linkflags bits that must stay set for the wait to keep going.
static bool WaitForLinkToken(int sem_index, uint8_t peer_mask);

// One bounded throttle step per LinkUpdate call while ahead: ~0.2 ms on
// POSIX (nanosleep), ~1 ms on Windows (Sleep(1) at the raised 1 ms timer
// resolution -- Windows cannot sleep shorter). Each branch debits the
// budget by what it actually sleeps.
static bool LinkAheadThrottleStep()
{
    if (ahead_throttle_budget_us <= 0)
        return false;
    // Nap on the doorbell when we have one: drain stale rings (bounding the
    // count without sem_getvalue, which macOS lacks), then wait for a fresh
    // ring or the quantum. A ring consumed by the drain belonged to a
    // transfer the post-nap numtransfers check picks up anyway.
    if (link_doorbell_self >= 0 && link_doorbell[link_doorbell_self] != NULL) {
        ahead_throttle_budget_us -= 1000;
        while (WaitForSingleObject(link_doorbell[link_doorbell_self], 0) != WAIT_TIMEOUT) {
        }
        WaitForSingleObject(link_doorbell[link_doorbell_self], 1);
        return true;
    }
#ifdef _WIN32
    ahead_throttle_budget_us -= 1000;
    Sleep(1);
#else
    ahead_throttle_budget_us -= 200;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 200 * 1000;
    nanosleep(&ts, NULL);
#endif
    return true;
}

// ---------------------------------------------------------------------------
// Cable-path tracing: set VBAM_TRACE_CABLE=1 to stream per-transfer events
// (starts, latches, commits, timeouts, drops) to stderr with linktime /
// numtransfers stamps. Always compiled — unlike the VBAM_TRACE_SIO hooks in
// gba.cpp, which only exist under the VBAM_HB_TRACE build flag — because a
// game like Pokémon clocks nine transfers per frame and per-transfer
// visibility is the only way to diagnose a stall in a release build. Cost
// when disabled is one cached-bool test per event.
// Trace sink: stderr when VBAM_TRACE_CABLE is set in the environment; a
// per-process /tmp/vbam-cable-<pid>.log when the trigger file
// /tmp/vbam-trace-cable exists (`touch /tmp/vbam-trace-cable`) -- that
// second path is for GUI instances launched from Finder, whose stderr goes
// nowhere. NULL means tracing is off.
static FILE* CableTraceStream()
{
    static FILE* const stream = []() -> FILE* {
        const char* v = getenv("VBAM_TRACE_CABLE");
        if (v && *v && *v != '0')
            return stderr;
#ifndef _WIN32
        if (FILE* trigger = fopen("/tmp/vbam-trace-cable", "r")) {
            fclose(trigger);
            char path[64];
            snprintf(path, sizeof(path), "/tmp/vbam-cable-%d.log",
                (int)getpid());
            FILE* f = fopen(path, "w");
            if (f)
                setvbuf(f, NULL, _IOLBF, 0);
            return f;
        }
#endif
        return NULL;
    }();
    return stream;
}

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
static void CableTrace(const char* fmt, ...)
{
    FILE* out = CableTraceStream();
    if (!out)
        return;
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    const uint32_t ms = GetTickCount();
    fprintf(out, "[CABLE %u.%03u] %s\n", ms / 1000, ms % 1000, buf);
}

// RFU-path tracing: set VBAM_TRACE_RFU=1 (or `touch /tmp/vbam-trace-rfu`
// for Finder-launched GUI instances) to stream every wireless-adapter
// transfer to stderr / a per-pid log: the word the game clocked out, the
// state machine's reply, and the state/command context. The adapter
// protocol is a strict request/response lockstep, so this is the only
// diagnostic that shows *which* exchange desynchronized a game's RFU
// library. Same design as CableTraceStream above.
static FILE* RfuTraceStream()
{
    static FILE* const stream = []() -> FILE* {
        const char* v = getenv("VBAM_TRACE_RFU");
        if (v && *v && *v != '0')
            return stderr;
#ifndef _WIN32
        if (FILE* trigger = fopen("/tmp/vbam-trace-rfu", "r")) {
            fclose(trigger);
            char path[64];
            snprintf(path, sizeof(path), "/tmp/vbam-rfu-%d.log",
                (int)getpid());
            FILE* f = fopen(path, "w");
            if (f)
                setvbuf(f, NULL, _IOLBF, 0);
            return f;
        }
#endif
        return NULL;
    }();
    return stream;
}

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
static void RfuTrace(const char* fmt, ...)
{
    FILE* out = RfuTraceStream();
    if (!out)
        return;
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    const uint32_t ms = GetTickCount();
    fprintf(out, "[RFU %u.%03u] %s\n", ms / 1000, ms % 1000, buf);
}

// Hodgepodge
static uint8_t tspeed = 3;
static int transfer_direction = 0;
static uint16_t linkid = 0;
#if (defined __WIN32__ || defined _WIN32)
static HANDLE linksync[4];
#else
[[maybe_unused]] static sem_t* linksync[4];
#endif
static int transfer_start_time_from_master = 0;
#if (defined __WIN32__ || defined _WIN32)
static HANDLE mmf = NULL;
#else
[[maybe_unused]] static int mmf = -1;
#endif
// Interprocess lock guarding *structural* mutations of the shared linkmem
// metadata: slot allocation in InitIPC, the linkflags/numgbas connection
// topology, numtransfers/trgbas bookkeeping, and disconnect cleanup. It is
// deliberately NOT held across the per-transfer handshake waits on
// linksync[] -- those semaphores coordinate who-sends-when between master
// and slaves, and holding this lock across them would deadlock (the master
// would wait for a slave that can't proceed without the same lock). Keep
// every guarded region short and free of blocking waits.
#if (defined __WIN32__ || defined _WIN32)
static HANDLE linkmem_lock = NULL;
#else
static sem_t* linkmem_lock = SEM_FAILED;
#endif

// ---------------------------------------------------------------------------
// Names of the shared IPC objects.
//
// VBAM_LINK_NAMESPACE (sanitized to [A-Za-z0-9_-], at most 10 chars) lets a
// test harness or CI job run link sessions isolated from a real emulator on
// the same machine. With it unset, every name is byte-identical to the
// historical ones. Longest macOS-visible name is "/VBA link event <sfx>N" =
// 17 + 10 chars, under the 31-char macOS shm/sem name limit.
static const std::string& LinkNamespaceSuffix()
{
    static const std::string suffix = [] {
        std::string s;
        if (const char* env = getenv("VBAM_LINK_NAMESPACE")) {
            for (const char* p = env; *p && s.size() < 10; p++)
                if (isalnum((unsigned char)*p) || *p == '_' || *p == '-')
                    s += *p;
        }
        return s;
    }();
    return suffix;
}

#if (defined __WIN32__ || defined _WIN32)
#define LINK_NAME_PREFIX ""
#else
#define LINK_NAME_PREFIX "/"
#endif

static std::string LinkShmName()
{
    return LINK_NAME_PREFIX LOCAL_LINK_NAME + LinkNamespaceSuffix();
}

static std::string LinkSemName(int i)
{
    return LINK_NAME_PREFIX "VBA link event " + LinkNamespaceSuffix() + (char)('1' + i);
}

static std::string LinkDoorbellName(int i)
{
    return LINK_NAME_PREFIX "VBA link doorbell " + LinkNamespaceSuffix() + (char)('1' + i);
}

static std::string LinkLockSemName()
{
    return LINK_NAME_PREFIX "VBA link lock" + LinkNamespaceSuffix();
}

#if !(defined __WIN32__ || defined _WIN32) && !defined(__ANDROID__)
// The POSIX backend pairs the named objects above with two flock(2)-based
// lock files (never unlinked -- unlink+recreate would split the lock across
// two inodes and let two probes both "succeed"):
//  - ".init" is held exclusively for the whole of InitIPC's role selection
//    and CloseIPC's last-one-out probe, so a joiner can never observe a
//    half-initialized segment or race the creator for slot 0.
//  - ".flock" is held shared by every attached instance for its whole
//    session. flock locks die with their process (even on SIGKILL), so a
//    LOCK_EX|LOCK_NB probe succeeding proves any existing shm/semaphores
//    are a crashed run's leftovers: sweep them and recreate instead of
//    joining a corpse (macOS shm objects otherwise persist until reboot).
//    The Android backend below already works this way.
// They live in a world-writable directory because the POSIX shm/sem
// namespace is system-global: a per-user $TMPDIR would let two users'
// probes disagree about one global object.
static int link_liveness_fd = -1;

static std::string LinkLockFilePath(const char* which)
{
    std::string dir = LOCAL_LINK_DIR;
    if (dir.empty())
        if (const char* env = getenv("VBAM_LINK_DIR"))
            dir = env;
    if (dir.empty())
        dir = "/tmp";
    return dir + "/vbam-link" + LinkNamespaceSuffix() + which;
}

// RAII holder of the exclusive ".init" lock.
class LinkInitLock {
public:
    explicit LinkInitLock(const std::string& path)
    {
        fd_ = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
        if (fd_ >= 0) {
            fchmod(fd_, 0666); // umask-proof; any user may take the lock
            int r;
            do {
                r = flock(fd_, LOCK_EX);
            } while (r != 0 && errno == EINTR);
            held_ = (r == 0);
        }
    }
    ~LinkInitLock()
    {
        if (fd_ >= 0) {
            if (held_)
                flock(fd_, LOCK_UN);
            close(fd_);
        }
    }
    bool held() const { return held_; }
    LinkInitLock(const LinkInitLock&) = delete;
    LinkInitLock& operator=(const LinkInitLock&) = delete;

private:
    int fd_ = -1;
    bool held_ = false;
};
#endif  // POSIX non-Android

#if defined(__ANDROID__)
// ---------------------------------------------------------------------------
// Android IPC backend
//
// Bionic offers none of the named POSIX IPC this code was written against:
// shm_open()/shm_unlink() do not exist, and sem_open()/sem_close()/
// sem_unlink() are stubs that always fail with ENOSYS. memfd_create() is
// API 30+ and, being anonymous, cannot be found by a peer by name anyway.
//
// So the entire session lives in one MAP_SHARED mapping of a regular file in
// the app's private directory -- every instance of the app runs under the
// same uid and sees the same data dir -- with the structural lock and the
// four handshake semaphores placed *inside* that mapping as process-shared
// unnamed semaphores. sem_init(..., pshared=1, ...) is supported by Bionic
// (it futexes on the shared address), so every existing sem_wait/sem_post/
// sem_trywait/sem_timedwait call site keeps working untouched.
//
// Each participant also holds a shared flock() on the file for as long as it
// is connected. That answers the two questions the named-IPC version got
// from O_EXCL: "am I the first instance?" is "can I take the lock
// exclusively?", and "am I the last one out?" is the same probe at teardown.
// Unlike a leftover shm object, it also makes a segment abandoned by a
// crashed instance detectable -- nobody holds the lock, so the next instance
// reinitializes it instead of joining a dead session with every slot marked
// taken. Role selection is serialized through a separate lock file so two
// instances starting at once cannot both decide they are the creator.
// ---------------------------------------------------------------------------

#define VBAM_LINK_SHM_MAGIC 0x4c4b4256u  // 'VBKL'

struct AndroidLinkShm {
    uint32_t magic;    // written last by the creator; joiners wait to see it
    uint32_t layout;   // sizeof(LINKDATA), so a stale file of a different
                       // build is rejected rather than misread
    sem_t lock;        // the linkmem structural lock, initial count 1
    sem_t sync[4];     // the per-slot handshake semaphores, initial count 0
    LINKDATA data;     // what linkmem points at
};

static AndroidLinkShm* android_shm = NULL;
static int android_shm_fd = -1;
static bool android_shm_created = false;
static std::string android_shm_path;

// The app's own cache dir, derived without any framework calls: for an
// Android app process /proc/self/cmdline is the package name (possibly with
// a ":subprocess" suffix), and the Android user id is the uid divided by the
// per-user offset. Returns an empty string when we are plainly not running
// as an app (an adb-shell binary, a test runner), whose cmdline is a path.
static std::string AndroidAppCacheDir()
{
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return std::string();

    char cmdline[192];
    ssize_t n = read(fd, cmdline, sizeof(cmdline) - 1);
    close(fd);
    if (n <= 0)
        return std::string();
    cmdline[n] = '\0';

    char* colon = strchr(cmdline, ':');
    if (colon != NULL)
        *colon = '\0';
    // A package name, not a path to an executable.
    if (cmdline[0] == '\0' || cmdline[0] == '/' || strchr(cmdline, '.') == NULL)
        return std::string();

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/data/user/%d/%s/cache",
        (int)(getuid() / 100000), cmdline);
    return std::string(path);
}

static bool AndroidDirUsable(const std::string& dir)
{
    if (dir.empty())
        return false;
    struct stat st;
    if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
        return false;
    return access(dir.c_str(), R_OK | W_OK | X_OK) == 0;
}

// Locate the backing file, preferring anything the frontend or the user
// pointed us at. All instances must resolve this to the same path, which is
// why the candidates are process-independent.
static const std::string& AndroidLinkShmPath()
{
    if (!android_shm_path.empty())
        return android_shm_path;

    const char* env_dir = getenv("VBAM_LINK_DIR");
    const char* tmp_dir = getenv("TMPDIR");
    const char* run_dir = getenv("XDG_RUNTIME_DIR");
    const std::string candidates[] = {
        LOCAL_LINK_DIR,
        env_dir != NULL ? std::string(env_dir) : std::string(),
        tmp_dir != NULL ? std::string(tmp_dir) : std::string(),
        run_dir != NULL ? std::string(run_dir) : std::string(),
        AndroidAppCacheDir(),
        std::string("/data/local/tmp"),
    };

    for (const std::string& dir : candidates) {
        if (AndroidDirUsable(dir)) {
            android_shm_path = dir + "/vbam-link.shm";
            break;
        }
    }
    return android_shm_path;
}

static void AndroidLinkShmUnlinkFiles()
{
    if (android_shm_path.empty())
        return;
    unlink(android_shm_path.c_str());
    unlink((android_shm_path + ".lock").c_str());
    // The per-slot liveness files (see LinkAliveFilePath). Only ever
    // reached when nobody holds the session's shared lock, so no live
    // instance can still be flock-holding one of these.
    for (int i = 0; i < 4; i++)
        unlink((android_shm_path + ".slot" + (char)('0' + i)).c_str());
}

// Release our mapping and liveness lock; the last instance out also removes
// the backing file so a later run starts from a clean slate.
static void AndroidLinkShmClose()
{
    if (android_shm != NULL) {
        void* base = android_shm;
        android_shm = NULL;
        munmap(base, sizeof(AndroidLinkShm));
    }
    if (android_shm_fd >= 0) {
        // Taking the lock exclusively can only succeed if no other instance
        // still holds its shared lock, i.e. we really are the last one out.
        if (flock(android_shm_fd, LOCK_EX | LOCK_NB) == 0)
            AndroidLinkShmUnlinkFiles();
        close(android_shm_fd);  // also drops the flock
        android_shm_fd = -1;
    }
    android_shm_created = false;
}

// Map the shared session, creating and initializing it if we are first.
// On success android_shm is mapped and android_shm_created says which role
// we took; on failure nothing is left open.
static bool AndroidLinkShmOpen()
{
    const std::string& path = AndroidLinkShmPath();
    if (path.empty()) {
        fprintf(stderr,
            "gbaLink: no writable directory for the IPC link segment; set "
            "VBAM_LINK_DIR or use the LAN link instead\n");
        return false;
    }

    // Serialize creator/joiner selection across instances. Best effort: if
    // the lock file cannot be opened we still work, just with the same
    // startup race the named-IPC backends have.
    const std::string lock_path = path + ".lock";
    int init_fd = open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (init_fd >= 0)
        while (flock(init_fd, LOCK_EX) < 0 && errno == EINTR)
            ;

    android_shm_fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (android_shm_fd < 0) {
        fprintf(stderr, "gbaLink: cannot open %s: %s\n", path.c_str(), strerror(errno));
        if (init_fd >= 0) {
            flock(init_fd, LOCK_UN);
            close(init_fd);
        }
        return false;
    }

    // Nobody holding a shared lock means no live peer: we are the first
    // instance, and anything already in the file is a crashed run's corpse.
    android_shm_created = flock(android_shm_fd, LOCK_EX | LOCK_NB) == 0;

    void* map = MAP_FAILED;
    if (android_shm_created) {
        if (ftruncate(android_shm_fd, 0) == 0
            && ftruncate(android_shm_fd, (off_t)sizeof(AndroidLinkShm)) == 0) {
            map = mmap(NULL, sizeof(AndroidLinkShm), PROT_READ | PROT_WRITE,
                MAP_SHARED, android_shm_fd, 0);
        }
        if (map != MAP_FAILED) {
            AndroidLinkShm* shm = (AndroidLinkShm*)map;
            memset(shm, 0, sizeof(*shm));
            bool ok = sem_init(&shm->lock, 1, 1) == 0;
            for (int i = 0; i < 4 && ok; i++)
                ok = sem_init(&shm->sync[i], 1, 0) == 0;
            if (!ok) {
                fprintf(stderr, "gbaLink: sem_init failed: %s\n", strerror(errno));
                munmap(map, sizeof(AndroidLinkShm));
                map = MAP_FAILED;
            } else {
                shm->layout = (uint32_t)sizeof(LINKDATA);
                // Publish last: a joiner spins on magic, and this release
                // store is what makes the initialized semaphores visible.
                __atomic_store_n(&shm->magic, VBAM_LINK_SHM_MAGIC, __ATOMIC_RELEASE);
            }
        }
    } else {
        // A peer is alive, so the segment is fully initialized -- the init
        // lock above held us behind the creator until it published magic.
        struct stat st;
        if (fstat(android_shm_fd, &st) == 0
            && (size_t)st.st_size >= sizeof(AndroidLinkShm)) {
            map = mmap(NULL, sizeof(AndroidLinkShm), PROT_READ | PROT_WRITE,
                MAP_SHARED, android_shm_fd, 0);
        }
        if (map != MAP_FAILED) {
            AndroidLinkShm* shm = (AndroidLinkShm*)map;
            if (__atomic_load_n(&shm->magic, __ATOMIC_ACQUIRE) != VBAM_LINK_SHM_MAGIC
                || shm->layout != (uint32_t)sizeof(LINKDATA)) {
                fprintf(stderr, "gbaLink: %s is not a usable link segment\n", path.c_str());
                munmap(map, sizeof(AndroidLinkShm));
                map = MAP_FAILED;
            }
        }
    }

    if (map == MAP_FAILED) {
        if (android_shm_created)
            AndroidLinkShmUnlinkFiles();
        close(android_shm_fd);
        android_shm_fd = -1;
        android_shm_created = false;
        if (init_fd >= 0) {
            flock(init_fd, LOCK_UN);
            close(init_fd);
        }
        return false;
    }

    // Downgrade to (or take) the shared lock we hold while connected. Safe
    // to do under the init lock: no other instance can be probing for the
    // exclusive lock right now.
    flock(android_shm_fd, LOCK_SH);
    if (init_fd >= 0) {
        flock(init_fd, LOCK_UN);
        close(init_fd);
    }

    android_shm = (AndroidLinkShm*)map;
    return true;
}
#endif  // defined(__ANDROID__)

// ---------------------------------------------------------------------------
// Per-slot liveness for the IPC link.
//
// A stalled peer must never be dropped on a timer: a real cable never times
// out, and a GUI emulator process can legitimately stop emulating for a
// very long time (occluded or dragged window, modal dialog, an Android app
// sent to the background). But a peer whose PROCESS died with its linkflags
// bit still set would otherwise hang every wait forever, so each instance
// holds a per-slot token that its OS releases on any exit, even SIGKILL: a
// named kernel object on Windows (it vanishes when the owning process's
// last handle closes), an flock(2)-held file everywhere else (the lock dies
// with the process). LinkPeerAlive() probes that token to tell "slow" from
// "gone" without ever putting a deadline on "slow".
#if (defined __WIN32__ || defined _WIN32)
static HANDLE link_alive_handle = NULL;

static std::string LinkAliveName(int slot)
{
    return "VBA link alive " + LinkNamespaceSuffix() + (char)('1' + slot);
}

static void LinkAliveAcquire(int slot)
{
    if (link_alive_handle == NULL)
        link_alive_handle = CreateEventA(NULL, TRUE, FALSE, LinkAliveName(slot).c_str());
}

static void LinkAliveRelease()
{
    if (link_alive_handle != NULL) {
        CloseHandle(link_alive_handle);
        link_alive_handle = NULL;
    }
}

static bool LinkPeerAlive(int slot)
{
    // The named object exists exactly while some process holds a handle to
    // it; close the probe handle immediately so we never keep a dead
    // peer's token alive ourselves between probes.
    HANDLE h = OpenEventA(SYNCHRONIZE, FALSE, LinkAliveName(slot).c_str());
    if (h != NULL) {
        CloseHandle(h);
        return true;
    }
    return false;
}

#else  // POSIX (including Android)

static int link_alive_fd = -1;

static std::string LinkAliveFilePath(int slot)
{
#if defined(__ANDROID__)
    // Next to the shared mapping, so every instance resolves the same file
    // (AndroidLinkShmUnlinkFiles removes these with the session).
    return AndroidLinkShmPath() + ".slot" + (char)('0' + slot);
#else
    char which[8];
    snprintf(which, sizeof(which), ".slot%d", slot);
    return LinkLockFilePath(which);
#endif
}

static void LinkAliveAcquire(int slot)
{
    if (link_alive_fd >= 0)
        return;
    link_alive_fd = open(LinkAliveFilePath(slot).c_str(),
        O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    if (link_alive_fd < 0)
        return;
    fchmod(link_alive_fd, 0666); // umask-proof; any user's instance may probe
    // Never blocks: slot allocation is serialized, and a previous owner's
    // lock died with its process. On failure this slot simply has no
    // liveness token; peers then treat it as alive and keep waiting.
    if (flock(link_alive_fd, LOCK_EX | LOCK_NB) != 0) {
        close(link_alive_fd);
        link_alive_fd = -1;
    }
}

static void LinkAliveRelease()
{
    if (link_alive_fd >= 0) {
        close(link_alive_fd); // drops the flock with it
        link_alive_fd = -1;
    }
}

static bool LinkPeerAlive(int slot)
{
    // A live owner holds LOCK_EX, so a non-blocking shared probe failing
    // proves the peer is alive, and the probe succeeding proves nobody
    // owns the slot. Can't-tell (no file, open failure) counts as alive:
    // better to keep waiting on a peer we cannot verify than to drop a
    // healthy one.
    int fd = open(LinkAliveFilePath(slot).c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return true;
    bool dead = (flock(fd, LOCK_SH | LOCK_NB) == 0);
    close(fd); // drops the probe lock
    return !dead;
}

#endif  // POSIX

// Acquire the linkmem structural lock, giving up after timeout_ms so a
// crashed peer that died holding the lock cannot wedge every other
// instance forever. Returns true only when the lock was actually taken;
// the caller must pass that result to LinkMemUnlock so a timed-out
// acquisition never posts a token it does not own.
static bool LinkMemLock(int timeout_ms)
{
#if (defined __WIN32__ || defined _WIN32)
    if (linkmem_lock == NULL)
        return false;
    return WaitForSingleObject(linkmem_lock, timeout_ms) == WAIT_OBJECT_0;
#else
    if (linkmem_lock == SEM_FAILED)
        return false;
    if (sem_trywait(linkmem_lock) == 0)
        return true;
    uint32_t start = GetTickCount();
    while ((int)(GetTickCount() - start) < timeout_ms) {
        struct timespec ts = { 0, 200000 }; // 0.2 ms
        nanosleep(&ts, NULL);
        if (sem_trywait(linkmem_lock) == 0)
            return true;
    }
    return false;
#endif
}

static void LinkMemUnlock(bool held)
{
    if (!held)
        return;
#if (defined __WIN32__ || defined _WIN32)
    if (linkmem_lock != NULL)
        ReleaseSemaphore(linkmem_lock, 1, NULL);
#else
    if (linkmem_lock != SEM_FAILED)
        sem_post(linkmem_lock);
#endif
}

// RAII wrapper for LinkMemLock/Unlock. Best-effort: if acquisition times
// out, held() is false and the region proceeds unguarded (degrading to the
// legacy racy behavior only in the crash-recovery corner) rather than
// deadlocking the emulator.
class LinkMemGuard {
    bool held_;

public:
    explicit LinkMemGuard(int timeout_ms = 500) : held_(LinkMemLock(timeout_ms)) {}
    ~LinkMemGuard() { LinkMemUnlock(held_); }
    bool held() const { return held_; }
    LinkMemGuard(const LinkMemGuard&) = delete;
    LinkMemGuard& operator=(const LinkMemGuard&) = delete;
};

inline static int GetSIOMode(uint16_t siocnt, uint16_t rcnt)
{
    if (!(rcnt & 0x8000)) {
        switch (siocnt & 0x3000) {
        case 0x0000:
            return NORMAL8;
        case 0x1000:
            return NORMAL32;
        case 0x2000:
            return MULTIPLAYER;
        case 0x3000:
            return UART;
        }
    }

    if (rcnt & 0x4000)
        return JOYBUS;

    return GP;
}

// ---------------------------------------------------------------------------
// General-Purpose (GP) mode: RCNT[15:14] = 10 turns the four cable lines
// SC/SD/SI/SO into 4-bit GPIO (RCNT bits 0-3 data, 4-7 direction with
// 1 = output, bit 8 = serial IRQ on SI falling edge). Games use it to probe
// cable/peer presence and for custom protocols. The cable drivers exchange
// each side's committed RCNT and model the wires here.
//
// Wiring of a standard GBA<->GBA cable: SC<->SC and SD<->SD straight
// through, SO and SI crossed (each side's SO drives the other side's SI).
// All lines are pulled up: a wire nobody drives reads 1. With more than two
// IPC instances the shared lines are the wired-AND of every driver (the
// 2-player case degenerates to exactly the crossed cable; real >2-player GP
// wiring through multi cables is not modeled). The socket transport is
// 2-player-exact: slave<->slave GP state is not relayed.

// Which peer pin feeds each local pin: SC<-SC, SD<-SD, SI<-SO, SO<-SI.
static const int kGpPeerPin[4] = { 0, 1, 3, 2 };

// Process-local GP session state. Deliberately not serialized in save
// states (like linktime/transfer_direction); it resets with Init*/Close*
// and resynchronizes on the next RCNT write or peer update.
static bool gp_prev_si = false;
static bool gp_prev_si_valid = false; // suppresses a phantom edge on entry
static bool gp_mode_active = false;   // last committed RCNT was GP mode
static uint16_t gp_peer_rcnt[4];      // socket: last state received per player
static uint16_t gp_last_sent = 0;     // socket: last state we transmitted
static int gp_poll_ticks = 0;         // socket: receive-poll throttle
static bool gp_socket_sent_initial = false;

static void GpResetState()
{
    gp_prev_si = false;
    gp_prev_si_valid = false;
    gp_mode_active = false;
    for (int i = 0; i < 4; i++)
        gp_peer_rcnt[i] = 0;
    gp_last_sent = 0;
    gp_poll_ticks = 0;
    gp_socket_sent_initial = false;
}

// Recompute the local input pins from the peers' published RCNT values.
// Rewrites ONLY data bits 0-3 whose direction bit says input; output-pin
// data, direction, IRQ-enable, and mode bits pass through untouched (an
// output pin reads back its own latch on hardware). A peer whose published
// RCNT is not GP mode contributes no drive -- its pins belong to its SIO
// engine -- which is also what makes mode transitions safe: leaving GP mode
// publishes a non-GP value and the peers stop honoring our old levels.
static uint16_t GpMergeInputs(uint16_t my, const uint16_t* peers, int npeers)
{
    uint16_t out = my;
    for (int pin = 0; pin < 4; pin++) {
        if (my & (1 << (pin + 4)))
            continue; // output: the latch reads back, never rewritten
        int level = 1; // pull-up default
        for (int p = 0; p < npeers; p++) {
            const uint16_t pr = peers[p];
            if ((pr >> 14) != 2)
                continue; // peer not in GP mode: not driving
            const int pp = kGpPeerPin[pin];
            if (pr & (1 << (pp + 4)))
                level &= (pr >> pp) & 1; // wired-AND of all drivers
        }
        out = (uint16_t)((out & ~(1 << pin)) | (level << pin));
    }
    return out;
}

// Commit a merged RCNT and deliver the SI falling-edge IRQ. Called only
// from the LinkUpdate paths, never from the register-write handler: an IRQ
// raised synchronously inside the write could be discarded by a BIOS
// IntrWait the game is just entering (see CPURaiseSioIRQ's contract).
static void GpCommitMerged(uint16_t merged)
{
    if (merged != READ16LE(&g_ioMem[COMM_RCNT]))
        UPDATE_REG(COMM_RCNT, merged);

    const bool si = (merged >> 2) & 1;
    if ((merged & 0x0100) && !(merged & 0x40) // IRQ enabled, SI is an input
        && gp_prev_si_valid && gp_prev_si && !si)
        CPURaiseSioIRQ();
    gp_prev_si = si;
    gp_prev_si_valid = true;
}

// Peers currently attached to the IPC session, by their published RCNT.
static int GpIpcCollectPeers(uint16_t peers[4])
{
    int n = 0;
    const int f = linkmem->linkflags;
    for (int i = 0; i < 4; i++) {
        if (i == linkid || !(f & (1 << i)))
            continue;
        peers[n++] = linkmem->gp_rcnt[i];
    }
    return n;
}

// A CPU write to RCNT was committed; publish it to the IPC session.
// gp_rcnt[linkid] is a single aligned word with exactly one writer (us), so
// no LinkMemGuard -- same discipline as the per-tick lastlinktime reads.
static void GpIpcRcntWritten(uint16_t value)
{
    if (!linkmem)
        return;

    linkmem->gp_rcnt[linkid] = value;

    // Refresh our input pins immediately so a write-then-read sequence sees
    // current peer state. Data bits only: no IRQ, no gp_prev_si update --
    // edge delivery stays in the update path (see GpCommitMerged).
    if ((value >> 14) == 2) {
        uint16_t peers[4];
        const int n = GpIpcCollectPeers(peers);
        const uint16_t merged = GpMergeInputs(value, peers, n);
        if (merged != READ16LE(&g_ioMem[COMM_RCNT]))
            UPDATE_REG(COMM_RCNT, merged);
    }
}

// Per-tick GP servicing for the IPC cable driver (local RCNT is GP mode).
static void GpIpcUpdate()
{
    if (!linkmem)
        return;

    // GP mode has no multiplayer transfer in flight; abandon any
    // half-finished cycle from before the mode switch. Peers' waits on
    // linksync[] are bounded by linktimeout, so nobody blocks forever.
    transfer_direction = 0;

    uint16_t peers[4];
    const int n = GpIpcCollectPeers(peers);
    GpCommitMerged(GpMergeInputs(READ16LE(&g_ioMem[COMM_RCNT]), peers, n));
}

LinkMode GetLinkMode()
{
    if (linkDriver && gba_connection_state == LINK_OK)
        return linkDriver->mode;
    else
        return LINK_DISCONNECTED;
}

bool GetLinkServerHost(char* const host, size_t size)
{
    if (host == NULL || size == 0) {
        return false;
    }

    host[0] = '\0';

    if (linkDriver && linkDriver->mode == LINK_GAMECUBE_DOLPHIN) {
#if __STDC_WANT_SECURE_LIB__
        strncpy_s(host, size, joybusHostAddr.toString().c_str(), size);
#else
        strncpy(host, joybusHostAddr.toString().c_str(), size);
#endif
    } else if (lanlink.server) {
        if (IP_LINK_BIND_ADDRESS == "*") {
            auto local_addr = sf::IpAddress::getLocalAddress();
            if (local_addr) {
#if __STDC_WANT_SECURE_LIB__
                strncpy_s(host, size, local_addr.value().toString().c_str(), size);
#else
                strncpy(host, local_addr.value().toString().c_str(), size);
#endif
            } else {
                return false;
            }
        } else {
#if __STDC_WANT_SECURE_LIB__
            strncpy_s(host, size, IP_LINK_BIND_ADDRESS.c_str(), size);
#else
            strncpy(host, IP_LINK_BIND_ADDRESS.c_str(), size);
#endif
        }
    }
    else {
#if __STDC_WANT_SECURE_LIB__
        strncpy_s(host, size, lc.serveraddr.toString().c_str(), size);
#else
        strncpy(host, lc.serveraddr.toString().c_str(), size);
#endif
    }

    // strncpy does not NUL-terminate a source exactly `size` bytes long.
    host[size - 1] = '\0';

    return true;
}

bool SetLinkServerHost(const char* host)
{
    sf::IpAddress addr{0};

    auto resolved = sf::IpAddress::resolve(host);
    if (!resolved) {
        return false;
    }
    addr = resolved.value();
    lc.serveraddr = addr;
    joybusHostAddr = addr;

    return true;
}

int GetLinkPlayerId()
{
    if (GetLinkMode() == LINK_DISCONNECTED) {
        return -1;
    } else if (linkid > 0) {
        return linkid;
    } else {
        return vbaid;
    }
}

void SetLinkTimeout(int value)
{
    linktimeout = value;
}

void EnableLinkServer(bool enable, int numSlaves)
{
    lanlink.server = enable;
    lanlink.numslaves = (uint16_t)numSlaves;
}

void EnableSpeedHacks(bool enable)
{
    lanlink.speed = enable;
}

void BootLink(int m_type, const char* hostAddr, int timeout, bool m_hacks, int m_numplayers)
{
    (void)m_numplayers; // unused param
    if (linkDriver) {
        // Connection has already been established
        return;
    }

    LinkMode mode = (LinkMode)m_type;

    if (mode == LINK_DISCONNECTED || mode == LINK_CABLE_SOCKET || mode == LINK_RFU_SOCKET || mode == LINK_GAMEBOY_SOCKET) {
        return;
    }

    // Close any previous link
    CloseLink();

    bool needsServerHost = (mode == LINK_GAMECUBE_DOLPHIN);

    if (needsServerHost) {
        bool valid = SetLinkServerHost(hostAddr);
        if (!valid) {
            return;
        }
    }

    SetLinkTimeout(timeout);
    EnableSpeedHacks(m_hacks);

    // Init link
    ConnectionState state = InitLink(mode);

    if (!linkDriver->uses_socket) {
        // The user canceled the connection attempt
        if (state == LINK_ABORT) {
            CloseLink();
            return;
        }

        // Something failed during init
        if (state == LINK_ERROR) {
            return;
        }
    } else {
        CloseLink();
        return;
    }
}

//////////////////////////////////////////////////////////////////////////
// Probably from here down needs to be replaced with SFML goodness :)
// tjm: what SFML goodness?  SFML for network, yes, but not for IPC

ConnectionState InitLink(LinkMode mode)
{
    if (mode == LINK_DISCONNECTED)
        return LINK_ABORT;

    // Do nothing if we are already connected
    if (GetLinkMode() != LINK_DISCONNECTED) {
        systemMessage(0, N_("Error, link already connected"));
        return LINK_ERROR;
    }

    // Find the link driver
    linkDriver = NULL;
    for (uint8_t i = 0; i < sizeof(linkDrivers) / sizeof(linkDrivers[0]); i++) {
        if (linkDrivers[i].mode == mode) {
            linkDriver = &linkDrivers[i];
            break;
        }
    }

    if (!linkDriver || !linkDriver->connect) {
        systemMessage(0, N_("Unable to find link driver"));
        return LINK_ERROR;
    }

#ifdef _WIN32
    // Raised before connect() so the handshake waits already run at 1 ms
    // granularity; restored in CloseLink (every failure path below funnels
    // through it).
    LinkRaiseTimerResolution();
#endif

    // Connect the link
    gba_connection_state = linkDriver->connect();

    if (gba_connection_state == LINK_ERROR) {
        CloseLink();
    }

    return gba_connection_state;
}

void StartLink(uint16_t siocnt)
{
    // Only drive a real transfer once the connection is fully established.
    // The socket connect handshake runs inside a modal dialog that pumps
    // the wx event loop, so emuMain (hence StartLink) can fire while the
    // link is still LINK_NEEDS_UPDATE; dispatching to the driver then would
    // touch half-open sockets and shared state mid-handshake.
    if (!linkDriver || !linkDriver->start || gba_connection_state != LINK_OK) {
        // Stand-alone (no real link cable) fallback. Some games rely on
        // SIOCNT being updated so they don't hang; the shared helper
        // commits the write with proper hardware-side masking (also used
        // by the NO_LINK / libretro build — see gba.cpp).
        SioStandaloneSiocntWrite(siocnt);
        return;
    }

    linkDriver->start(siocnt);
}

ConnectionState ConnectLinkUpdate(char* const message, size_t size)
{
    message[0] = '\0';

    if (!linkDriver || !linkDriver->connectUpdate || gba_connection_state != LINK_NEEDS_UPDATE) {
        gba_connection_state = LINK_ERROR;
        snprintf(message, size, N_("Link connection does not need updates."));

        return LINK_ERROR;
    }

    gba_connection_state = linkDriver->connectUpdate(message, size);

    return gba_connection_state;
}

void StartGPLink(uint16_t value)
{
    // The register-commit part (RCNT masking + the multiplayer-mode SIOCNT
    // status bits) is shared with the NO_LINK / libretro build — see
    // SioStandaloneRcntWrite in gba.cpp.
    value = SioStandaloneRcntWrite(value, linkid);

    // Publish the committed register to cable peers before the !value
    // early-out below: writing 0 *leaves* GP mode, and the peers must see
    // the departure or they would keep honoring our last driven levels.
    // (The multiplayer paths' own RCNT status writes are direct UPDATE_REGs
    // that never come through here, so the published value always reflects
    // the last CPU write — the correct hardware model.)
    switch (GetLinkMode()) {
    case LINK_CABLE_IPC:
        GpIpcRcntWritten(value);
        break;
    case LINK_CABLE_SOCKET:
        GpSocketRcntWritten(value);
        break;
    default:
        break;
    }

    if (!value) {
        gp_mode_active = false;
        return;
    }

    const bool now_gp = GetSIOMode(READ16LE(&g_ioMem[COMM_SIOCNT]), value) == GP;
    if (now_gp && !gp_mode_active)
        gp_prev_si_valid = false; // fresh GP entry: no phantom SI edge
    gp_mode_active = now_gp;

    if (now_gp && GetLinkMode() == LINK_RFU_IPC)
        rfu_state = RFU_INIT;
}

// Deferred link teardown.
//
// Several socket I/O helpers (CableServer/CableClient::Recv/RecvGB/CheckConn
// and RFUClient::Recv) detect a dropped peer while they are running *inside*
// a linkDriver->update / gbStartLink / gbLinkUpdate call. Calling CloseLink()
// from there is re-entrant: it nulls linkDriver and disconnects the very
// sockets the caller is about to keep touching. Instead they request a
// close, and the real CloseLink() runs at a safe point between emulation
// steps (top of LinkUpdate / CheckLinkConnection / the GB serial hooks).
static bool link_close_pending = false;

static void RequestLinkClose()
{
    link_close_pending = true;
}

// True once a disconnect has been observed, even before CloseLink() has run,
// so an in-flight exchange bails out immediately instead of issuing another
// socket op on a dead connection.
static bool LinkIsClosing()
{
    return link_close_pending || GetLinkMode() == LINK_DISCONNECTED;
}

static void ProcessDeferredLinkClose()
{
    if (link_close_pending) {
        link_close_pending = false;
        CloseLink();
    }
}

// See the declaration next to kPeerAliveProbeMs for the rationale. Blocking
// here is deliberate: it freezes this instance's emulated time so its game
// never observes the peer's stall -- exactly what a physical cable does.
// There is NO wall-clock ceiling on the wait: a healthy-but-stalled peer is
// waited out indefinitely on every platform. Only a peer that left the
// session (flag cleared) or whose process died (liveness probe) ends it.
static bool WaitForLinkToken(int sem_index, uint8_t peer_mask)
{
    if (peer_mask == 0)
        return false; // no live peers to wait for
    const int probe_ms = linktimeout > 0 ? linktimeout : 1;
    int since_alive_probe_ms = 0;
    for (;;) {
        if (WaitForSingleObject(linksync[sem_index], probe_ms) != WAIT_TIMEOUT)
            return true;
        if (LinkIsClosing())
            return false;
        if ((linkmem->linkflags & peer_mask) != peer_mask)
            return false; // peer left the session cleanly
        since_alive_probe_ms += probe_ms;
        if (since_alive_probe_ms >= kPeerAliveProbeMs) {
            since_alive_probe_ms = 0;
            for (int slot = 0; slot < 4; slot++) {
                if ((peer_mask & (1 << slot)) && !LinkPeerAlive(slot)) {
                    CableTrace("ipc wait: peer %d process died, giving up",
                        slot);
                    return false;
                }
            }
        }
    }
}

void LinkUpdate(int ticks)
{
    // Perform any close requested by a socket helper on the previous step,
    // before touching linkDriver again.
    ProcessDeferredLinkClose();

    if (!linkDriver || !linkDriver->update) {
        return;
    }

    // Don't step transfers until the connection is fully established. The
    // socket connect handshake pumps the wx event loop from a modal dialog,
    // so this can be reached mid-handshake (state == LINK_NEEDS_UPDATE).
    if (gba_connection_state != LINK_OK) {
        return;
    }

    // this actually gets called every single instruction, so keep default
    // path as short as possible

    linktime += ticks;

    linkDriver->update(ticks);

    // update() may have flagged a dropped peer; tear it down now, out of
    // the driver dispatch.
    ProcessDeferredLinkClose();
}

void CheckLinkConnection()
{
    ProcessDeferredLinkClose();

    if (GetLinkMode() == LINK_CABLE_SOCKET) {
        if (linkid && !lc.transferring) {
            lc.CheckConn();
        }
    }

    ProcessDeferredLinkClose();
}

void CloseLink(void)
{
#ifdef _WIN32
    // Balanced with the raise in InitLink; a no-op when never raised, so
    // it is safe ahead of the driver check below.
    LinkRestoreTimerResolution();
#endif

    if (!linkDriver || !linkDriver->close) {
        return; // Nothing to do
    }

    linkDriver->close();
    linkDriver = NULL;

    return;
}

// ---------------------------------------------------------------------------
// Socket I/O discipline.
//
// The connect handshake runs fully non-blocking (it is polled from a modal
// UI dialog every 50 ms and must never stall a tick). Once a connection is
// established the sockets run *blocking*: an SFML blocking send() loops
// internally until everything is written (Partial is unreachable), and every
// receive below is guarded by a selector wait with a bounded budget so a
// stalled peer can cost the emulator thread at most ~50 ms per call.

// Every link protocol here is a request/response exchange of tiny frames
// (4-12 bytes, thousands per second in-game). With Nagle enabled the OS
// holds each frame until the previous one is ACKed, and the peer's delayed
// ACK stretches that to tens of milliseconds per exchange -- on Windows the
// classic Nagle + 200 ms delayed-ACK interaction made the LAN link crawl no
// matter how fast both emulators ran, the socket-transport twin of the
// 15.6 ms timer-tick crawl fixed by LinkRaiseTimerResolution. Latency beats
// throughput on a link cable; disable Nagle on every established socket.
static void LinkSetNoDelay(sf::TcpSocket& sock)
{
    const int one = 1;
    setsockopt(sock.getNativeHandle(), IPPROTO_TCP, TCP_NODELAY,
        (const char*)&one, sizeof(one));
}

// The canonical goodbye frame: 4 bytes, length + marker, zero padding. Sent
// in both directions with no reply round-trip -- a dying or dead peer can't
// be counted on to answer, and waiting for one used to block the emulator.
static void SendGoodbye(sf::TcpSocket& sock)
{
    char goodbye[4] = { 4, (char)kLinkGoodbyeByte, 0, 0 };
    (void)sock.send(goodbye, sizeof(goodbye));
}

// A failed send is the only way an idle endpoint notices a vanished peer
// (there may be nothing to receive for a long time); flag the close here.
static bool CheckSendResult(sf::Socket::Status st)
{
    if (st == sf::Socket::Status::Disconnected || st == sf::Socket::Status::Error) {
        RequestLinkClose();
        return false;
    }
    return true;
}

enum class RecvResult { Ok, Timeout, Dropped };

// Read exactly len bytes, spending at most budget_ms measured from the
// caller's budget clock (shared across several reads of one frame set).
static RecvResult ReceiveExact(sf::TcpSocket& sock, sf::SocketSelector& sel,
    char* buf, size_t len, sf::Clock& budget, int budget_ms)
{
    size_t got = 0;
    while (got < len) {
        const int remaining = budget_ms - (int)budget.getElapsedTime().asMilliseconds();
        if (remaining <= 0)
            return RecvResult::Timeout;
        sel.clear();
        sel.add(sock);
        if (!sel.wait(sf::milliseconds(remaining)))
            continue; // re-checks the budget above
        size_t nr = 0;
        sf::Socket::Status st = sock.receive(buf + got, len - got, nr);
        if (st == sf::Socket::Status::Disconnected || st == sf::Socket::Status::Error)
            return RecvResult::Dropped;
        // NotReady is only defensive here: established sockets are blocking.
        got += nr;
    }
    return RecvResult::Ok;
}

// A General-Purpose-mode state frame (see kLinkGpByte).
static void SendGpFrame(sf::TcpSocket& sock, uint16_t rcnt)
{
    char frame[4] = { 4, (char)kLinkGpByte, (char)(rcnt & 0xff), (char)(rcnt >> 8) };
    CheckSendResult(sock.send(frame, sizeof(frame)));
}

// A CPU write to RCNT was committed; publish it to the socket session.
// Send-on-change only, and only while GP mode is involved on our side (the
// value entering or leaving GP mode both matter to the peers); blocking
// sends from a register write match StartCableSocket's existing behavior.
static void GpSocketRcntWritten(uint16_t value)
{
    if (gba_connection_state != LINK_OK)
        return;
    if (value == gp_last_sent)
        return;
    if ((value >> 14) != 2 && (gp_last_sent >> 14) != 2)
        return;

    gp_last_sent = value;
    if (linkid) {
        SendGpFrame(lanlink.tcpsocket, value);
    } else {
        for (int i = 1; i <= lanlink.numslaves; i++)
            SendGpFrame(ls.tcpsocket[i], value);
    }
}

// Client-side connect handshake state (see InitSocket/ConnectUpdateSocket).
static sf::Clock connect_clock;
static bool client_handshake_connected = false;
static bool connect_attempt_failed = false;
static int32_t last_connect_attempt_ms = 0;
// The client keeps trying to reach the server for as long as the user
// lets it: the connect poll runs inside a cancellable progress dialog, so
// a wall-clock deadline would only second-guess the user (who may well
// have started the client before the server on purpose). Attempts are
// merely paced.
static const int kConnectRetryMs = 500;

// Accumulated missed-frame time in UpdateCableSocket, kept only for
// tracing. A stalled peer never times the link out (a real cable doesn't);
// only a real disconnect (goodbye frame, TCP drop, failed send) ends the
// session.
static int missed_recv_ms = 0;

// Tick accumulator for the cable slave's idle poll for the master's next
// start frame (see UpdateCableSocket).
static int cable_poll_ticks = 0;

// Server
CableServer::CableServer(void)
{
    intinbuffer = (int32_t*)inbuffer;
    uint16_tinbuffer = (uint16_t*)inbuffer;
    intoutbuffer = (int32_t*)outbuffer;
    uint16_toutbuffer = (uint16_t*)outbuffer;
    gb_pending = 0;
    // Never let uninitialized stack/heap bytes reach the wire.
    memset(inbuffer, 0, sizeof(inbuffer));
    memset(outbuffer, 0, sizeof(outbuffer));
}

void CableServer::Send(void)
{
    outbuffer[1] = tspeed;
    WRITE16LE(&uint16_toutbuffer[1], cable_data[0]);
    WRITE32LE(&intoutbuffer[1], transfer_start_time_from_master);

    if (lanlink.numslaves == 1) {
        outbuffer[0] = 8;
        CheckSendResult(tcpsocket[1].send(outbuffer, 8));
    } else if (lanlink.numslaves == 2) {
        outbuffer[0] = 10;
        WRITE16LE(&uint16_toutbuffer[4], cable_data[2]);
        CheckSendResult(tcpsocket[1].send(outbuffer, 10));
        WRITE16LE(&uint16_toutbuffer[4], cable_data[1]);
        CheckSendResult(tcpsocket[2].send(outbuffer, 10));
    } else {
        outbuffer[0] = 12;
        WRITE16LE(&uint16_toutbuffer[4], cable_data[2]);
        WRITE16LE(&uint16_toutbuffer[5], cable_data[3]);
        CheckSendResult(tcpsocket[1].send(outbuffer, 12));
        WRITE16LE(&uint16_toutbuffer[4], cable_data[1]);
        CheckSendResult(tcpsocket[2].send(outbuffer, 12));
        WRITE16LE(&uint16_toutbuffer[5], cable_data[2]);
        CheckSendResult(tcpsocket[3].send(outbuffer, 12));
    }
}

// Receive data from all slaves to master. Returns true only when every
// slave's frame arrived intact this call; on a timeout the caller retries
// on its next update tick (the stream is still frame-aligned then).
bool CableServer::Recv(void)
{
    fdset.clear();

    for (int i = 0; i < lanlink.numslaves; i++)
        fdset.add(tcpsocket[i + 1]);

    sf::Clock budget;
    if (fdset.wait(sf::milliseconds(50)) == 0)
        return false;

    for (int i = 0; i < lanlink.numslaves; i++) {
        RecvResult r = ReceiveExact(tcpsocket[i + 1], fdset, inbuffer, 1, budget, 50);
        if (r == RecvResult::Timeout)
            return false; // clean frame boundary; safe to retry
        bool dropped = (r == RecvResult::Dropped);
        if (!dropped) {
            // A slave frame is always 4 bytes (CableClient::Send), as is
            // the goodbye frame; any other length byte is a protocol
            // desync. A timeout mid-frame would leave the stream
            // misaligned, so it counts as a drop too.
            dropped = inbuffer[0] != 4
                || ReceiveExact(tcpsocket[i + 1], fdset, inbuffer + 1, 3, budget, 50) != RecvResult::Ok;
        }
        if (dropped || (uint8_t)inbuffer[1] == kLinkGoodbyeByte) {
            char message[30];
            snprintf(message, sizeof(message), _("Player %d disconnected."), i + 2);
            systemScreenMessage(message);
            // Tell the remaining slaves and shut the session down. No
            // reply round-trip: a dead peer never answers, and the old
            // blocking wait for one could hang the emulator here.
            for (int j = 1; j <= lanlink.numslaves; j++) {
                if (j != i + 1)
                    SendGoodbye(tcpsocket[j]);
                tcpsocket[j].disconnect();
            }
            RequestLinkClose();
            return false;
        }
        if ((uint8_t)inbuffer[1] == kLinkGpByte) {
            // A GP state frame racing the transfer start; cache it and
            // re-read this slave's actual data frame (same 50 ms budget).
            gp_peer_rcnt[i + 1] = READ16LE(&uint16_tinbuffer[1]);
            i--;
            continue;
        }
        cable_data[i + 1] = READ16LE(&uint16_tinbuffer[1]);
    }
    return true;
}

void CableServer::SendGB(void)
{
    if (lanlink.numslaves == 1) {
        CheckSendResult(tcpsocket[1].send(&cable_gb_data[0], 1));
    }
}

// Receive one byte from the slave into cable_gb_data[1].
// Waits in <= 50 ms slices so a deferred close request is honored promptly
// even with a long user-configured timeout.
bool CableServer::RecvGB(int timeout_ms)
{
    if (lanlink.numslaves != 1)
        return false;

    sf::Clock budget;
    do {
        if (LinkIsClosing())
            return false;

        fdset.clear();
        fdset.add(tcpsocket[1]);

        int slice = timeout_ms - (int)budget.getElapsedTime().asMilliseconds();
        if (slice > 50)
            slice = 50;
        // sf::Time::Zero means "wait forever" to the selector, so always
        // give the poll case a real (minimal) timeout
        if (slice < 1)
            slice = 1;

        if (fdset.wait(sf::milliseconds(slice))) {
            uint8_t recv_byte = 0;
            size_t nr = 0;
            sf::Socket::Status status = tcpsocket[1].receive(&recv_byte, 1, nr);

            if (status == sf::Socket::Status::Disconnected || status == sf::Socket::Status::Error) {
                systemScreenMessage(_("Player 2 disconnected."));
                tcpsocket[1].disconnect();
                RequestLinkClose();
                return false;
            }

            if (status == sf::Socket::Status::Done && nr > 0) {
                cable_gb_data[1] = recv_byte;
                return true;
            }
        }
    } while ((int)budget.getElapsedTime().asMilliseconds() < timeout_ms);
    return false;
}

// Master-side transfer: send our byte and wait for the slave's reply.
// Late replies from timed-out exchanges are drained first so the byte
// stream stays aligned with the transfers.
bool CableServer::ExchangeGB(uint8_t b, int timeout_ms)
{
    while (gb_pending > 0 && RecvGB(0))
        gb_pending--;

    cable_gb_data[0] = b;
    SendGB();

    if (LinkIsClosing())
        return false;

    if (RecvGB(timeout_ms))
        return true;

    if (!LinkIsClosing())
        gb_pending++;
    return false;
}

// Client
CableClient::CableClient(void)
{
    intinbuffer = (int32_t*)inbuffer;
    uint16_tinbuffer = (uint16_t*)inbuffer;
    intoutbuffer = (int32_t*)outbuffer;
    uint16_toutbuffer = (uint16_t*)outbuffer;
    transferring = false;
    gb_pending = 0;
    // Never let uninitialized stack/heap bytes reach the wire.
    memset(inbuffer, 0, sizeof(inbuffer));
    memset(outbuffer, 0, sizeof(outbuffer));
    return;
}

// Idle-time poll for the master starting a transfer (or leaving). The
// socket is blocking, so probe with a short selector wait first; an idle
// frame costs ~1 ms rather than parking the emulator thread in recv().
void CableClient::CheckConn(void)
{
    fdset.clear();
    fdset.add(lanlink.tcpsocket);
    // Minimal-timeout probe (sf::Time::Zero would wait forever). This runs
    // both from the frontend's per-frame CheckLinkConnection and from the
    // tick-throttled idle poll in UpdateCableSocket, so it must cost
    // microseconds, not milliseconds, when nothing is pending.
    if (fdset.wait(sf::microseconds(1)) == 0)
        return;

    sf::Clock budget;
    RecvResult r = ReceiveExact(lanlink.tcpsocket, fdset, inbuffer, 1, budget, 50);
    if (r == RecvResult::Timeout)
        return;
    bool dropped = (r == RecvResult::Dropped);
    if (!dropped) {
        // Master frames are 8/10/12 bytes for 1/2/3 slaves; goodbye is 4.
        uint8_t len = (uint8_t)inbuffer[0];
        dropped = (len != 4 && len != 8 && len != 10 && len != 12)
            || ReceiveExact(lanlink.tcpsocket, fdset, inbuffer + 1, len - 1, budget, 50) != RecvResult::Ok;
    }
    if (dropped || (uint8_t)inbuffer[1] == kLinkGoodbyeByte) {
        systemScreenMessage(_("Server disconnected."));
        RequestLinkClose();
        return;
    }
    if ((uint8_t)inbuffer[0] == 4 && (uint8_t)inbuffer[1] == kLinkGpByte) {
        // A GP state frame, not a transfer start; just cache it.
        gp_peer_rcnt[0] = READ16LE(&uint16_tinbuffer[1]);
        return;
    }
    transferring = true;
    transfer_start_time_from_master = 0;
    cable_data[0] = READ16LE(&uint16_tinbuffer[1]);
    tspeed = inbuffer[1] & 3;
    CableTrace("sock[%d] CheckConn transfer detected data=%04x speed=%d",
        linkid, cable_data[0], tspeed);
    for (int i = 1, bytes = 4; i <= lanlink.numslaves; i++)
        if (i != linkid) {
            cable_data[i] = READ16LE(&uint16_tinbuffer[bytes]);
            bytes++;
        }
    return;
}

// Receive one byte from the server into cable_gb_data[0].
// Waits in <= 50 ms slices so a deferred close request is honored promptly
// even with a long user-configured timeout.
bool CableClient::RecvGB(int timeout_ms)
{
    sf::Clock budget;
    do {
        if (LinkIsClosing())
            return false;

        fdset.clear();
        fdset.add(lanlink.tcpsocket);

        int slice = timeout_ms - (int)budget.getElapsedTime().asMilliseconds();
        if (slice > 50)
            slice = 50;
        // sf::Time::Zero means "wait forever" to the selector, so always
        // give the poll case a real (minimal) timeout
        if (slice < 1)
            slice = 1;

        if (fdset.wait(sf::milliseconds(slice))) {
            uint8_t recv_byte = 0;
            size_t nr = 0;
            sf::Socket::Status status = lanlink.tcpsocket.receive(&recv_byte, 1, nr);

            if (status == sf::Socket::Status::Disconnected || status == sf::Socket::Status::Error) {
                systemScreenMessage(_("Server disconnected."));
                RequestLinkClose();
                return false;
            }

            if (status == sf::Socket::Status::Done && nr > 0) {
                cable_gb_data[0] = recv_byte;
                return true;
            }
        }
    } while ((int)budget.getElapsedTime().asMilliseconds() < timeout_ms);
    return false;
}

void CableClient::SendGB()
{
    CheckSendResult(lanlink.tcpsocket.send(&cable_gb_data[1], 1));
}

// Master-side transfer: send our byte and wait for the server's reply.
// Late replies from timed-out exchanges are drained first so the byte
// stream stays aligned with the transfers.
bool CableClient::ExchangeGB(uint8_t b, int timeout_ms)
{
    while (gb_pending > 0 && RecvGB(0))
        gb_pending--;

    cable_gb_data[1] = b;
    SendGB();

    if (LinkIsClosing())
        return false;

    if (RecvGB(timeout_ms))
        return true;

    if (!LinkIsClosing())
        gb_pending++;
    return false;
}

// Receive the master's frame. Returns true only when a full frame arrived;
// on a timeout the caller retries next tick (the stream is frame-aligned),
// and `transferring` is deliberately left alone -- clearing it re-armed
// CheckConn, which then re-parsed this same frame with the clock field
// forced to zero.
bool CableClient::Recv(void)
{
    fdset.clear();
    fdset.add(lanlink.tcpsocket);
    sf::Clock budget;
    if (fdset.wait(sf::milliseconds(50)) == 0)
        return false;

    RecvResult r = ReceiveExact(lanlink.tcpsocket, fdset, inbuffer, 1, budget, 50);
    if (r == RecvResult::Timeout)
        return false; // clean frame boundary; safe to retry
    bool dropped = (r == RecvResult::Dropped);
    if (!dropped) {
        // Master frames are 8/10/12 bytes for 1/2/3 slaves; goodbye is 4.
        // A timeout mid-frame would leave the stream misaligned, so it
        // counts as a drop too.
        uint8_t len = (uint8_t)inbuffer[0];
        dropped = (len != 4 && len != 8 && len != 10 && len != 12)
            || ReceiveExact(lanlink.tcpsocket, fdset, inbuffer + 1, len - 1, budget, 50) != RecvResult::Ok;
    }
    if (dropped || (uint8_t)inbuffer[1] == kLinkGoodbyeByte) {
        systemScreenMessage(_("Server disconnected."));
        RequestLinkClose();
        return false;
    }
    if ((uint8_t)inbuffer[0] == 4 && (uint8_t)inbuffer[1] == kLinkGpByte) {
        // A GP state frame, not our transfer result; cache it and retry on
        // the next update tick (the stream is still frame-aligned).
        gp_peer_rcnt[0] = READ16LE(&uint16_tinbuffer[1]);
        return false;
    }
    tspeed = inbuffer[1] & 3;
    cable_data[0] = READ16LE(&uint16_tinbuffer[1]);
    transfer_start_time_from_master = (int32_t)READ32LE(&intinbuffer[1]);
    for (int i = 1, bytes = 4; i < lanlink.numslaves + 1; i++) {
        if (i != linkid) {
            cable_data[i] = READ16LE(&uint16_tinbuffer[bytes]);
            bytes++;
        }
    }
    return true;
}

void CableClient::Send()
{
    outbuffer[0] = 4;
    outbuffer[1] = (char)(linkid << 2);
    WRITE16LE(&uint16_toutbuffer[1], cable_data[linkid]);
    CheckSendResult(lanlink.tcpsocket.send(outbuffer, 4));
    return;
}

// The OS error left behind by the last failed socket call. The bundled SFML
// preserves it across its own err() logging, so this is reliable right after
// a Status::Error return.
static int LastSocketError()
{
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

static const char* SocketErrorString(int error)
{
#if defined(_WIN32)
    static char buf[256];
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                       (DWORD)error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                       sizeof(buf), NULL) == 0) {
        snprintf(buf, sizeof(buf), "socket error %d", error);
    }
    return buf;
#else
    return strerror(error);
#endif
}

static bool IsAddrInUse(int error)
{
#if defined(_WIN32)
    return error == WSAEADDRINUSE;
#else
    return error == EADDRINUSE;
#endif
}

static bool IsAddrNotAvail(int error)
{
#if defined(_WIN32)
    return error == WSAEADDRNOTAVAIL;
#else
    return error == EADDRNOTAVAIL;
#endif
}

static ConnectionState InitSocket()
{
    linkid = 0;

    // Start every connection attempt from a clean transfer state so a
    // previous (possibly aborted) session can't leak stale flags into the
    // new one.
    link_close_pending = false;
    transfer_direction = 0;
    missed_recv_ms = 0;
    lc.transferring = false;
    rfu_client.transferring = false;
    GpResetState();

    for (int i = 0; i < 4; i++) {
        cable_data[i] = 0xffff;
    }

    for (int i = 0; i < 4; i++) {
        cable_gb_data[i] = 0xff;
    }

    ls.gb_pending = 0;
    lc.gb_pending = 0;

    if (lanlink.server) {
        lanlink.connectedSlaves = 0;
        // should probably use GetPublicAddress()
        //sid->ShowServerIP(sf::IpAddress::getLocalAddress());

        // too bad Listen() doesn't take an address as well
        // then again, old code used INADDR_ANY anyway
        sf::IpAddress bind_ip{0};

        if (IP_LINK_BIND_ADDRESS != "*") {
            auto resolved = sf::IpAddress::resolve(IP_LINK_BIND_ADDRESS);
            if (resolved) {
                bind_ip = resolved.value();
            } else {
                systemMessage(0,
                              N_("Could not resolve link server bind address \"%s\".\nUse \"*\" to listen on all interfaces."),
                              IP_LINK_BIND_ADDRESS.c_str());
                return LINK_ERROR;
            }
        }

        if (lanlink.tcplistener.listen(IP_LINK_PORT, bind_ip) == sf::Socket::Status::Error) {
            int error = LastSocketError();

            // A non-local bind address, typically the peer's IP typed into
            // the Server IP field: fall back to listening on all interfaces.
            if (IsAddrNotAvail(error) && bind_ip.toInteger() != 0) {
                systemMessage(0,
                              N_("Bind address \"%s\" is not a local address; listening on all interfaces instead."),
                              IP_LINK_BIND_ADDRESS.c_str());

                if (lanlink.tcplistener.listen(IP_LINK_PORT, sf::IpAddress{0}) != sf::Socket::Status::Error) {
                    return LINK_NEEDS_UPDATE;
                }

                error = LastSocketError();
            }

            if (IsAddrInUse(error)) {
                systemMessage(0,
                              N_("Port %d is already in use by another program (or another emulator instance acting as server).\nClose it or change the link port."),
                              (int)IP_LINK_PORT);
            } else {
                systemMessage(0, N_("Failed to start link server on port %d: %s"),
                              (int)IP_LINK_PORT, SocketErrorString(error));
            }

            return LINK_ERROR;
        } else {
            return LINK_NEEDS_UPDATE;
        }
    } else {
        lc.serverport = IP_LINK_PORT;

        // Non-blocking connect; ConnectUpdateSocket polls it to completion
        // under a wall-clock deadline, keeping the modal progress dialog
        // responsive and cancellable throughout.
        lanlink.tcpsocket.setBlocking(false);
        sf::Socket::Status status = lanlink.tcpsocket.connect(lc.serveraddr, lc.serverport);

        connect_clock.restart();
        client_handshake_connected = false;
        connect_attempt_failed = (status == sf::Socket::Status::Disconnected);
        last_connect_attempt_ms = 0;

        // Only a synchronous failure is fatal here. NotReady means the
        // connect is in progress, and even Disconnected only means this
        // attempt failed fast (server not up yet); the poll loop retries.
        if (status == sf::Socket::Status::Error) {
            systemMessage(0, N_("Could not initiate connection to %s port %d: %s"),
                          lc.serveraddr.toString().c_str(), (int)lc.serverport,
                          SocketErrorString(LastSocketError()));
            return LINK_ERROR;
        } else {
            return LINK_NEEDS_UPDATE;
        }
    }
}

static ConnectionState ConnectUpdateSocket(char* const message, size_t size)
{
    ConnectionState newState = LINK_NEEDS_UPDATE;

    if (lanlink.server) {
        sf::SocketSelector fdset;
        fdset.add(lanlink.tcplistener);

        if (fdset.wait(sf::milliseconds(150))) {
            uint16_t nextSlave = (uint16_t)(lanlink.connectedSlaves + 1);

            sf::Socket::Status st = lanlink.tcplistener.accept(ls.tcpsocket[nextSlave]);

            // Anything but a completed accept just means "no new client
            // this tick"; treating it as fatal (or counting a dead socket
            // as a connected slave) broke the whole session.
            if (st == sf::Socket::Status::Done) {
                LinkSetNoDelay(ls.tcpsocket[nextSlave]);
                sf::Packet packet;
                packet << nextSlave << lanlink.numslaves;

                if (ls.tcpsocket[nextSlave].send(packet) == sf::Socket::Status::Done) {
                    snprintf(message, size, N_("Player %d connected"), nextSlave);
                    lanlink.connectedSlaves++;
                } else {
                    ls.tcpsocket[nextSlave].disconnect();
                }
            }
        }

        if (lanlink.numslaves == lanlink.connectedSlaves) {
            for (int i = 1; i <= lanlink.numslaves; i++) {
                sf::Packet packet;
                packet << true;

                (void)ls.tcpsocket[i].send(packet);
            }

            snprintf(message, size, N_("All players connected"));
            newState = LINK_OK;
        }
    } else {
        if (!client_handshake_connected) {
            // getpeername() succeeds only once the TCP connection is
            // ESTABLISHED. Probing with receive() before that is not
            // portable: during SYN_SENT macOS fails recv() with ENOTCONN,
            // which SFML maps to Disconnected -- the old code treated that
            // as a fatal network error on the very first poll tick.
            if (lanlink.tcpsocket.getRemoteAddress().has_value()) {
                client_handshake_connected = true;
                LinkSetNoDelay(lanlink.tcpsocket);
            } else {
                // No deadline here: the user is watching a cancellable
                // progress dialog, so how long to keep trying is their
                // call, not a timer's.
                const int32_t elapsed = (int32_t)connect_clock.getElapsedTime().asMilliseconds();

                // A refused attempt (server not listening yet) reports its
                // error exactly once, on the next socket op (ECONNREFUSED
                // -> Error; later probes degrade to ENOTCONN ->
                // Disconnected, which also means "still connecting" on
                // macOS). Latch the failure and retry the connect,
                // throttled, so starting the client first works.
                char probe;
                size_t nr = 0;
                if (lanlink.tcpsocket.receive(&probe, 1, nr) == sf::Socket::Status::Error)
                    connect_attempt_failed = true;
                if (connect_attempt_failed && elapsed - last_connect_attempt_ms >= kConnectRetryMs) {
                    last_connect_attempt_ms = elapsed;
                    connect_attempt_failed = false;
                    (void)lanlink.tcpsocket.connect(lc.serveraddr, lc.serverport);
                }

                snprintf(message, size, N_("Connecting to server..."));
                return LINK_NEEDS_UPDATE;
            }
        }

        sf::Packet packet;
        sf::Socket::Status status = lanlink.tcpsocket.receive(packet);

        if (status == sf::Socket::Status::Error || status == sf::Socket::Status::Disconnected) {
            snprintf(message, size, N_("Network error."));
            newState = LINK_ERROR;
        } else if (status == sf::Socket::Status::Done) {

            if (linkid == 0) {
                uint16_t receivedId, receivedSlaves;
                packet >> receivedId >> receivedSlaves;

                if (packet) {
                    linkid = receivedId;
                    lanlink.numslaves = receivedSlaves;

                    snprintf(message, size, N_("Connected as #%d, Waiting for %d players to join"),
                        linkid + 1, lanlink.numslaves - linkid);
                }
            } else {
                bool gameReady;
                packet >> gameReady;

                if (packet && gameReady) {
                    // Established sockets run blocking from here on: every
                    // receive is selector-guarded (see ReceiveExact), and a
                    // blocking send can't return Partial. Left non-blocking,
                    // the transfer loops spun on NotReady at 100% CPU.
                    lanlink.tcpsocket.setBlocking(true);
                    newState = LINK_OK;
                    snprintf(message, size, N_("All players joined."));
                }
            }
        }
    }

    return newState;
}

void StartCableSocket(uint16_t value)
{
    switch (GetSIOMode(value, READ16LE(&g_ioMem[COMM_RCNT]))) {
    case MULTIPLAYER: {
        bool start = (value & 0x80) && !linkid && !transfer_direction;
        // clear start, seqno, si (RO on slave, start = pulse on master)
        value &= 0xff4b;
        // get current si.  This way, on slaves, it is low during xfer
        if (linkid) {
            if (!transfer_direction)
                value |= 4;
            else
                value |= READ16LE(&g_ioMem[COMM_SIOCNT]) & 4;
        }
        if (start) {
            // linktime overflows negative after ~128 s of emulated idle; a
            // negative published clock would still work (the slave's gate
            // passes trivially) but destroys the pacing semantics, so
            // normalize before publishing.
            if (linktime < 0)
                linktime = 0;
            cable_data[0] = READ16LE(&g_ioMem[COMM_SIODATA8]);
            transfer_start_time_from_master = linktime;
            tspeed = value & 3;
            CableTrace("sock[%d] master start data=%04x speed=%d start_time=%d",
                linkid, cable_data[0], tspeed, transfer_start_time_from_master);
            (void)ls.Send();
            transfer_direction = RECEIVING;
            linktime = 0;
            UPDATE_REG(COMM_SIOMULTI0, cable_data[0]);
            UPDATE_REG(COMM_SIOMULTI1, 0xffff);
            WRITE32LE(&g_ioMem[COMM_SIOMULTI2], 0xffffffff);
            value &= ~0x40;
        }
        value |= (transfer_direction ? 1 : 0) << 7;
        value |= (linkid && !transfer_direction) ? 0x0c : 0x08; // set SD (high), SI (low on master)
        value |= linkid << 4; // set seq
        UPDATE_REG(COMM_SIOCNT, value);
        if (linkid)
            // SC low -> transfer in progress
            // not sure why SO is low
            UPDATE_REG(COMM_RCNT, transfer_direction ? 6 : 7);
        else
            // SI is always low on master
            // SO, SC always low during transfer
            // not sure why SO low otherwise
            UPDATE_REG(COMM_RCNT, transfer_direction ? 2 : 3);
        break;
    }
    case NORMAL8:
    case NORMAL32:
    case UART:
    default:
        UPDATE_REG(COMM_SIOCNT, value);
        break;
    }
}

// Drain any frames pending on one peer socket while we are in GP mode. GP
// state frames update the cache; a goodbye or dropped connection tears the
// session down (mirroring CableServer/CableClient::Recv); anything else is
// stale multiplayer traffic from before a mode switch -- parsed to its full
// length and discarded so the stream stays frame-aligned. peer_index is the
// peer's player id (server caches slave i at [i], the client caches the
// server at [0]). Returns false when the link is going down.
static bool GpSocketDrainOne(sf::TcpSocket& sock, int peer_index)
{
    sf::SocketSelector sel;
    for (;;) {
        sel.clear();
        sel.add(sock);
        // A real (minimal) timeout: sf::Time::Zero means wait forever.
        if (!sel.wait(sf::microseconds(1)))
            return true;

        sf::Clock budget;
        char buf[16];
        RecvResult r = ReceiveExact(sock, sel, buf, 1, budget, 50);
        if (r == RecvResult::Timeout)
            return true; // clean frame boundary; drained again next poll
        bool dropped = (r == RecvResult::Dropped);
        if (!dropped) {
            const uint8_t len = (uint8_t)buf[0];
            dropped = (len != 4 && len != 8 && len != 10 && len != 12)
                || ReceiveExact(sock, sel, buf + 1, len - 1, budget, 50) != RecvResult::Ok;
        }
        if (dropped || (uint8_t)buf[1] == kLinkGoodbyeByte) {
            if (linkid) {
                systemScreenMessage(_("Server disconnected."));
            } else {
                char message[30];
                snprintf(message, sizeof(message), _("Player %d disconnected."), peer_index + 1);
                systemScreenMessage(message);
                for (int j = 1; j <= lanlink.numslaves; j++) {
                    if (j != peer_index)
                        SendGoodbye(ls.tcpsocket[j]);
                    ls.tcpsocket[j].disconnect();
                }
            }
            RequestLinkClose();
            return false;
        }
        if ((uint8_t)buf[1] == kLinkGpByte)
            gp_peer_rcnt[peer_index] = (uint16_t)((uint8_t)buf[2] | ((uint16_t)(uint8_t)buf[3] << 8));
        // else: stale multiplayer traffic from a mode transition; discarded
    }
}

// Per-tick GP servicing for the socket cable driver (local RCNT is in GP
// mode). 2-player-exact; the server does not relay slave<->slave GP state.
static void GpSocketUpdate(int ticks)
{
    // GP mode has no multiplayer transfer in flight; clear the machinery so
    // a half-finished cycle from before the mode switch can't resume, and
    // never let GP quiescence accumulate toward the link-death timeout.
    transfer_direction = SENDING;
    lc.transferring = false;
    missed_recv_ms = 0;

    // State committed before the connection existed (or while the connect
    // dialog was still up) was never transmitted; publish it once.
    if (!gp_socket_sent_initial) {
        gp_socket_sent_initial = true;
        GpSocketRcntWritten(READ16LE(&g_ioMem[COMM_RCNT]));
    }

    // Throttle the receive poll: LinkUpdate runs every instruction batch,
    // and even a minimal selector wait per call would swamp the emulator.
    gp_poll_ticks += ticks;
    if (gp_poll_ticks >= (int)(TICKS_PER_FRAME / 4)) {
        gp_poll_ticks = 0;
        if (linkid) {
            if (!GpSocketDrainOne(lanlink.tcpsocket, 0))
                return;
        } else {
            for (int i = 1; i <= lanlink.numslaves; i++)
                if (!GpSocketDrainOne(ls.tcpsocket[i], i))
                    return;
        }
    }

    GpCommitMerged(GpMergeInputs(READ16LE(&g_ioMem[COMM_RCNT]),
                                 linkid ? gp_peer_rcnt : gp_peer_rcnt + 1,
                                 linkid ? 1 : lanlink.numslaves));
}

static void UpdateCableSocket(int ticks)
{
    if ((READ16LE(&g_ioMem[COMM_RCNT]) >> 14) == 2) {
        GpSocketUpdate(ticks);
        return;
    }

    if (linkid && transfer_direction == SENDING) {
        // linktime overflows negative after ~128 s of emulated time without
        // a transfer; a negative clock deadlocks the send gate below until
        // it wraps back positive (~128 s later). Pokémon reaches the Cable
        // Club minutes into a session, so this window is hit in normal
        // play, not just in pathological cases.
        if (linktime < 0) {
            CableTrace("sock[%d] idle-overflow clamp (linktime<0)", linkid);
            linktime = 0;
        }

        // Between transfers, poll for the master's next start frame here
        // rather than only in the frontend's once-per-frame
        // CheckLinkConnection: a master chaining several transfers per
        // frame off its timer interrupt (Pokémon clocks its whole command
        // block back-to-back) outruns a 60 Hz poll unrecoverably.
        if (!lc.transferring) {
            cable_poll_ticks += ticks;
            if (cable_poll_ticks >= (int)(TICKS_PER_FRAME / 64)) {
                cable_poll_ticks = 0;
                lc.CheckConn();
            }
            if (!lc.transferring) {
                // Between transfers, hold a slave that ran several frames
                // ahead of the master's transfer stream (see
                // kMaxLinkClockAheadTicks).
                if (linktime > kMaxLinkClockAheadTicks)
                    (void)LinkAheadThrottleStep();
                return;
            }
            ahead_throttle_budget_us = kAheadThrottleBudgetUs;
        }
    }

    if (linkid && transfer_direction == SENDING && lc.transferring) {
        // Pace our reply to the master's inter-transfer gap, but never
        // wait out a clock desync bigger than the cap: past it the master
        // is only serving dead air waiting for our reply.
        if (transfer_start_time_from_master - linktime > kMaxLinkClockLagTicks)
            linktime = transfer_start_time_from_master;
        if (linktime < transfer_start_time_from_master)
            return;
        cable_data[linkid] = READ16LE(&g_ioMem[COMM_SIODATA8]);

        CableTrace("sock[%d] slave latch data=%04x linktime=%d start_time=%d",
            linkid, cable_data[linkid], linktime, transfer_start_time_from_master);
        (void)lc.Send();
        UPDATE_REG(COMM_SIODATA32_L, cable_data[0]);
        UPDATE_REG(COMM_SIOCNT, READ16LE(&g_ioMem[COMM_SIOCNT]) | 0x80);
        transfer_direction = RECEIVING;
        linktime = 0;
    }

    if (linkid && lanlink.numslaves == 1 && transfer_direction == RECEIVING
        && linktime >= trtimeend[0][tspeed]) {
        // 2-player slave: nothing more travels on the wire for this
        // transfer -- SIOMULTI0 was committed at send time from the
        // master's start frame and SIOMULTI1 is our own word -- so complete
        // at the hardware transfer time and go idle. The old code parked in
        // Recv() until the *next* start frame arrived, which delayed the
        // last serial IRQ of every burst by a whole frame and turned any
        // master-side lull into 50 ms emulation stalls that accumulated
        // missed_recv_ms into a bogus "Link timeout." The next start frame
        // is picked up by the tick-throttled poll above. 3-4 player
        // sessions keep the pipelined path below: the other slaves' words
        // genuinely travel in the master's next frame.
        if (READ16LE(&g_ioMem[COMM_SIOCNT]) & 0x4000)
            CPURaiseSioIRQ();

        UPDATE_REG(COMM_SIOCNT, (READ16LE(&g_ioMem[COMM_SIOCNT]) & 0xff0f) | (linkid << 4));
        transfer_direction = SENDING;
        linktime -= trtimeend[0][tspeed];

        UPDATE_REG(COMM_SIOMULTI1, cable_data[1]);
        // Absent slots read back 0xffff on hardware.
        UPDATE_REG(COMM_SIOMULTI2, 0xffff);
        UPDATE_REG(COMM_SIOMULTI3, 0xffff);
        lc.transferring = false;
        missed_recv_ms = 0;
        CableTrace("sock[%d] commit M0=%04x M1=%04x M2=ffff M3=ffff irq=%d",
            linkid, READ16LE(&g_ioMem[COMM_SIOMULTI0]), cable_data[1],
            (READ16LE(&g_ioMem[COMM_SIOCNT]) & 0x4000) ? 1 : 0);
        return;
    }

    if (transfer_direction == RECEIVING && linktime >= trtimeend[lanlink.numslaves - 1][tspeed]) {
        // Commit the transfer (IRQ, SIOCNT, SIOMULTI) only when the peer
        // frames actually arrived. The old code committed unconditionally,
        // publishing the *previous* transfer's cable_data as this one's
        // result on any 50 ms hiccup -- a silent, undetectable desync.
        // On a miss, stay in RECEIVING and retry next update, forever: a
        // real cable never times out, and a stalled peer (occluded or
        // dragged window, coalesced timers, an Android app backgrounded)
        // only freezes progress -- the stream is still frame-aligned when
        // it comes back. A peer that actually DIED is caught by the TCP
        // layer instead (goodbye frame or Dropped status in
        // Recv/CheckSendResult), so no wall-clock budget is needed on any
        // platform.
        bool received;
        if (linkid) {
            lc.transferring = true;
            received = lc.Recv();
        } else {
            received = ls.Recv(); // Receive data from all of the slaves
        }

        if (!received) {
            if (LinkIsClosing())
                return; // drop already flagged; CloseLink runs after update
            missed_recv_ms += 50;
            CableTrace("sock[%d] recv miss (missed_recv_ms=%d)", linkid,
                missed_recv_ms);
            return;
        }
        missed_recv_ms = 0;

        if (READ16LE(&g_ioMem[COMM_SIOCNT]) & 0x4000)
            CPURaiseSioIRQ();

        UPDATE_REG(COMM_SIOCNT, (READ16LE(&g_ioMem[COMM_SIOCNT]) & 0xff0f) | (linkid << 4));
        transfer_direction = SENDING;
        linktime -= trtimeend[lanlink.numslaves - 1][tspeed];

        UPDATE_REG(COMM_SIOMULTI1, cable_data[1]);
        UPDATE_REG(COMM_SIOMULTI2, cable_data[2]);
        UPDATE_REG(COMM_SIOMULTI3, cable_data[3]);
        CableTrace("sock[%d] commit M0=%04x M1=%04x M2=%04x M3=%04x irq=%d",
            linkid, READ16LE(&g_ioMem[COMM_SIOMULTI0]), cable_data[1],
            cable_data[2], cable_data[3],
            (READ16LE(&g_ioMem[COMM_SIOCNT]) & 0x4000) ? 1 : 0);
    }
}

static void CloseSocket()
{
    // The GB byte stream has no framing, so the -32 goodbye marker used by
    // the GBA protocol would be read back as transfer data; GB peers detect
    // the plain TCP disconnect instead.
    bool send_goodbye = GetLinkMode() != LINK_GAMEBOY_SOCKET;

    if (linkid) {
        if (send_goodbye)
            SendGoodbye(lanlink.tcpsocket);
    } else {
        for (int i = 1; i <= lanlink.numslaves; i++) {
            if (send_goodbye)
                SendGoodbye(ls.tcpsocket[i]);
            ls.tcpsocket[i].disconnect();
        }
    }
    lanlink.tcpsocket.disconnect();

    // Free the listening port and any RFU per-slave sockets. These used to
    // leak: the listener kept the link port bound for the whole process
    // lifetime, and RFU server sessions leaked their slave sockets.
    lanlink.tcplistener.close();
    for (int i = 1; i <= 4; i++)
        rfu_server.tcpsocket[i].disconnect();
    lanlink.connectedSlaves = 0;
    GpResetState();
}

// call this to clean up crashed program's shared state
// or to use TCP on same machine (for testing)
// this may be necessary under MSW as well, but I wouldn't know how
void CleanLocalLink()
{
#if defined(__ANDROID__)
    // Everything lives in one file, so dropping it clears the whole session.
    // Only do that when no instance is still attached, otherwise the peers
    // that remain would keep running against a mapping nobody can find.
    const std::string& path = AndroidLinkShmPath();
    if (path.empty())
        return;
    int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return;
    if (flock(fd, LOCK_EX | LOCK_NB) == 0)
        AndroidLinkShmUnlinkFiles();
    close(fd);
#elif !(defined __WIN32__ || defined _WIN32)
    // Crash recovery is automatic these days -- InitIPC sweeps a dead
    // session's leftovers once nobody holds the liveness lock -- so this
    // manual escape hatch only sweeps when it can prove no instance is
    // attached; unguarded, it used to nuke a *live* session's IPC.
    int fd = open(LinkLockFilePath(".flock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    if (fd < 0)
        return;
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        shm_unlink(LinkShmName().c_str());
        for (int i = 0; i < 4; i++) {
            sem_unlink(LinkSemName(i).c_str());
            sem_unlink(LinkDoorbellName(i).c_str());
        }
        sem_unlink(LinkLockSemName().c_str());
    }
    close(fd);
#endif
}

static ConnectionState JoyBusConnect()
{
    delete dol;
    dol = NULL;

    dol = new GBASockClient(joybusHostAddr);
    if (dol) {
        return LINK_OK;
    } else {
        return LINK_ERROR;
    }
}

static void JoyBusUpdate(int ticks)
{
    lastjoybusupdate += ticks;
    lastcommand += ticks;

    bool joybus_activated = ((READ16LE(&g_ioMem[COMM_RCNT])) >> 14) == 3;
    gba_joybus_active = dol && gba_joybus_enabled && joybus_activated;

    if ((lastjoybusupdate > nextjoybusupdate)) {
        if (!joybus_activated) {
            if (dol && booted) {
                JoyBusShutdown();
            }

            lastjoybusupdate = 0;
            nextjoybusupdate = 0;
            lastcommand = 0;
            return;
        }

        if (!dol) {
            booted = false;
            JoyBusConnect();
        }

        dol->ReceiveClock(false);

        if (dol->IsDisconnected()) {
            JoyBusShutdown();
            nextjoybusupdate = TICKS_PER_SECOND * 2; // try to connect after 2 seconds
            lastjoybusupdate = 0;
            lastcommand = 0;
            return;
        }

        dol->ClockSync(lastjoybusupdate);

        char data[5] = { 0x10, 0, 0, 0, 0 }; // init with invalid cmd
        std::vector<char> resp;
        uint8_t cmd = 0x10;

        if (lastcommand > (TICKS_PER_FRAME * 4)) {
            cmd = dol->ReceiveCmd(data, true);
        } else {
            cmd = dol->ReceiveCmd(data, false);
        }

        switch (cmd) {
        case JOY_CMD_RESET:
            UPDATE_REG(COMM_JOYCNT, READ16LE(&g_ioMem[COMM_JOYCNT]) | JOYCNT_RESET);
            resp.push_back(0x00); // GBA device ID
            resp.push_back(0x04);
            nextjoybusupdate = TICKS_PER_SECOND / BYTES_PER_SECOND;
            break;

        case JOY_CMD_STATUS:
            resp.push_back(0x00); // GBA device ID
            resp.push_back(0x04);

            nextjoybusupdate = TICKS_PER_SECOND / BYTES_PER_SECOND;
            break;

        case JOY_CMD_READ:
            resp.push_back((uint8_t)(READ16LE(&g_ioMem[COMM_JOY_TRANS_L]) & 0xff));
            resp.push_back((uint8_t)(READ16LE(&g_ioMem[COMM_JOY_TRANS_L]) >> 8));
            resp.push_back((uint8_t)(READ16LE(&g_ioMem[COMM_JOY_TRANS_H]) & 0xff));
            resp.push_back((uint8_t)(READ16LE(&g_ioMem[COMM_JOY_TRANS_H]) >> 8));

            UPDATE_REG(COMM_JOYCNT, READ16LE(&g_ioMem[COMM_JOYCNT]) | JOYCNT_SEND_COMPLETE);
            nextjoybusupdate = TICKS_PER_SECOND / BYTES_PER_SECOND;
            booted = true;
            break;

        case JOY_CMD_WRITE:
            UPDATE_REG(COMM_JOY_RECV_L, (uint16_t)((uint16_t)data[2] << 8) | (uint8_t)data[1]);
            UPDATE_REG(COMM_JOY_RECV_H, (uint16_t)((uint16_t)data[4] << 8) | (uint8_t)data[3]);
            UPDATE_REG(COMM_JOYSTAT, READ16LE(&g_ioMem[COMM_JOYSTAT]) | JOYSTAT_RECV);
            UPDATE_REG(COMM_JOYCNT, READ16LE(&g_ioMem[COMM_JOYCNT]) | JOYCNT_RECV_COMPLETE);
            nextjoybusupdate = TICKS_PER_SECOND / BYTES_PER_SECOND;
            booted = true;
            break;

        default:
            nextjoybusupdate = TICKS_PER_SECOND / 40000;
            lastjoybusupdate = 0;
            return; // ignore
        }

        lastjoybusupdate = 0;
        resp.push_back((uint8_t)READ16LE(&g_ioMem[COMM_JOYSTAT]));

        if (cmd == JOY_CMD_READ) {
            UPDATE_REG(COMM_JOYSTAT, READ16LE(&g_ioMem[COMM_JOYSTAT]) & ~JOYSTAT_SEND);
        }

        dol->Send(resp);

        // Generate SIO interrupt if we can
        if (((cmd == JOY_CMD_RESET) || (cmd == JOY_CMD_READ) || (cmd == JOY_CMD_WRITE))
            && (READ16LE(&g_ioMem[COMM_JOYCNT]) & JOYCNT_INT_ENABLE)) {
            CPURaiseSioIRQ();
        }

        lastcommand = 0;
    }
}

static void JoyBusShutdown()
{
    delete dol;
    dol = NULL;
}

#define MAX_CLIENTS (lanlink.numslaves + 1)

// Server
RFUServer::RFUServer(void)
{
    // Nobody has declared themselves host yet; without this the "is host"
    // flag serialized to every client was an indeterminate stack value.
    current_host = 0;
    for (int j = 0; j < 5; j++)
        rfu_data.rfu_signal[j] = 0;
}

sf::Packet& RFUServer::Serialize(sf::Packet& packet, int slave)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != slave) {
            packet << (i == current_host);
            packet << rfu_data.rfu_reqid[i];
            if (i == current_host) {
                for (int j = 0; j < 7; j++)
                    packet << rfu_data.rfu_broadcastdata[i][j];
            }
        }

        if (i == slave) {
            packet << rfu_data.rfu_clientidx[i];
            packet << rfu_data.rfu_is_host[i];
            packet << rfu_data.rfu_listback[i];

            if (rfu_data.rfu_listback[i] > 0)
                log("num_data_packets from %d to %d = %d\n", linkid, i, rfu_data.rfu_listback[i]);

            for (int j = 0; j <= rfu_data.rfu_listback[i]; j++) {
                packet << rfu_data.rfu_datalist[i][j & 0xff].len;

                for (int k = 0; k < rfu_data.rfu_datalist[i][j & 0xff].len; k++)
                    packet << rfu_data.rfu_datalist[i][j & 0xff].data[k];

                packet << rfu_data.rfu_datalist[i][j & 0xff].gbaid;
            }
        }
    }

    packet << linktime; // Synchronise clocks by setting slave clock to master clock
    return packet;
}

void RFUServer::DeSerialize(sf::Packet& packet, int slave)
{
    bool slave_is_host = false;
    packet >> slave_is_host;
    packet >> rfu_data.rfu_reqid[slave];
    if (slave_is_host) {
        current_host = (uint8_t)slave;
        for (int j = 0; j < 7; j++)
            packet >> rfu_data.rfu_broadcastdata[slave][j];
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != slave) {
            uint8_t num_data_sent = 0;
            packet >> rfu_data.rfu_clientidx[i];
            packet >> rfu_data.rfu_is_host[i];
            packet >> num_data_sent;

            for (int j = rfu_data.rfu_listback[i]; j <= (rfu_data.rfu_listback[i] + num_data_sent); j++) {
                packet >> rfu_data.rfu_datalist[i][j & 0xff].len;

                for (int k = 0; k < rfu_data.rfu_datalist[i][j & 0xff].len; k++)
                    packet >> rfu_data.rfu_datalist[i][j & 0xff].data[k];

                packet >> rfu_data.rfu_datalist[i][j & 0xff].gbaid;
            }

            rfu_data.rfu_listback[i] = (rfu_data.rfu_listback[i] + num_data_sent) & 0xff;
        }
    }
}

void RFUServer::Send(void)
{
    // One fresh packet per slave: Serialize() appends to the packet it is
    // given, so reusing one packet sent slave 2 a copy of slave 1's frame
    // followed by its own (and slave 3 all three).
    for (int i = 1; i <= lanlink.numslaves && i <= 3; i++) {
        sf::Packet packet;
        (void)tcpsocket[i].send(Serialize(packet, i));
    }
}

// Receive data from all slaves to master
void RFUServer::Recv(void)
{
    //int numbytes;
    {
        fdset.clear();

        for (int i = 0; i < lanlink.numslaves; i++)
            fdset.add(tcpsocket[i + 1]);

        //bool all_ready = false;
        //while (!all_ready)
        //{
        //	fdset.wait(sf::milliseconds(1));
        //	int count = 0;
        //	for (int sl = 0; sl < lanlink.numslaves; sl++)
        //	{
        //		if (fdset.isReady(tcpsocket[sl + 1]))
        //			count++;
        //	}
        //	if (count == lanlink.numslaves)
        //		all_ready = true;
        //}

        for (int i = 0; i < lanlink.numslaves; i++) {
            sf::Packet packet;
            tcpsocket[i + 1].setBlocking(false);
            sf::Socket::Status status = tcpsocket[i + 1].receive(packet);
            if (status == sf::Socket::Status::Disconnected || status == sf::Socket::Status::Error) {
                char message[30];
                snprintf(message, sizeof(message), _("Player %d disconnected."), i + 1);
                systemScreenMessage(message);
                RequestLinkClose();
                return;
            }
            // Only deserialize a fully-received packet. The old code ran
            // DeSerialize even on a partial/failed receive, writing garbage
            // over rfu_data for that slave.
            if (status != sf::Socket::Status::Done)
                continue;
            DeSerialize(packet, i + 1);
        }
    }
}

// Client
RFUClient::RFUClient(void)
{
    transferring = false;

    for (int j = 0; j < 5; j++)
        rfu_data.rfu_signal[j] = 0;
}

sf::Packet& RFUClient::Serialize(sf::Packet& packet)
{
    packet << rfu_ishost;
    packet << rfu_data.rfu_reqid[linkid];
    if (rfu_ishost) {
        for (int j = 0; j < 7; j++)
            packet << rfu_data.rfu_broadcastdata[linkid][j];
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != linkid) {
            packet << rfu_data.rfu_clientidx[i];
            packet << rfu_data.rfu_is_host[i];
            packet << rfu_data.rfu_listback[i];

            if (rfu_data.rfu_listback[i] > 0)
                log("num_data_packets from %d to %d = %d\n", linkid, i, rfu_data.rfu_listback[i]);

            for (int j = 0; j <= rfu_data.rfu_listback[i]; j++) {
                packet << rfu_data.rfu_datalist[i][j].len;

                for (int k = 0; k < rfu_data.rfu_datalist[i][j].len; k++)
                    packet << rfu_data.rfu_datalist[i][j].data[k];

                packet << rfu_data.rfu_datalist[i][j].gbaid;
            }
        }
    }
    return packet;
}

void RFUClient::DeSerialize(sf::Packet& packet)
{
    bool is_current_host = false;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != linkid) {
            packet >> is_current_host;
            packet >> rfu_data.rfu_reqid[i];
            if (is_current_host) {
                for (int j = 0; j < 7; j++)
                    packet >> rfu_data.rfu_broadcastdata[i][j];
            }
        }

        if (i == linkid) {
            uint8_t num_data_sent = 0;
            packet >> rfu_data.rfu_clientidx[i];
            packet >> rfu_data.rfu_is_host[i];
            packet >> num_data_sent;

            for (int j = rfu_data.rfu_listback[i]; j <= (rfu_data.rfu_listback[i] + num_data_sent); j++) {
                packet >> rfu_data.rfu_datalist[i][j & 0xff].len;

                for (int k = 0; k < rfu_data.rfu_datalist[i][j & 0xff].len; k++)
                    packet >> rfu_data.rfu_datalist[i][j & 0xff].data[k];

                packet >> rfu_data.rfu_datalist[i][j & 0xff].gbaid;
            }

            rfu_data.rfu_listback[i] = (rfu_data.rfu_listback[i] + num_data_sent) & 0xff;
        }
    }

    packet >> linktime; // Synchronise clocks by setting slave clock to master clock
}

void RFUClient::Send()
{
    sf::Packet packet;
    (void)lanlink.tcpsocket.send(Serialize(packet));
}

void RFUClient::Recv(void)
{
    if (rfu_data.numgbas < 2)
        return;

    fdset.clear();
    lanlink.tcpsocket.setBlocking(false);
    fdset.add(lanlink.tcpsocket);
    if (fdset.wait(sf::milliseconds(166)) == 0) {
        // No data within the window. Bail instead of falling through to
        // receive(): the old code deserialized an unfilled packet, which
        // wrote zeros over rfu_data and corrupted the RFU state.
        // Rate-limit the message; on a slow link this fired every 166 ms.
        static sf::Clock timeout_message_clock;
        if (timeout_message_clock.getElapsedTime().asMilliseconds() > 3000) {
            timeout_message_clock.restart();
            systemScreenMessage(_("Server timed out."));
        }
        return;
    }
    sf::Packet packet;
    sf::Socket::Status status = lanlink.tcpsocket.receive(packet);
    if (status == sf::Socket::Status::Disconnected || status == sf::Socket::Status::Error) {
        systemScreenMessage(_("Server disconnected."));
        RequestLinkClose();
        return;
    }
    if (status != sf::Socket::Status::Done)
        return; // partial / not ready: nothing to deserialize yet
    DeSerialize(packet);
}

static ConnectionState ConnectUpdateRFUSocket(char* const message, size_t size)
{
    ConnectionState newState = LINK_NEEDS_UPDATE;

    if (lanlink.server) {
        sf::SocketSelector fdset;
        fdset.add(lanlink.tcplistener);

        if (fdset.wait(sf::milliseconds(150))) {
            // uint16_t to match the client's `packet >> receivedId`; as an
            // int this serialized 4 bytes where the client read 2, so the
            // client parsed id 0 and never finished the handshake.
            uint16_t nextSlave = (uint16_t)(lanlink.connectedSlaves + 1);

            sf::Socket::Status st = lanlink.tcplistener.accept(rfu_server.tcpsocket[nextSlave]);

            // Anything but a completed accept just means "no new client
            // this tick".
            if (st == sf::Socket::Status::Done) {
                LinkSetNoDelay(rfu_server.tcpsocket[nextSlave]);
                sf::Packet packet;
                packet << nextSlave << lanlink.numslaves;

                if (rfu_server.tcpsocket[nextSlave].send(packet) == sf::Socket::Status::Done) {
                    snprintf(message, size, N_("Player %d connected"), nextSlave);
                    lanlink.connectedSlaves++;
                } else {
                    rfu_server.tcpsocket[nextSlave].disconnect();
                }
            }
        }

        if (lanlink.numslaves == lanlink.connectedSlaves) {
            for (int i = 1; i <= lanlink.numslaves; i++) {
                sf::Packet packet;
                packet << true;

                (void)rfu_server.tcpsocket[i].send(packet);
                rfu_server.tcpsocket[i].setBlocking(false);
            }

            snprintf(message, size, N_("All players connected"));
            newState = LINK_OK;
        }
    } else {

        sf::Packet packet;
        lanlink.tcpsocket.setBlocking(false);
        sf::Socket::Status status = lanlink.tcpsocket.receive(packet);

        if (status == sf::Socket::Status::Error || status == sf::Socket::Status::Disconnected) {
            snprintf(message, size, N_("Network error."));
            newState = LINK_ERROR;
        } else if (status == sf::Socket::Status::Done) {
            // First completed receive proves the connection is established
            // (and survives any connect retry, which swaps the underlying
            // socket handle).
            LinkSetNoDelay(lanlink.tcpsocket);

            if (linkid == 0) {
                uint16_t receivedId, receivedSlaves;
                packet >> receivedId >> receivedSlaves;

                if (packet) {
                    linkid = receivedId;
                    lanlink.numslaves = receivedSlaves;

                    snprintf(message, size, N_("Connected as #%d, Waiting for %d players to join"),
                        linkid + 1, lanlink.numslaves - linkid);
                }
            } else {
                bool gameReady;
                packet >> gameReady;

                if (packet && gameReady) {
                    newState = LINK_OK;
                    snprintf(message, size, N_("All players joined."));
                }
            }

            sf::SocketSelector fdset;
            fdset.add(lanlink.tcpsocket);
            (void)fdset.wait(sf::milliseconds(150));
        }
    }

    rfu_data.numgbas = (uint8_t)(lanlink.numslaves + 1);
    log("num gbas: %d\n", rfu_data.numgbas);

    return newState;
}

// The GBA wireless RFU (see adapter3.txt)
static void StartRFUSocket(uint16_t value)
{
    int siomode = GetSIOMode(value, READ16LE(&g_ioMem[COMM_RCNT]));

    if (value)
        rfu_enabled = (siomode == NORMAL32);

    if (((READ16LE(&g_ioMem[COMM_SIOCNT]) & 0x5080) == SIO_TRANS_32BIT) && ((value & 0x5080) == (SIO_TRANS_32BIT | SIO_IRQ_ENABLE | SIO_TRANS_START))) { //RFU Reset, may also occur before cable link started
        rfu_data.rfu_listfront[linkid] = 0;
        rfu_data.rfu_listback[linkid] = 0;
    }

    if (!rfu_enabled) {
        if ((value & 0x5080) == (SIO_TRANS_32BIT | SIO_IRQ_ENABLE | SIO_TRANS_START)) { //0x5083 //game tried to send wireless command but w/o the adapter
            if (READ16LE(&g_ioMem[COMM_SIOCNT]) & SIO_IRQ_ENABLE) //IRQ Enable
            {
                CPURaiseSioIRQ(); //Serial Communication
            }
            value &= ~SIO_TRANS_START; //Start bit.7 reset //may cause the game to retry sending again
            value |= SIO_TRANS_FLAG_SEND_DISABLE; //SO bit.3 set automatically upon transfer completion
            transfer_direction = SENDING;
        }
        return;
    }

    uint32_t CurCOM = 0, CurDAT = 0;

    switch (GetSIOMode(value, READ16LE(&g_ioMem[COMM_RCNT]))) {
    case NORMAL8:
        rfu_polarity = 0;
        UPDATE_REG(COMM_SIOCNT, value);
        return;
        break;
    case NORMAL32:
        //don't do anything if previous cmd aren't sent yet, may fix Boktai2 Not Detecting wireless adapter
        //if (transfer_direction == RECEIVING)
        //{
        //	UPDATE_REG(COMM_SIOCNT, value);
        //	return;
        //}

        //Moving this to the bottom might prevent Mario Golf Adv from Occasionally Not Detecting wireless adapter
        if (value & SIO_TRANS_FLAG_SEND_DISABLE) //Transfer Enable Flag Send (SO.bit.3, 1=Disable Transfer/Not Ready)
            value &= ~SIO_TRANS_FLAG_RECV_ENABLE; //Transfer enable flag receive (0=Enable Transfer/Ready, SI.bit.2=SO.bit.3 of otherside)	// A kind of acknowledge procedure
        else //(SO.Bit.3, 0=Enable Transfer/Ready)
            value |= SIO_TRANS_FLAG_RECV_ENABLE; //SI.bit.2=1 (otherside is Not Ready)

        if ((value & (SIO_INT_CLOCK | SIO_TRANS_FLAG_RECV_ENABLE)) == SIO_INT_CLOCK)
            value |= SIO_INT_CLOCK_SEL_2MHZ; //wireless always use 2Mhz speed right? this will fix MarioGolfAdv Not Detecting wireless

        if (value & SIO_TRANS_START) //start/busy bit
        {
            if ((value & (SIO_INT_CLOCK | SIO_INT_CLOCK_SEL_2MHZ)) == SIO_INT_CLOCK)
                rfu_transfer_end = 2048;
            else
                rfu_transfer_end = 256;

            uint16_t siodata_h = READ16LE(&g_ioMem[COMM_SIODATA32_H]);
            RfuTrace("sock st=%d out=%08X cnt=%04X cmd=%02X pol=%d",
                rfu_state, READ32LE(&g_ioMem[COMM_SIODATA32_L]), value,
                rfu_cmd, rfu_polarity);
            switch (rfu_state) {
            case RFU_INIT:
                if (READ32LE(&g_ioMem[COMM_SIODATA32_L]) == 0xb0bb8001) {
                    rfu_state = RFU_COMM; // end of startup
                    rfu_initialized = true;
                    value &= ~SIO_TRANS_FLAG_RECV_ENABLE; //0xff7b; //Bit.2 need to be 0 to indicate a finished initialization to fix MarioGolfAdv from occasionally Not Detecting wireless adapter (prevent it from sending 0x7FFE8001 comm)?
                    rfu_polarity = 0; //not needed?
                }
                rfu_buf = (READ16LE(&g_ioMem[COMM_SIODATA32_L]) << 16) | siodata_h;
                break;
            case RFU_COMM:
                CurCOM = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                if (siodata_h == 0x9966) //initialize cmd
                {
                    uint8_t tmpcmd = (uint8_t)CurCOM;
                    if (tmpcmd != 0x10 && tmpcmd != 0x11 && tmpcmd != 0x13 && tmpcmd != 0x14 && tmpcmd != 0x16 && tmpcmd != 0x17 && tmpcmd != 0x19 && tmpcmd != 0x1a && tmpcmd != 0x1b && tmpcmd != 0x1c && tmpcmd != 0x1d && tmpcmd != 0x1e && tmpcmd != 0x1f && tmpcmd != 0x20 && tmpcmd != 0x21 && tmpcmd != 0x24 && tmpcmd != 0x25 && tmpcmd != 0x26 && tmpcmd != 0x27 && tmpcmd != 0x30 && tmpcmd != 0x32 && tmpcmd != 0x33 && tmpcmd != 0x34 && tmpcmd != 0x3d && tmpcmd != 0xa8 && tmpcmd != 0xee) {
                    }
                    rfu_counter = 0;
                    if ((rfu_qsend2 = rfu_qsend = g_ioMem[0x121]) != 0) { //COMM_SIODATA32_L+1, following data [to send]
                        rfu_state = RFU_SEND;
                    }
                    if (g_ioMem[COMM_SIODATA32_L] == 0xee) { //0xee cmd shouldn't override previous cmd
                        rfu_lastcmd = rfu_cmd2;
                        rfu_cmd2 = g_ioMem[COMM_SIODATA32_L];
                    } else {
                        rfu_lastcmd = rfu_cmd;
                        rfu_cmd = g_ioMem[COMM_SIODATA32_L];
                        rfu_cmd2 = 0;
                        if (rfu_cmd == 0x27 || rfu_cmd == 0x37) {
                            rfu_lastcmd2 = rfu_cmd;
                            rfu_lasttime = linktime;
                        } else if (rfu_cmd == 0x24) { //non-important data shouldn't overwrite important data from 0x25
                            rfu_lastcmd2 = rfu_cmd;
                            rfu_cansend = false;
                            //previous important data need to be received successfully before sending another important data
                            rfu_lasttime = linktime; //just to mark the last time a data being sent
                            if (rfu_data.rfu_q[linkid] < 2) { //can overwrite now
                                rfu_cansend = true;
                                rfu_data.rfu_q[linkid] = 0; //rfu_qsend;
                                rfu_data.rfu_qid[linkid] = 0;
                            } else if (!speedhack)
                                rfu_waiting = true; //don't wait with speedhack
                        } else if (rfu_cmd == 0x25 || rfu_cmd == 0x35) {
                            rfu_lastcmd2 = rfu_cmd;
                            rfu_cansend = false;
                            //previous important data need to be received successfully before sending another important data
                            rfu_lasttime = linktime;
                            if (rfu_data.rfu_q[linkid] < 2) {
                                rfu_cansend = true;
                                rfu_data.rfu_q[linkid] = 0; //rfu_qsend;
                                rfu_data.rfu_qid[linkid] = 0;
                            } else if (!speedhack)
                                rfu_waiting = true; //don't wait with speedhack
                        } else if (rfu_cmd == 0xa8 || rfu_cmd == 0xb6) {
                            //wait for [important] data when previously sent is important data, might only need to wait for the 1st 0x25 cmd
                        } else if (rfu_cmd == 0x11 || rfu_cmd == 0x1a || rfu_cmd == 0x26) {
                            if (rfu_lastcmd2 == 0x24)
                                rfu_waiting = true;
                        }
                    }
                    if (rfu_waiting)
                        rfu_buf = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    else
                        rfu_buf = 0x80000000;
                } else if (siodata_h == 0x8000) //finalize cmd, the game will send this when polarity reversed (expecting something)
                {
                    rfu_qrecv_broadcast_data_len = 0;
                    if (rfu_cmd2 == 0xee) {
                        if (rfu_masterdata[0] == 2) //is this value of 2 related to polarity?
                            rfu_polarity = 0; //to normalize polarity after finalize looks more proper
                        rfu_buf = 0x99660000 | (rfu_qrecv_broadcast_data_len << 8) | (rfu_cmd2 ^ 0x80);
                    } else {
                        switch (rfu_cmd) {
                        case 0x1a: // check if someone joined
                            if (rfu_data.rfu_is_host[linkid]) {
                                gbaidx = gbaid;

                                do {
                                    gbaidx = (gbaidx + 1) % rfu_data.numgbas; // check this numgbas = 3, gbaid = 0, gbaidx = 1,
                                    if (gbaidx != linkid && rfu_data.rfu_reqid[gbaidx] == (linkid << 3) + 0x61f1) {
                                        rfu_masterdata[rfu_qrecv_broadcast_data_len++] = (gbaidx << 3) + 0x61f1;
                                    }
                                } while (gbaidx != gbaid && rfu_data.numgbas >= 2);

                                if (rfu_qrecv_broadcast_data_len > 0) {
                                    bool ok = false;
                                    for (int i = 0; i < rfu_numclients; i++)
                                        if ((rfu_clientlist[i] & 0xffff) == rfu_masterdata[0]) {
                                            ok = true;
                                            break;
                                        }
                                    if (!ok) {
                                        rfu_curclient = rfu_numclients;
                                        rfu_data.rfu_clientidx[(rfu_masterdata[0] - 0x61f1) >> 3] = rfu_numclients;
                                        rfu_clientlist[rfu_numclients] = rfu_masterdata[0] | (rfu_numclients << 16);
                                        rfu_numclients++;
                                        gbaid = (rfu_masterdata[0] - 0x61f1) >> 3;
                                        rfu_data.rfu_signal[gbaid] = 0xffffffff >> ((3 - (rfu_numclients - 1)) << 3);
                                    }
                                    if (gbaid == linkid) {
                                        gbaid = (rfu_masterdata[0] - 0x61f1) >> 3;
                                    }
                                    rfu_state = RFU_RECV;
                                }
                            }
                            if (rfu_numclients > 0) {
                                for (int i = 0; i < rfu_numclients; i++)
                                    rfu_masterdata[i] = rfu_clientlist[i];
                            }
                            rfu_id = (uint16_t)((gbaid << 3) + 0x61f1);
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x1f: // join a room as client
                            // TODO: to fix infinte send&recv w/o giving much cance to update the screen when both side acting as client
                            // on MarioGolfAdv lobby(might be due to leftover data when switching from host to join mode at the same time?)
                            rfu_id = (uint16_t)rfu_masterdata[0];
                            gbaid = (rfu_id - 0x61f1) >> 3;
                            rfu_idx = rfu_id;
                            gbaidx = gbaid;
                            rfu_lastcmd2 = 0;
                            numtransfers = 0;
                            rfu_data.rfu_q[linkid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            rfu_data.rfu_reqid[linkid] = rfu_id;
                            // TODO:might failed to reset rfu_request when being accessed by otherside at the same time, sometimes both acting
                            // as client but one of them still have request[linkid]!=0 //to prevent both GBAs from acting as Host, client can't
                            // be a host at the same time
                            rfu_data.rfu_is_host[linkid] = 0;
                            if (linkid != gbaid) {
                                rfu_data.rfu_signal[linkid] = 0x00ff;
                                rfu_data.rfu_is_host[gbaid] |= 1 << linkid; // tells the other GBA(a host) that someone(a client) is joining
                                log("%09d: joining room: signal: %d   linkid: %d  gbaid: %d\n", linktime, rfu_data.rfu_signal[linkid], linkid, gbaid);
                            }
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x1e: // receive broadcast data
                            numtransfers = 0;
                            rfu_numclients = 0;
                            rfu_data.rfu_is_host[linkid] = 0; //to prevent both GBAs from acting as Host and thinking both of them have Client?
                            rfu_data.rfu_q[linkid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            [[fallthrough]];
                        case 0x1d: // no visible difference
                            rfu_data.rfu_is_host[linkid] = 0;
                            memset(rfu_masterdata, 0, sizeof(rfu_data.rfu_broadcastdata[linkid]));
                            rfu_qrecv_broadcast_data_len = 0;
                            for (int i = 0; i < rfu_data.numgbas; i++) {
                                if (i != linkid && rfu_data.rfu_broadcastdata[i][0]) {
                                    memcpy(&rfu_masterdata[rfu_qrecv_broadcast_data_len], rfu_data.rfu_broadcastdata[i], sizeof(rfu_data.rfu_broadcastdata[i]));
                                    rfu_qrecv_broadcast_data_len += 7;
                                }
                            }
                            // is this needed? to prevent MarioGolfAdv from joining it's own room when switching
                            // from host to client mode due to left over room data in the game buffer?
                            // if(rfu_qrecv==0) rfu_qrecv = 7;
                            if (rfu_qrecv_broadcast_data_len > 0) {
                                log("%09d: switching to RFU_RECV (broadcast)\n", linktime);
                                rfu_state = RFU_RECV;
                            }
                            rfu_polarity = 0;
                            rfu_counter = 0;
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x16: // send broadcast data (ie. room name)
                            //start broadcasting here may cause client to join other client in pokemon coloseum
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x11: // get signal strength
                            //Switch remote id
                            //check signal
                            if (rfu_data.numgbas >= 2 && (rfu_data.rfu_is_host[linkid] | rfu_data.rfu_is_host[gbaid])) //signal only good when connected
                                if (rfu_ishost) { //update, just incase there are leaving clients
                                    uint8_t rfureq = rfu_data.rfu_is_host[linkid];
                                    uint8_t oldnum = rfu_numclients;
                                    rfu_numclients = 0;
                                    for (int i = 0; i < 8; i++) {
                                        if (rfureq & 1)
                                            rfu_numclients++;
                                        rfureq >>= 1;
                                    }
                                    if (rfu_numclients > oldnum)
                                        rfu_numclients = oldnum; //must not be higher than old value, which means the new client haven't been processed by 0x1a cmd yet
                                    rfu_data.rfu_signal[linkid] = 0xffffffff >> ((4 - rfu_numclients) << 3);
                                } else
                                    rfu_data.rfu_signal[linkid] = rfu_data.rfu_signal[gbaid];
                            else
                                rfu_data.rfu_signal[linkid] = 0;
                            if (rfu_qrecv_broadcast_data_len == 0) {
                                rfu_qrecv_broadcast_data_len = 1;
                                rfu_masterdata[0] = (uint32_t)rfu_data.rfu_signal[linkid];
                            }
                            if (rfu_qrecv_broadcast_data_len > 0) {
                                rfu_state = RFU_RECV;
                                rfu_masterdata[rfu_qrecv_broadcast_data_len - 1] = (uint32_t)rfu_data.rfu_signal[gbaid];
                            }
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x33: // rejoin status check?
                            if (rfu_data.rfu_signal[linkid] || numtransfers == 0)
                                rfu_masterdata[0] = 0;
                            else //0=success
                                rfu_masterdata[0] = (uint32_t)-1; //0xffffffff; //1=failed, 2++ = reserved/invalid, we use invalid value to let the game retries 0x33 until signal restored
                            rfu_cmd ^= 0x80;
                            rfu_state = RFU_RECV;
                            rfu_qrecv_broadcast_data_len = 1;
                            break;
                        case 0x14: // reset current client index and error check?
                            if ((rfu_data.rfu_signal[linkid] || numtransfers == 0) && gbaid != linkid)
                                rfu_masterdata[0] = ((!rfu_ishost ? 0x100 : 0 + rfu_data.rfu_clientidx[gbaid]) << 16) | ((gbaid << 3) + 0x61f1);
                            rfu_masterdata[0] = 0; //0=error, non-zero=good?
                            rfu_cmd ^= 0x80;
                            rfu_state = RFU_RECV;
                            rfu_qrecv_broadcast_data_len = 1;
                            break;
                        case 0x13: // error check?
                            if (rfu_data.rfu_signal[linkid] || numtransfers == 0 || rfu_initialized) {
                                rfu_masterdata[0] = ((rfu_ishost ? 0x100 : 0 + rfu_data.rfu_clientidx[linkid]) << 16) | ((linkid << 3) + 0x61f1);
                            } else //high word should be 0x0200 ? is 0x0200 means 1st client and 0x4000 means 2nd client?
                            {
                                log("%09d: error status\n", linktime);
                                rfu_masterdata[0] = 0; //0=error, non-zero=good?
                            }
                            rfu_cmd ^= 0x80;
                            rfu_state = RFU_RECV;
                            rfu_qrecv_broadcast_data_len = 1;
                            break;
                        case 0x20: // client, this has something to do with 0x1f
                            rfu_masterdata[0] = (rfu_data.rfu_clientidx[linkid]) << 16; //needed for client
                            rfu_masterdata[0] |= (linkid << 3) + 0x61f1; //0x1234; //0x641b; //max id value? Encryption key or Station Mode? (0xFBD9/0xDEAD=Access Point mode?)
                            rfu_data.rfu_q[linkid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            rfu_data.rfu_is_host[linkid] = 0; //TODO:may not works properly, sometimes both acting as client but one of them still have request[linkid]!=0 //to prevent both GBAs from acting as Host, client can't be a host at the same time
                            if (rfu_data.rfu_signal[gbaid] < rfu_data.rfu_signal[linkid])
                                rfu_data.rfu_signal[gbaid] = rfu_data.rfu_signal[linkid];

                            rfu_polarity = 0;
                            rfu_state = RFU_RECV;
                            rfu_qrecv_broadcast_data_len = 1;
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x21: // client, this too
                            rfu_masterdata[0] = (rfu_data.rfu_clientidx[linkid]) << 16; //not needed?
                            rfu_masterdata[0] |= (linkid << 3) + 0x61f1; //0x641b; //max id value? Encryption key or Station Mode? (0xFBD9/0xDEAD=Access Point mode?)
                            rfu_data.rfu_q[linkid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            rfu_data.rfu_is_host[linkid] = 0; //TODO:may not works properly, sometimes both acting as client but one of them still have request[linkid]!=0 //to prevent both GBAs from acting as Host, client can't be a host at the same time
                            rfu_polarity = 0;
                            rfu_state = RFU_RECV; //3;
                            rfu_qrecv_broadcast_data_len = 1;
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x19: // server bind/start listening for client to join, may be used in the middle of host<->client communication w/o causing clients to dc?
                            rfu_data.rfu_q[linkid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            rfu_data.rfu_broadcastdata[linkid][0] = (linkid << 3) + 0x61f1; //start broadcasting room name
                            rfu_data.rfu_clientidx[linkid] = 0;
                            rfu_ishost = true;
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x1c: //client, might reset some data?
                            rfu_ishost = false; //TODO: prevent both GBAs act as client but one of them have rfu_request[linkid]!=0 on MarioGolfAdv lobby
                            rfu_numclients = 0;
                            rfu_curclient = 0;
                            rfu_data.rfu_listfront[linkid] = 0;
                            rfu_data.rfu_listback[linkid] = 0;
                            rfu_data.rfu_q[linkid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            [[fallthrough]];
                        case 0x1b: //host, might reset some data? may be used in the middle of host<->client communication w/o causing clients to dc?
                            rfu_data.rfu_broadcastdata[linkid][0] = 0; //0 may cause player unable to join in pokemon union room?
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x30: //reset some data
                            if (linkid != gbaid) { //(rfu_data.numgbas >= 2)
                                rfu_data.rfu_is_host[gbaid] &= ~(1 << linkid); //rfu_data.rfu_request[gbaid] = 0;
                            }
                            while (rfu_data.rfu_signal[linkid]) {
                                rfu_data.rfu_signal[linkid] = 0;
                                rfu_data.rfu_is_host[linkid] = 0; //There is a possibility where rfu_request/signal didn't get zeroed here when it's being read by the other GBA at the same time
                            }
                            rfu_data.rfu_listfront[linkid] = 0;
                            rfu_data.rfu_listback[linkid] = 0;
                            rfu_data.rfu_q[linkid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            rfu_data.rfu_proto[linkid] = 0;
                            rfu_data.rfu_reqid[linkid] = 0;
                            rfu_data.rfu_linktime[linkid] = 0;
                            rfu_data.rfu_gdata[linkid] = 0;
                            rfu_data.rfu_broadcastdata[linkid][0] = 0;
                            rfu_polarity = 0; //is this included?
                            numtransfers = 0;
                            rfu_numclients = 0;
                            rfu_curclient = 0;
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x3d: // init/reset rfu data
                            rfu_initialized = false;
                            [[fallthrough]];
                        case 0x10: // init/reset rfu data
                            if (linkid != gbaid) { //(rfu_data.numgbas >= 2)
                                rfu_data.rfu_is_host[gbaid] &= ~(1 << linkid); //rfu_data.rfu_request[gbaid] = 0;
                            }
                            while (rfu_data.rfu_signal[linkid]) {
                                rfu_data.rfu_signal[linkid] = 0;
                                rfu_data.rfu_is_host[linkid] = 0; //There is a possibility where rfu_request/signal didn't get zeroed here when it's being read by the other GBA at the same time
                            }
                            rfu_data.rfu_listfront[linkid] = 0;
                            rfu_data.rfu_listback[linkid] = 0;
                            rfu_data.rfu_q[linkid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            rfu_data.rfu_proto[linkid] = 0;
                            rfu_data.rfu_reqid[linkid] = 0;
                            rfu_data.rfu_linktime[linkid] = 0;
                            rfu_data.rfu_gdata[linkid] = 0;
                            rfu_data.rfu_broadcastdata[linkid][0] = 0;
                            rfu_polarity = 0; //is this included?
                            numtransfers = 0;
                            rfu_numclients = 0;
                            rfu_curclient = 0;
                            rfu_id = 0;
                            rfu_idx = 0;
                            gbaid = linkid;
                            gbaidx = gbaid;
                            rfu_ishost = false;
                            rfu_qrecv_broadcast_data_len = 0;
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x36: //does it expect data returned?
                        case 0x26:
                            //Switch remote id to available data
                            bool ok;
                            int ctr;
                            ctr = 0;
                            if (rfu_data.rfu_listfront[linkid] != rfu_data.rfu_listback[linkid]) //data existed
                                do {
                                    uint8_t qdata_len = rfu_data.rfu_datalist[linkid][rfu_data.rfu_listfront[linkid]].len; //(uint8_t)rfu_data.rfu_qlist[linkid][rfu_data.rfu_listfront[linkid]];
                                    ok = false;
                                    if (qdata_len != rfu_qrecv_broadcast_data_len)
                                        ok = true;
                                    else
                                        for (int i = 0; i < qdata_len; i++)
                                            if (rfu_data.rfu_datalist[linkid][rfu_data.rfu_listfront[linkid]].data[i] != rfu_masterdata[i]) {
                                                ok = true;
                                                break;
                                            } // dupe data check

                                    if (qdata_len == 0 && ctr == 0)
                                        ok = true; //0-size data

                                    //if (ok) //next data is not a duplicate of currently unprocessed data
                                    if (rfu_qrecv_broadcast_data_len < 2 || qdata_len > 1) {
                                        if (rfu_qrecv_broadcast_data_len > 1) { //stop here if next data is different than currently unprocessed non-ping data
                                            //break;
                                        }

                                        if (qdata_len >= rfu_qrecv_broadcast_data_len) {
                                            rfu_masterq = rfu_qrecv_broadcast_data_len = qdata_len;
                                            gbaid = rfu_data.rfu_datalist[linkid][rfu_data.rfu_listfront[linkid]].gbaid;
                                            rfu_id = (uint16_t)((gbaid << 3) + 0x61f1);
                                            if (rfu_ishost) {
                                                rfu_curclient = (uint8_t)rfu_data.rfu_clientidx[gbaid];
                                            }
                                            if (rfu_qrecv_broadcast_data_len != 0) { //data size > 0
                                                memcpy(rfu_masterdata, rfu_data.rfu_datalist[linkid][rfu_data.rfu_listfront[linkid]].data, std::min(rfu_masterq << 2, (int)sizeof(rfu_masterdata)));
                                            }
                                        }
                                    }

                                    rfu_data.rfu_listfront[linkid]++;
                                    ctr++;

                                    ok = (rfu_data.rfu_listfront[linkid] != rfu_data.rfu_listback[linkid] && rfu_data.rfu_datalist[linkid][rfu_data.rfu_listfront[linkid]].gbaid == gbaid);
                                } while (ok);

                            if (rfu_qrecv_broadcast_data_len > 0) { //data was available
                                rfu_state = RFU_RECV;
                                rfu_counter = 0;
                                rfu_lastcmd2 = 0;

                                //Switch remote id to next remote id
                            }
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x24: // send [non-important] data (used by server often)
                            rfu_data.rfu_linktime[linkid] = linktime; //save the ticks before reseted to zero

			    // rfu_qsend2 >= 0 due to being `uint8_t`
                            if (rfu_cansend) {
                                if (rfu_ishost) {
                                    for (int j = 0; j < rfu_data.numgbas; j++)
                                        if (j != linkid) {
                                            memcpy(rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].data, rfu_masterdata, 4 * rfu_qsend2);
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].gbaid = (uint8_t)linkid;
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].len = rfu_qsend2;
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].time = linktime;
                                            rfu_data.rfu_listback[j]++;
                                        }
                                } else if (linkid != gbaid) {
                                    memcpy(rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].data, rfu_masterdata, 4 * rfu_qsend2);
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].gbaid = (uint8_t)linkid;
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].len = rfu_qsend2;
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].time = linktime;
                                    rfu_data.rfu_listback[gbaid]++;
                                }
                            } else {
                                log("IgnoredSend[%02X] %d\n", rfu_cmd, rfu_qsend2);
                            }
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x25: // send [important] data & wait for [important?] reply data
                        case 0x35: // send [important] data & wait for [important?] reply data
                            rfu_data.rfu_linktime[linkid] = linktime; //save the ticks before changed to synchronize performance

                            if (rfu_cansend && rfu_qsend2 > 0) {
                                if (rfu_ishost) {
                                    for (int j = 0; j < rfu_data.numgbas; j++)
                                        if (j != linkid) {
                                            memcpy(rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].data, rfu_masterdata, 4 * rfu_qsend2);
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].gbaid = (uint8_t)linkid;
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].len = rfu_qsend2;
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].time = linktime;
                                            rfu_data.rfu_listback[j]++;
                                        }
                                } else if (linkid != gbaid) {
                                    memcpy(rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].data, rfu_masterdata, 4 * rfu_qsend2);
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].gbaid = (uint8_t)linkid;
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].len = rfu_qsend2;
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].time = linktime;
                                    rfu_data.rfu_listback[gbaid]++;
                                }
                            } else {
                                log("IgnoredSend[%02X] %d\n", rfu_cmd, rfu_qsend2);
                            }
                            [[fallthrough]];
                        //TODO: there is still a chance for 0x25 to be used at the same time on both GBA (both GBAs acting as client but keep sending & receiving using 0x25 & 0x26 for infinity w/o updating the screen much)
                        //Waiting here for previous data to be received might be too late! as new data already sent before finalization cmd
                        case 0x27: // wait for data ?
                        case 0x37: // wait for data ?
                            rfu_data.rfu_linktime[linkid] = linktime; //save the ticks before changed to synchronize performance

                            if (rfu_ishost) {
                                for (int j = 0; j < rfu_data.numgbas; j++)
                                    if (j != linkid) {
                                        rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].gbaid = (uint8_t)linkid;
                                        rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].len = 0; //rfu_qsend2;
                                        rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].time = linktime;
                                        rfu_data.rfu_listback[j]++;
                                    }
                            } else if (linkid != gbaid) {
                                rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].gbaid = (uint8_t)linkid;
                                rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].len = 0; //rfu_qsend2;
                                rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].time = linktime;
                                rfu_data.rfu_listback[gbaid]++;
                            }
                            rfu_cmd ^= 0x80;
                            break;

                        case 0xee: //is this need to be processed?
                            rfu_cmd ^= 0x80;
                            rfu_polarity = 1;
                            break;

                        case 0x17: // setup or something ?
                        default:
                            rfu_cmd ^= 0x80;
                            break;

                        case 0xa5: //	2nd part of send&wait function 0x25
                        case 0xa7: //	2nd part of wait function 0x27
                        case 0xb5: //	2nd part of send&wait function 0x35?
                        case 0xb7: //	2nd part of wait function 0x37?
                            if (rfu_data.rfu_listfront[linkid] != rfu_data.rfu_listback[linkid]) {
                                rfu_polarity = 1; //reverse polarity to make the game send 0x80000000 command word (to be replied with 0x99660028 later by the adapter)
                                if (rfu_cmd == 0xa5 || rfu_cmd == 0xa7)
                                    rfu_cmd = 0x28;
                                else
                                    rfu_cmd = 0x36; //there might be 0x29 also //don't return 0x28 yet until there is incoming data (or until 500ms-6sec timeout? may reset RFU after timeout)
                            } else
                                rfu_waiting = true;
                            //prevent GBAs from sending data at the same time (which may cause waiting at the same time in the case of 0x25), also gives time for the other side to read the data

                            if (rfu_waiting) {
                                rfu_transfer_end = 1; //(rfu_masterq + rfu_qsend2 + 1) * 2500;
                            }

                            if (rfu_waiting && rfu_transfer_end < 0)
                                rfu_transfer_end = 0;

                            break;
                        }
                        if (!rfu_waiting)
                            rfu_buf = 0x99660000 | (rfu_qrecv_broadcast_data_len << 8) | rfu_cmd;
                        else
                            rfu_buf = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    }
                } else { //unknown COMM word
                    // Three cases land here:
                    //  1. The game's RFU library restarts the adapter login
                    //     mid-session: the first handshake word carries the
                    //     "NI" key halfword in the LSBs (e.g. 0x7FFF494E when
                    //     our last reply was 0x8000xxxx). Drop back to
                    //     RFU_INIT with a clean slate so the NINTENDO
                    //     handshake is served by the login echo; stale
                    //     polarity/command state from the aborted session
                    //     would otherwise flip the SI bit during login and
                    //     make the library retry forever.
                    //  2. A stray login-tail word with the 0x7FF prefix
                    //     (MarioGolfAdv sends 0x7FFE8001 when a client exits
                    //     the lobby): must NOT reset the state machine or the
                    //     game shows "Linking error" instead of continuing.
                    //     (The old `>> 24 != 0x7ff` guard meant this but a
                    //     byte can never equal 0x7ff, so it always reset.)
                    //  3. Garbage: resync via a fresh login.
                    const uint32_t com = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    log("%09d: UnkCOM %08X  %04X  %08X %08X\n", linktime, com, PrevVAL, PrevCOM, PrevDAT);
                    if ((com & 0xFFFF) == 0x494E) { // login restart ("NI")
                        rfu_state = RFU_INIT;
                        rfu_polarity = 0;
                        rfu_cmd = 0;
                        rfu_cmd2 = 0;
                        rfu_lastcmd = 0;
                        rfu_lastcmd2 = 0;
                        rfu_waiting = false;
                    } else if ((com >> 20) != 0x7ff) {
                        rfu_state = RFU_INIT;
                    }
                    rfu_buf = (READ16LE(&g_ioMem[COMM_SIODATA32_L]) << 16) | siodata_h;
                }
                break;

            case RFU_SEND: //data following after initialize cmd
                CurDAT = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                if (--rfu_qsend == 0) {
                    rfu_state = RFU_COMM;
                }

                switch (rfu_cmd) {
                case 0x16:
                    if (rfu_counter < kRfuBroadcastPayloadWords) {
                        rfu_data.rfu_broadcastdata[linkid][1 + rfu_counter] =
                            READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    }
                    rfu_counter++;
                    break;

                case 0x17:
                    rfu_masterdata[rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x1f:
                    rfu_masterdata[rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x24:
                //if(rfu_data.rfu_proto[linkid]) break; //important data from 0x25 shouldn't be overwritten by 0x24
                case 0x25:
                case 0x35:
                    rfu_masterdata[rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    break;

                default:
                    rfu_masterdata[rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    break;
                }
                rfu_buf = 0x80000000;
                break;

            case RFU_RECV: //data following after finalize cmd
                if (--rfu_qrecv_broadcast_data_len == 0) {
                    rfu_state = RFU_COMM;
                }

                switch (rfu_cmd) {
                case 0x9d:
                case 0x9e:
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0xb6:
                case 0xa6:
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0x91: //signal strength
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0xb3: //rejoin error code?
                case 0x94: //last error code? //it seems like the game doesn't care about this value
                case 0x93: //last error code? //it seems like the game doesn't care about this value
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0xa0:
                    //max id value? Encryption key or Station Mode? (0xFBD9/0xDEAD=Access Point mode?)
                    //high word 0 = a success indication?
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;
                case 0xa1:
                    //max id value? the same with 0xa0 cmd?
                    //high word 0 = a success indication?
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0x9a:
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                default: //unknown data (should use 0 or -1 as default), usually returning 0 might cause the game to think there is something wrong with the connection (ie. 0x11/0x13 cmd)
                    //0x0173 //not 0x0000 as default?
                    //0x0000
                    //rfu_buf = 0xffffffff; //rfu_masterdata[rfu_counter++];
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;
                }
                break;
            }
            transfer_direction = RECEIVING;

            PrevVAL = value;
            PrevDAT = CurDAT;
            PrevCOM = CurCOM;
        }

        if (rfu_polarity)
            value ^= 4; // sometimes it's the other way around
        [[fallthrough]];
    default:
        UPDATE_REG(COMM_SIOCNT, value);
        return;
    }
}

bool LinkRFUUpdateSocket()
{
    if (rfu_enabled) {
        if (transfer_direction == RECEIVING && rfu_transfer_end <= 0) {
            if (rfu_waiting) {
                if (rfu_state != RFU_INIT) {
                    if (rfu_cmd == 0x24 || rfu_cmd == 0x25 || rfu_cmd == 0x35) {
                        if (rfu_data.rfu_q[linkid] < 2 || rfu_qsend > 1) {
                            rfu_cansend = true;
                            rfu_data.rfu_q[linkid] = 0;
                            rfu_data.rfu_qid[linkid] = 0;
                        }
                        rfu_buf = 0x80000000;
                    } else {
                        if (rfu_cmd == 0xa5 || rfu_cmd == 0xa7 || rfu_cmd == 0xb5 || rfu_cmd == 0xb7 || rfu_cmd == 0xee)
                            rfu_polarity = 1;
                        if (rfu_cmd == 0xa5 || rfu_cmd == 0xa7)
                            rfu_cmd = 0x28;
                        else if (rfu_cmd == 0xb5 || rfu_cmd == 0xb7)
                            rfu_cmd = 0x36;

                        if (READ32LE(&g_ioMem[COMM_SIODATA32_L]) == 0x80000000)
                            rfu_buf = 0x99660000 | (rfu_qrecv_broadcast_data_len << 8) | rfu_cmd;
                        else
                            rfu_buf = 0x80000000;
                    }
                    rfu_waiting = false;
                }
            }
            UPDATE_REG(COMM_SIODATA32_L, (uint16_t)rfu_buf);
            UPDATE_REG(COMM_SIODATA32_H, rfu_buf >> 16);
        }
    }
    return true;
}

static void UpdateRFUSocket(int ticks)
{
    // Age the in-flight transfer like the IPC path does (UpdateRFUIPC);
    // without this a started exchange only completed when the broadcast
    // timer below happened to zero rfu_transfer_end, up to 3000 ticks
    // late. The RFU library polls the start bit with a short timeout,
    // reads SIODATA32 before the reply is committed, fails its login
    // validation and restarts the NINTENDO handshake forever.
    rfu_transfer_end -= ticks;

    rfu_last_broadcast_time -= ticks;

    if (rfu_last_broadcast_time < 0) {
        if (linkid == 0) {
            linktime = 0;
            rfu_server.Recv(); // recv broadcast data
            (void)rfu_server.Send(); // send broadcast data
        } else {
            (void)rfu_client.Send(); // send broadcast data
            rfu_client.Recv(); // recv broadcast data
        }
        {
            const int max_clients = MAX_CLIENTS > 5 ? 5 : MAX_CLIENTS;
            for (int i = 0; i < max_clients; i++) {
                if (i != linkid) {
                    rfu_data.rfu_listback[i] = 0; // Flush the queue
                }
            }
        }
        // (rfu_transfer_end is aged by ticks above; historically it was
        // only zeroed here, which made every SIO exchange complete at the
        // whim of this 3000-tick broadcast timer instead of after the
        // 256/2048-tick transfer the game's RFU library expects.)

        if (rfu_last_broadcast_time < 0)
            rfu_last_broadcast_time = 3000;
        //rfu_last_broadcast_time = 5600; // Upper physical limit of 5600? 3000 packets/sec
    }

    if (rfu_enabled) {
        if (LinkRFUUpdateSocket()) {
            if (transfer_direction == RECEIVING && rfu_transfer_end <= 0) {
                transfer_direction = SENDING;
                uint16_t value = READ16LE(&g_ioMem[COMM_SIOCNT]);
                RfuTrace("sock st=%d  in=%08X cnt=%04X cmd=%02X pol=%d done",
                    rfu_state, READ32LE(&g_ioMem[COMM_SIODATA32_L]), value,
                    rfu_cmd, rfu_polarity);
                if (value & SIO_IRQ_ENABLE) {
                    CPURaiseSioIRQ();
                }

                //if (rfu_polarity) value ^= 4;
                value &= ~SIO_TRANS_FLAG_RECV_ENABLE;
                value |= (value & 1) << 2; //this will automatically set the correct polarity, even w/o rfu_polarity since the game will be the one who change the polarity instead of the adapter

                UPDATE_REG(COMM_SIOCNT, (value & ~SIO_TRANS_START) | SIO_TRANS_FLAG_SEND_DISABLE); //Start bit.7 reset, SO bit.3 set automatically upon transfer completion?
            }
            return;
        }
    }
}

void gbInitLink()
{
    if (GetLinkMode() == LINK_GAMEBOY_IPC) {
        gbInitLinkIPC();
    } else {
        LinkIsWaiting = false;
        LinkFirstTime = true;
    }
}

uint8_t gbStartLink(uint8_t b) //used on internal clock
{
    // A dropped peer detected on the previous byte defers its close to here
    // (the GB serial path does not run LinkUpdate); perform it before we
    // touch the sockets again.
    ProcessDeferredLinkClose();

    uint8_t dat = 0xff; //master (w/ internal clock) will gets 0xff if slave is turned off (or not ready yet also?)
    //if(linkid) return 0xff; //b; //Slave shouldn't be sending from here
    //int gbSerialOn = (gbMemory[0xff02] & 0x80); //not needed?
    gba_link_enabled = true; //(gbMemory[0xff02]!=0); //not needed?
    rfu_enabled = false;

    if (!gba_link_enabled)
        return 0xff;

    //Single Computer
    if (GetLinkMode() == LINK_GAMEBOY_IPC) {
        dat = gbStartLinkIPC(b);
    } else if (GetLinkMode() == LINK_GAMEBOY_SOCKET) {
        if (lanlink.numslaves == 1) {
            if (lanlink.server) {
                if (ls.ExchangeGB(b, linktimeout))
                    dat = cable_gb_data[1];
            } else {
                if (lc.ExchangeGB(b, linktimeout))
                    dat = cable_gb_data[0];
            }

            LinkIsWaiting = false;
            LinkFirstTime = true;
            if (dat != 0xff /*||b==0x00||dat==0x00*/)
                LinkFirstTime = false;
        }
    }
    return dat;
}

uint16_t gbLinkUpdate(uint8_t b, int gbSerialOn) //used on external clock
{
    ProcessDeferredLinkClose();

    uint8_t dat = b; //0xff; //slave (w/ external clocks) won't be getting 0xff if master turned off
    uint8_t recvd = 0;

    gba_link_enabled = true; //(gbMemory[0xff02]!=0);
    rfu_enabled = false;

    if (gbSerialOn) {
        if (gba_link_enabled) {
            //Single Computer
            if (GetLinkMode() == LINK_GAMEBOY_IPC) {
                return gbLinkUpdateIPC(b, gbSerialOn);
            } else if (GetLinkMode() == LINK_GAMEBOY_SOCKET) {
                if (lanlink.numslaves == 1) {
                    if (lanlink.server) {
                        recvd = ls.RecvGB(0) ? 1 : 0;
                        if (recvd) {
                            // A byte consumed on the external-clock path
                            // means any exchange stream this counter was
                            // tracking has been abandoned; without this,
                            // ExchangeGB's drain later eats a real byte.
                            ls.gb_pending = 0;
                            dat = cable_gb_data[1];
                            LinkIsWaiting = false;
                        } else
                            LinkIsWaiting = true;

                        if (!LinkIsWaiting) {
                            cable_gb_data[0] = b;
                            ls.SendGB();
                        }
                    } else {
                        recvd = lc.RecvGB(0) ? 1 : 0;
                        if (recvd) {
                            lc.gb_pending = 0;
                            dat = cable_gb_data[0];
                            LinkIsWaiting = false;
                        } else
                            LinkIsWaiting = true;

                        if (!LinkIsWaiting) {
                            cable_gb_data[1] = b;
                            lc.SendGB();
                        }
                    }
                }
            }
	}
        if (dat == 0xff /*||dat==0x00||b==0x00*/) //dat==0xff||dat==0x00
            LinkFirstTime = true;
    }
    return ((dat << 8) | (recvd & (uint8_t)0xff));
}

// Undo a partially-completed InitIPC. release_slot says the shared topology
// already records our slot claim and must be rolled back; that path takes
// the structural lock, so callers must NOT hold a LinkMemGuard when passing
// release_slot = true. The creator tears the whole session down (it is the
// only member); a joiner only detaches.
static void AbortIPCInit([[maybe_unused]] bool firstone, bool release_slot)
{
    if (release_slot && linkmem != NULL) {
        LinkMemGuard guard;
        int f = linkmem->linkflags & ~(1 << vbaid);
        linkmem->linkflags = (uint8_t)f;
        int highest = 0;
        for (int i = 0; i < 4; i++)
            if (f & (1 << i))
                highest = i + 1;
        linkmem->numgbas = (uint8_t)highest;
    }
    LinkAliveRelease();
#if (defined __WIN32__ || defined _WIN32)
    for (int i = 0; i < 4; i++) {
        if (linksync[i] != NULL) {
            CloseHandle(linksync[i]);
            linksync[i] = NULL;
        }
        if (link_doorbell[i] != NULL) {
            CloseHandle(link_doorbell[i]);
            link_doorbell[i] = NULL;
        }
    }
    if (linkmem_lock != NULL) {
        CloseHandle(linkmem_lock);
        linkmem_lock = NULL;
    }
    if (linkmem != NULL) {
        UnmapViewOfFile(linkmem);
        linkmem = NULL;
    }
    if (mmf != NULL) {
        CloseHandle(mmf);
        mmf = NULL;
    }
#elif defined(__ANDROID__)
    // Everything lives in the shared mapping; there is nothing to close
    // per-object, and AndroidLinkShmClose() handles last-out cleanup.
    for (int i = 0; i < 4; i++)
        linksync[i] = NULL;
    linkmem_lock = SEM_FAILED;
    if (linkmem != NULL) {
        linkmem = NULL;
        AndroidLinkShmClose();
    }
#else
    for (int i = 0; i < 4; i++) {
        if (linksync[i] != NULL && linksync[i] != SEM_FAILED) {
            sem_close(linksync[i]);
            if (firstone)
                sem_unlink(LinkSemName(i).c_str());
        }
        linksync[i] = NULL;
        if (link_doorbell[i] != NULL && link_doorbell[i] != SEM_FAILED) {
            sem_close(link_doorbell[i]);
            if (firstone)
                sem_unlink(LinkDoorbellName(i).c_str());
        }
        link_doorbell[i] = NULL;
    }
    link_doorbell_self = -1;
    if (linkmem_lock != SEM_FAILED) {
        sem_close(linkmem_lock);
        if (firstone)
            sem_unlink(LinkLockSemName().c_str());
        linkmem_lock = SEM_FAILED;
    }
    if (linkmem != NULL) {
        munmap(linkmem, sizeof(LINKDATA));
        linkmem = NULL;
    }
    if (mmf >= 0) {
        close(mmf);
        mmf = -1;
    }
    if (firstone)
        shm_unlink(LinkShmName().c_str());
    if (link_liveness_fd >= 0) {
        close(link_liveness_fd);
        link_liveness_fd = -1;
    }
#endif
    vbaid = 0;
    linkid = 0;
}

static ConnectionState InitIPC()
{
    vbaid = 0;
    linkid = 0;

#if (defined __WIN32__ || defined _WIN32)
    if ((mmf = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LINKDATA), LinkShmName().c_str())) == NULL) {
        systemMessage(0, N_("Error creating file mapping"));
        return LINK_ERROR;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        vbaid = 1;
    else
        vbaid = 0;

    if ((linkmem = (LINKDATA*)MapViewOfFile(mmf, FILE_MAP_WRITE, 0, 0, sizeof(LINKDATA))) == NULL) {
        CloseHandle(mmf);
        mmf = NULL;
        systemMessage(0, N_("Error mapping file"));
        return LINK_ERROR;
    }
#elif defined(__ANDROID__)
    if (!AndroidLinkShmOpen()) {
        systemMessage(0, N_("Error creating file mapping"));
        return LINK_ERROR;
    }
    // Same role split as the liveness probe below: the creator is machine
    // 0, everyone else searches for a free slot further down.
    vbaid = android_shm_created ? 0 : 1;
    linkmem = &android_shm->data;
#else
    // Serialize the whole of role selection + segment creation against
    // other instances (CloseIPC's last-one-out probe takes it too). A
    // joiner therefore never sees a half-initialized segment, and two
    // simultaneous starters cannot both become machine 0.
    LinkInitLock init_lock(LinkLockFilePath(".init"));

    const std::string shm_name = LinkShmName();
    bool posix_creator = false;
    link_liveness_fd = open(LinkLockFilePath(".flock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    if (link_liveness_fd >= 0 && init_lock.held()) {
        fchmod(link_liveness_fd, 0666);
        // Nobody holding the liveness lock means no instance is attached,
        // so any existing shm/semaphores are a crashed run's leftovers
        // (they persist until reboot on macOS). Sweep them and start a
        // fresh session rather than joining a corpse.
        posix_creator = (flock(link_liveness_fd, LOCK_EX | LOCK_NB) == 0);
        if (posix_creator) {
            shm_unlink(shm_name.c_str());
            for (int i = 0; i < 4; i++)
                sem_unlink(LinkSemName(i).c_str());
            sem_unlink(LinkLockSemName().c_str());
        }
        mmf = shm_open(shm_name.c_str(), posix_creator ? (O_RDWR | O_CREAT | O_EXCL) : O_RDWR, 0777);
    } else {
        // Degraded fallback (no usable lock file): the historical O_EXCL
        // role heuristic, without stale-crash recovery.
        if (link_liveness_fd >= 0) {
            close(link_liveness_fd);
            link_liveness_fd = -1;
        }
        if ((mmf = shm_open(shm_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0777)) >= 0)
            posix_creator = true;
        else
            mmf = shm_open(shm_name.c_str(), O_RDWR, 0);
    }
    vbaid = posix_creator ? 0 : 1;
    // Only the creator may size the segment; on macOS a second ftruncate
    // on an already-sized shm object fails with EINVAL.
    if (mmf < 0 || (posix_creator && ftruncate(mmf, sizeof(LINKDATA)) < 0)
        || (linkmem = (LINKDATA*)mmap(NULL, sizeof(LINKDATA), PROT_READ | PROT_WRITE, MAP_SHARED, mmf, 0)) == MAP_FAILED) {
        linkmem = NULL;
        AbortIPCInit(posix_creator, false);
        systemMessage(0, N_("Error creating file mapping"));
        return LINK_ERROR;
    }
    if (posix_creator)
        memset(linkmem, 0, sizeof(LINKDATA));
#endif

    // get lowest-numbered available machine slot
    bool firstone = !vbaid;

    // Create/open the structural lock before touching the shared topology.
    // The creator initializes it unlocked (count 1); late joiners open it,
    // retrying briefly because the creator may not have created it yet
    // (only possible in the degraded no-lock-file fallback; under the
    // .init fence the creator finished before any joiner got here).
#if (defined __WIN32__ || defined _WIN32)
    linkmem_lock = firstone
        ? CreateSemaphoreA(NULL, 1, 1, LinkLockSemName().c_str())
        : OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, false, LinkLockSemName().c_str());
    for (int tries = 0; linkmem_lock == NULL && !firstone && tries < 100; tries++) {
        Sleep(2);
        linkmem_lock = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, false, LinkLockSemName().c_str());
    }
#elif defined(__ANDROID__)
    // Lives in the shared mapping; the creator initialized it to count 1
    // before publishing the segment, so it is ready for everyone.
    linkmem_lock = &android_shm->lock;
#else
    if (firstone)
        sem_unlink(LinkLockSemName().c_str()); // drop any stale lock from a crashed run
    linkmem_lock = sem_open(LinkLockSemName().c_str(), firstone ? (O_CREAT | O_EXCL) : 0, 0777, 1);
    for (int tries = 0; linkmem_lock == SEM_FAILED && !firstone && tries < 100; tries++) {
        struct timespec ts = { 0, 2000000 }; // 2 ms
        nanosleep(&ts, NULL);
        linkmem_lock = sem_open(LinkLockSemName().c_str(), 0, 0777, 1);
    }
#endif

    {
        // Slot allocation is a read-modify-write on the shared topology
        // (numgbas/linkflags); serialize it so two instances starting at
        // the same time cannot claim the same vbaid. (Old FIXME here.)
        LinkMemGuard guard;
        if (firstone) {
            linkmem->linkflags = 1;
            linkmem->numgbas = 1;
            linkmem->numtransfers = 0;
            for (int i = 0; i < 5; i++)
                linkmem->linkdata[i] = 0xffff;
            for (int i = 0; i < 4; i++)
                linkmem->core_clock[i] = 0;
        } else {
            int n = linkmem->numgbas;
            int f = linkmem->linkflags;
            for (int i = 0; i <= n; i++)
                if (!(f & (1 << i))) {
                    vbaid = i;
                    break;
                }
            if (vbaid == 4) {
                // Release the structural lock by hand before AbortIPCInit
                // closes it; the guard's destructor then finds it gone and
                // does nothing.
                LinkMemUnlock(guard.held());
                AbortIPCInit(firstone, false);
                systemMessage(0, N_("5 or more GBAs not supported."));
                return LINK_ERROR;
            }
            if (vbaid == n)
                linkmem->numgbas = (uint8_t)(n + 1);
            linkmem->linkflags = (uint8_t)(f | (1 << vbaid));
            // Don't inherit a previous occupant's published GP pin state.
            linkmem->gp_rcnt[vbaid] = 0;
            // Join the CPU-rate lockstep in sync with the incumbents: a
            // fresh clock of 0 would read as "hours behind", making every
            // running peer stall against us (or us free-run against them).
            uint32_t max_clock = 0;
            for (int i = 0; i < 4; i++)
                if (i != vbaid && (f & (1 << i))
                    && (int32_t)(linkmem->core_clock[i] - max_clock) > 0)
                    max_clock = linkmem->core_clock[i];
            linkmem->core_clock[vbaid] = max_clock;
        }
    }
    linkid = (uint16_t)vbaid;
    link_doorbell_self = (int)linkid;
    // Publish our per-slot liveness token before any peer could start
    // waiting on us (see LinkPeerAlive).
    LinkAliveAcquire(vbaid);
    GpResetState();

    // The GB serial path is strictly 2-player: it indexes linkcmd/linkdata/
    // linksync with 1 - linkid, so a third instance would index [-1].
    if (linkDriver != NULL && linkDriver->mode == LINK_GAMEBOY_IPC && vbaid > 1) {
        AbortIPCInit(firstone, true);
        systemMessage(0, N_("The GB link supports only 2 players."));
        return LINK_ERROR;
    }

#if defined(__ANDROID__)
    // The handshake semaphores are part of the shared mapping; there is
    // nothing to open, and no stale named object to clean up.
    for (int i = 0; i < 4; i++) {
        linksync[i] = &android_shm->sync[i];
        // The shm layout is fixed, so there is no doorbell on Android; the
        // throttle keeps its blind nap there.
        link_doorbell[i] = NULL;
    }
#else
    for (int i = 0; i < 4; i++) {
#if (defined __WIN32__ || defined _WIN32)
        linksync[i] = firstone ? CreateSemaphoreA(NULL, 0, 4, LinkSemName(i).c_str())
                               : OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, false, LinkSemName(i).c_str());
        if (linksync[i] == NULL) {
            AbortIPCInit(firstone, !firstone);
            systemMessage(0, N_("Error opening event"));
            return LINK_ERROR;
        }
        // Best-effort doorbell: on failure the throttle falls back to the
        // blind nap, nothing else depends on it.
        link_doorbell[i] = firstone ? CreateSemaphoreA(NULL, 0, 16, LinkDoorbellName(i).c_str())
                                    : OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, false, LinkDoorbellName(i).c_str());
#else
        if (firstone) {
            // remove any stale semaphore left over from a crashed instance
            // (redundant after the liveness sweep; still needed in the
            // degraded no-lock-file fallback)
            sem_unlink(LinkSemName(i).c_str());
            sem_unlink(LinkDoorbellName(i).c_str());
        }
        if ((linksync[i] = sem_open(LinkSemName(i).c_str(),
                 firstone ? O_CREAT | O_EXCL : 0,
                 0777, 0))
            == SEM_FAILED) {
            linksync[i] = NULL;
            AbortIPCInit(firstone, !firstone);
            systemMessage(0, N_("Error opening event"));
            return LINK_ERROR;
        }
        // Best-effort doorbell: on failure the throttle falls back to the
        // blind nap, nothing else depends on it.
        if ((link_doorbell[i] = sem_open(LinkDoorbellName(i).c_str(),
                 firstone ? O_CREAT | O_EXCL : 0,
                 0777, 0))
            == SEM_FAILED)
            link_doorbell[i] = NULL;
#endif
    }
#endif  // defined(__ANDROID__)

#if !(defined __WIN32__ || defined _WIN32) && !defined(__ANDROID__)
    // Hold the session-long shared liveness lock. The creator converts its
    // exclusive probe lock; safe under the init lock, since no other
    // instance can be probing right now.
    if (link_liveness_fd >= 0)
        flock(link_liveness_fd, LOCK_SH);
#endif

    return LINK_OK;
}

static void StartCableIPC(uint16_t value)
{
    switch (GetSIOMode(value, READ16LE(&g_ioMem[COMM_RCNT]))) {
    case MULTIPLAYER: {
        bool start = (value & 0x80) && !linkid && !transfer_direction;
        // clear start, seqno, si (RO on slave, start = pulse on master)
        value &= 0xff4b;
        // get current si.  This way, on slaves, it is low during xfer
        if (linkid) {
            if (!transfer_direction)
                value |= 4;
            else
                value |= READ16LE(&g_ioMem[COMM_SIOCNT]) & 4;
        }
        if (start) {
            // Reading the topology (numgbas/linkflags) to pick trgbas and
            // then publishing numtransfers/lastlinktime is a read-modify-
            // write shared with joining/leaving instances; serialize it.
            // The linksync drain below uses a zero timeout (non-blocking),
            // so the lock is never held across a blocking wait.
            LinkMemGuard guard;
            if (linkmem->numgbas > 1) {
                // find first active attached GBA
                // doing this first reduces the potential
                // race window size for new connections
                int n = linkmem->numgbas + 1;
                int f = linkmem->linkflags;
                int m;
                do {
                    n--;
                    m = (1 << n) - 1;
                } while ((f & m) != m);
                linkmem->trgbas = (uint8_t)n;

                // before starting xfer, make pathetic attempt
                // at clearing out any previous stuck xfer
                // this will fail if a slave was stuck for
                // too long
                for (int i = 0; i < 4; i++)
                    while (WaitForSingleObject(linksync[i], 0) != WAIT_TIMEOUT)
                        ;

                // transmit first value
                linkmem->linkcmd[0] = ('M' << 8) + (value & 3);
                linkmem->linkdata[0] = READ16LE(&g_ioMem[COMM_SIODATA8]);

                // start up slaves & sync clocks
                numtransfers = linkmem->numtransfers;
                if (numtransfers != 0)
                    linkmem->lastlinktime = linktime;
                else
                    linkmem->lastlinktime = 0;

                if ((++numtransfers) == 0)
                    linkmem->numtransfers = 2;
                else
                    linkmem->numtransfers = numtransfers;

                // Ring every live peer's doorbell: a peer napping in the
                // ahead-throttle wakes immediately instead of discovering
                // the new transfer at its next nap boundary.
                for (int i = 0; i < 4; i++) {
                    if (i == (int)linkid || !(linkmem->linkflags & (1 << i)))
                        continue;
                    if (link_doorbell[i] != NULL)
#if (defined __WIN32__ || defined _WIN32)
                        ReleaseSemaphore(link_doorbell[i], 1, NULL);
#else
                        sem_post(link_doorbell[i]);
#endif
                }

                transfer_direction = 1;
                linktime = 0;
                tspeed = value & 3;
                WRITE32LE(&g_ioMem[COMM_SIOMULTI0], 0xffffffff);
                WRITE32LE(&g_ioMem[COMM_SIOMULTI2], 0xffffffff);
                value &= ~0x40;
                CableTrace("ipc[%d] start #%u data=%04x speed=%d trgbas=%d lastlinktime=%d",
                    linkid, (unsigned)numtransfers, linkmem->linkdata[0], tspeed,
                    linkmem->trgbas, linkmem->lastlinktime);
            } else {
                // No partner in the session. Real hardware still clocks a
                // lone master's transfer out: its own word shows up in
                // SIOMULTI0, the absent slots read back 0xffff, the error
                // bit is set (SI stays high with no slaves), and the
                // completion IRQ fires when the transfer time elapses.
                // Setting only the error bit here left a game that starts
                // a probe transfer and IntrWaits on the serial IRQ hanging
                // at a white screen until a second instance appeared.
                UPDATE_REG(COMM_SIOMULTI0, READ16LE(&g_ioMem[COMM_SIODATA8]));
                UPDATE_REG(COMM_SIOMULTI1, 0xffff);
                WRITE32LE(&g_ioMem[COMM_SIOMULTI2], 0xffffffff);
                value |= 0x40; // comm error: no slaves attached
                value |= 0x80; // busy until the scheduled completion
                CableTrace("ipc[%d] lone-master start data=%04x", linkid,
                    READ16LE(&g_ioMem[COMM_SIODATA8]));
                CPUScheduleSioCompletion(trtimedata[0][value & 3]);
            }
        }
        value |= (transfer_direction != 0) << 7;
        value |= (linkid && !transfer_direction ? 0xc : 8); // set SD (high), SI (low on master)
        value |= linkid << 4; // set seq
        UPDATE_REG(COMM_SIOCNT, value);
        if (linkid)
            // SC low -> transfer in progress
            // not sure why SO is low
            UPDATE_REG(COMM_RCNT, transfer_direction ? 6 : 7);
        else
            // SI is always low on master
            // SO, SC always low during transfer
            // not sure why SO low otherwise
            UPDATE_REG(COMM_RCNT, transfer_direction ? 2 : 3);
        break;
    }
    case NORMAL8:
    case NORMAL32:
    case UART:
    default:
        UPDATE_REG(COMM_SIOCNT, value);
        break;
    }
}

// Re-add this instance to the shared topology after it was dropped.
// Guards its own linkflags/numgbas read-modify-write; callers (in
// UpdateCableIPC) must NOT already hold the lock, since it is not
// recursive.
static void ReconnectCableIPC()
{
    LinkMemGuard guard;
    int f = linkmem->linkflags;
    int n = linkmem->numgbas;
    if (f & (1 << linkid)) {
        systemMessage(0, N_("Lost link; reinitialize to reconnect"));
        return;
    }
    linkmem->linkflags |= 1 << linkid;
    if (n < linkid + 1)
        linkmem->numgbas = (uint8_t)(linkid + 1);
    numtransfers = linkmem->numtransfers;
    // Rejoin the CPU-rate lockstep in sync with the peers (see InitIPC).
    uint32_t max_clock = 0;
    for (int i = 0; i < 4; i++)
        if (i != (int)linkid && (f & (1 << i))
            && (int32_t)(linkmem->core_clock[i] - max_clock) > 0)
            max_clock = linkmem->core_clock[i];
    linkmem->core_clock[linkid] = max_clock;
    systemScreenMessage(_("Lost link; reconnected"));
}

// The lockstep pacing tolerates kMaxLinkLeadTicks (two frames) of skew, so the
// shared clock does not need per-tick precision. Publishing it on every
// LinkUpdate wrote a single shared cache line (all four core_clock[] slots
// share one line) ~once per emulated tick; on multi-core that line ping-pongs
// between the two instances' cores continuously (write own slot + read peers'
// slots = false + true sharing) and shows up as a large linked framerate drop.
// Keep the clock local and flush only when the peer actually needs it.
static uint32_t s_ipc_unpublished_ticks = 0;
static const uint32_t kClockPublishTicks = 1024; // << kMaxLinkLeadTicks; tunable

static void UpdateCableIPC(int ticks)
{
    // CPU-rate lockstep. Accumulate our emulated clock locally and only flush
    // it to shared memory when the peer needs our progress: in/entering a
    // transfer, a pending detection, or once we have advanced a meaningful
    // fraction of the two-frame pacing tolerance. Between those the shared
    // cache line stays cold. Runs in every RCNT mode (before the JOY/GP
    // early-outs) so pacing holds across mode probes too.
    s_ipc_unpublished_ticks += (uint32_t)ticks;
    const bool need_publish = transfer_direction
        || linkmem->numtransfers != numtransfers
        || s_ipc_unpublished_ticks >= kClockPublishTicks;
    if (need_publish) {
        linkmem->core_clock[linkid] += s_ipc_unpublished_ticks;
        s_ipc_unpublished_ticks = 0;
        if (transfer_direction)
            last_hot_own_clock = linkmem->core_clock[linkid];
        const uint32_t mine = linkmem->core_clock[linkid];
        const bool link_hot = linkmem->numgbas > 1
            && (int32_t)(mine - last_hot_own_clock) < kHotWindowTicks;
        if (link_hot && !link_was_hot) {
            // Cold -> hot: rebase the lead measurement (see hot_base_own).
            hot_base_own = mine;
            for (int i = 0; i < 4; i++)
                hot_base_peer[i] = linkmem->core_clock[i];
        }
        link_was_hot = link_hot;
        // Never throttle while local progress is needed to serve the link: an
        // in-flight transfer (transfer_direction) or one pending detection
        // (numtransfers changed) must be reached at full speed, or the peer's
        // wait for our reply would be starved by our own pacing.
        if (link_hot && !transfer_direction
            && linkmem->numtransfers == numtransfers) {
            const int32_t my_progress = (int32_t)(mine - hot_base_own);
            const int f = linkmem->linkflags;
            int32_t worst_lead = 0;
            for (int i = 0; i < 4; i++) {
                if (i == (int)linkid || !(f & (1 << i)))
                    continue;
                const int32_t lead = my_progress
                    - (int32_t)(linkmem->core_clock[i] - hot_base_peer[i]);
                if (lead > worst_lead)
                    worst_lead = lead;
            }
            if (worst_lead > kMaxLinkLeadTicks)
                (void)LinkAheadThrottleStep();
            else
                ahead_throttle_budget_us = kAheadThrottleBudgetUs;
        }
    }

    const uint16_t rcnt = READ16LE(&g_ioMem[COMM_RCNT]);
    if ((rcnt >> 14) == 3) // JOY mode: pins belong to the JoyBus engine
        return;
    if ((rcnt >> 14) == 2) { // GP mode: GPIO exchange, no transfers
        GpIpcUpdate();
        return;
    }

    // slave startup depends on detecting change in numtransfers
    // and syncing clock with master (after first transfer)
    if (!transfer_direction && linktime < 0) {
        // linktime overflows negative after ~128 s of emulated time with no
        // transfer. This clamp used to be gated on numtransfers != 0, so the
        // overflow went unhandled before a session's FIRST transfer --
        // leaving the start gate below unsatisfiable for the next ~128 s. A
        // game that opens the link minutes into the session (Pokémon at the
        // Cable Club) found a dead cable half the time.
        CableTrace("ipc[%d] idle-overflow reset (linktime<0)", linkid);
        linktime = 0;
        if (numtransfers) {
            // there is a very, very, small chance that this will abort
            // a transfer that was just started
            linkmem->numtransfers = numtransfers = 0;
        }
    }
    // If our clock is impossibly far behind the master's published start
    // time (asymmetric stall: one instance paused or backgrounded, or the
    // overflow clamp above fired on one side only), waiting out the
    // difference just serves dead air while the master blocks on our
    // reply; resync and start immediately instead.
    if (linkid && !transfer_direction && linkmem->numtransfers != numtransfers
        && linkmem->lastlinktime - linktime > kMaxLinkClockLagTicks) {
        CableTrace("ipc[%d] clock-lag resync linktime=%d lastlinktime=%d",
            linkid, linktime, linkmem->lastlinktime);
        linktime = linkmem->lastlinktime;
    }
    if (linkid && !transfer_direction && linktime >= linkmem->lastlinktime && linkmem->numtransfers != numtransfers) {
        numtransfers = linkmem->numtransfers;
        if (!numtransfers)
            return;
        CableTrace("ipc[%d] slave detect #%u linktime=%d lastlinktime=%d trgbas=%d",
            linkid, (unsigned)numtransfers, linktime, linkmem->lastlinktime,
            linkmem->trgbas);

        // if this or any previous machine was dropped, no transfer
        // can take place
        if (linkmem->trgbas <= linkid) {
            transfer_direction = 0;
            numtransfers = 0;
            // if this is the one that was dropped, reconnect
            if (!(linkmem->linkflags & (1 << linkid)))
                ReconnectCableIPC();
            return;
        }

        // sync clock
        if (numtransfers == 1)
            linktime = 0;
        else
            linktime -= linkmem->lastlinktime;
        // Cap the leftover: residue above one transfer slot pre-satisfies
        // every pacing gate below, so a run of pending transfers commits
        // back-to-back with almost no emulated time between serial IRQs --
        // the game's ISR (a few thousand cycles) then misses words, which
        // corrupts block transfers and trips the game's checksum ("link
        // error" mid-trade). The wx GUI's bursty frame pacing builds
        // exactly this residue (observed steady-state ~2 frames); bounding
        // it makes the slave spend the master's inter-transfer gap in its
        // OWN emulated time, the way real hardware does. The socket slave
        // already gets this for free by zeroing linktime at every send.
        // (linkcmd holds this transfer's speed; tspeed is assigned from it
        // a few lines below.)
        if (linktime > trtimedata[0][linkmem->linkcmd[0] & 3])
            linktime = trtimedata[0][linkmem->linkcmd[0] & 3];

        // 'M' (multiplayer start) is the only possible command here.
        tspeed = linkmem->linkcmd[0] & 3;
        transfer_direction = 1;
        ahead_throttle_budget_us = kAheadThrottleBudgetUs;
        WRITE32LE(&g_ioMem[COMM_SIOMULTI0], 0xffffffff);
        WRITE32LE(&g_ioMem[COMM_SIOMULTI2], 0xffffffff);
        UPDATE_REG(COMM_SIOCNT, (READ16LE(&g_ioMem[COMM_SIOCNT]) & ~0x40) | 0x80);
    }

    if (!transfer_direction)
        return;

    if (transfer_direction <= linkmem->trgbas && linktime >= trtimedata[transfer_direction - 1][tspeed]) {
        // transfer #n -> wait for value n - 1
        if (transfer_direction > 1 && linkid != transfer_direction - 1) {
            // Wait out peer stalls instead of dropping on the first missed
            // linktimeout: a GUI instance can stop emulating indefinitely
            // for completely healthy reasons (occluded window, drag,
            // dialog, backgrounded app), and the old one-shot timeout
            // rewrote the shared topology on the first miss --
            // unrecoverable for the game even though the peer came right
            // back. The wait keeps going for as long as the peer is still
            // a session member (its linkflags bit is set) and its process
            // exists; one that crashed with its flag up is caught by the
            // per-slot liveness probe (see LinkPeerAlive), never by a
            // wall-clock deadline.
            if (!WaitForLinkToken(transfer_direction - 1,
                    (uint8_t)(1 << (transfer_direction - 1)))) {
                CableTrace("ipc[%d] slot-%d wait abandoned, dropping player %d",
                    linkid, transfer_direction, transfer_direction - 1);
                // assume slave has dropped off if timed out
                if (!linkid) {
                    // Dropping a slave rewrites the shared topology
                    // (trgbas/linkflags/numgbas); serialize that RMW.
                    LinkMemGuard guard;
                    linkmem->trgbas = (uint8_t)(transfer_direction - 1);
                    int f = linkmem->linkflags;
                    f &= ~(1 << (transfer_direction - 1));
                    linkmem->linkflags = (uint8_t)f;
                    if (f < (1 << transfer_direction) - 1)
                        linkmem->numgbas = (uint8_t)(transfer_direction - 1);
                    char message[30];
                    snprintf(message, sizeof(message), _("Player %d disconnected."), transfer_direction - 1);
                    systemScreenMessage(message);
                }
                transfer_direction = linkmem->trgbas + 1;
                // next cycle, transfer will finish up
                return;
            }
        }
        // now that value is available, store it
        UPDATE_REG((COMM_SIOMULTI0 - 2) + (transfer_direction << 1), linkmem->linkdata[transfer_direction - 1]);

        // transfer machine's value at start of its transfer cycle
        if (linkid == transfer_direction) {
            // skip if dropped
            if (linkmem->trgbas <= linkid) {
                transfer_direction = 0;
                numtransfers = 0;
                // if this is the one that was dropped, reconnect
                if (!(linkmem->linkflags & (1 << linkid)))
                    ReconnectCableIPC();
                return;
            }
            // SI becomes low
            UPDATE_REG(COMM_SIOCNT, READ16LE(&g_ioMem[COMM_SIOCNT]) & ~4);
            UPDATE_REG(COMM_RCNT, 10);
            linkmem->linkdata[linkid] = READ16LE(&g_ioMem[COMM_SIODATA8]);
            CableTrace("ipc[%d] slave latch data=%04x linktime=%d", linkid,
                linkmem->linkdata[linkid], linktime);
            // The waiters this transfer are the trgbas participants minus
            // the sender; posting numgbas - 1 leaks tokens whenever a
            // machine joined or dropped mid-session (numgbas > trgbas),
            // pre-satisfying a rejoining slave's next wait.
            ReleaseSemaphore(linksync[linkid], linkmem->trgbas - 1, NULL);
        }
        if (linkid == transfer_direction - 1) {
            // SO becomes low to begin next trasnfer
            // may need to set DDR as well
            UPDATE_REG(COMM_RCNT, 0x22);
        }

        // next cycle; this is a 1-based transfer counter here, not a
        // SENDING/RECEIVING flag (see the > trgbas completion check below)
        transfer_direction++;
    }

    // trgbas can collapse to 1 after a drop, making transfer_direction 2
    // here; clamp so the lookups below can't index trtimeend[-1].
    int tt = transfer_direction - 3;
    if (tt < 0)
        tt = 0;
    if (transfer_direction > linkmem->trgbas && linktime >= trtimeend[tt][tspeed]) {
        // wait for slaves to finish
        // this keeps unfinished slaves from screwing up last xfer
        // not strictly necessary; may just slow things down
        if (!linkid) {
            for (int i = 2; i < transfer_direction; i++) {
                // Same stall-tolerant wait as the slot wait above: don't
                // reset a healthy session over one transient stall. Any
                // still-flagged slave keeps the wait alive.
                if (!WaitForLinkToken(0,
                        (uint8_t)(linkmem->linkflags & ~1u))) {
                    CableTrace("ipc[%d] completion wait abandoned, resetting comm",
                        linkid);
                    // impossible to determine which slave died
                    // so leave them alone for now
                    systemScreenMessage(_("Unknown slave timed out; resetting comm"));
                    linkmem->numtransfers = numtransfers = 0;
                    break;
                }
            }
        } else if (linkmem->trgbas > linkid)
            // signal master that this slave is finished
            ReleaseSemaphore(linksync[0], 1, NULL);
        linktime -= trtimeend[tt][tspeed];
        transfer_direction = 0;
        uint16_t value = READ16LE(&g_ioMem[COMM_SIOCNT]);
        // SI returns high on SLAVES after the transfer (the previous
        // device's SO idles high); the master's SI pin is hardwired low.
        // This condition used to be inverted (!linkid), which set SI on the
        // master after every transfer -- and Pokémon's per-frame master
        // election (master ⇔ SD=1, SI=0, ID=0) then demoted the master, so
        // no second transfer was ever started: the Cable Club sat on
        // "Please wait." forever after exactly one exchange.
        if (linkid)
            value |= 4;
        else
            value &= ~4;
        UPDATE_REG(COMM_SIOCNT, (value & 0xff0f) | (linkid << 4));
        // SC/SI high after transfer
        UPDATE_REG(COMM_RCNT, linkid ? 15 : 11);
        CableTrace("ipc[%d] commit M0=%04x M1=%04x M2=%04x M3=%04x irq=%d",
            linkid, READ16LE(&g_ioMem[COMM_SIOMULTI0]),
            READ16LE(&g_ioMem[COMM_SIOMULTI1]),
            READ16LE(&g_ioMem[COMM_SIOMULTI2]),
            READ16LE(&g_ioMem[COMM_SIOMULTI3]), (value & 0x4000) ? 1 : 0);
        if (value & 0x4000)
            CPURaiseSioIRQ();
    }
}

// The GBA wireless RFU (see adapter3.txt)
static void StartRFU(uint16_t value)
{
    int siomode = GetSIOMode(value, READ16LE(&g_ioMem[COMM_RCNT]));

    if (value)
        rfu_enabled = (siomode == NORMAL32);

    if (((READ16LE(&g_ioMem[COMM_SIOCNT]) & 0x5080) == 0x1000) && ((value & 0x5080) == 0x5080)) { //RFU Reset, may also occur before cable link started
        //log("RFU Reset2 : %04X  %04X  %d\n", READ16LE(&g_ioMem[COMM_RCNT]), READ16LE(&g_ioMem[COMM_SIOCNT]), GetTickCount());
        linkmem->rfu_listfront[vbaid] = 0;
        linkmem->rfu_listback[vbaid] = 0;
    }

    if (!rfu_enabled) {
        if ((value & 0x5080) == 0x5080) { //0x5083 //game tried to send wireless command but w/o the adapter
            /*if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
			value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
			else //(Bit.3, 0=Enable Transfer/Ready)
			value |= 4; //bit.2=1 (otherside is Not Ready)*/
            if (READ16LE(&g_ioMem[COMM_SIOCNT]) & 0x4000) //IRQ Enable
            {
                CPURaiseSioIRQ(); //Serial Communication
            }
            value &= 0xff7f; //Start bit.7 reset //may cause the game to retry sending again
            //value |= 0x0008; //SO bit.3 set automatically upon transfer completion
            transfer_direction = 0;
        }
        return;
    }

    linktimeout = 1;

    uint32_t CurCOM = 0, CurDAT = 0;
    // bool rfulogd = (READ16LE(&g_ioMem[COMM_SIOCNT]) != value);

    switch (GetSIOMode(value, READ16LE(&g_ioMem[COMM_RCNT]))) {
    case NORMAL8:
        rfu_polarity = 0;
        UPDATE_REG(COMM_SIOCNT, value);
        return;
        break;
    case NORMAL32:
        //don't do anything if previous cmd aren't sent yet, may fix Boktai2 Not Detecting wireless adapter
        if (transfer_direction) {
            UPDATE_REG(COMM_SIOCNT, value);
            return;
        }

        //Moving this to the bottom might prevent Mario Golf Adv from Occasionally Not Detecting wireless adapter
        if (value & 8) //Transfer Enable Flag Send (SO.bit.3, 1=Disable Transfer/Not Ready)
            value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, SI.bit.2=SO.bit.3 of otherside)	// A kind of acknowledge procedure
        else //(SO.Bit.3, 0=Enable Transfer/Ready)
            value |= 4; //SI.bit.2=1 (otherside is Not Ready)

        if ((value & 5) == 1)
            value |= 0x02; //wireless always use 2Mhz speed right? this will fix MarioGolfAdv Not Detecting wireless

        if (value & 0x80) //start/busy bit
        {
            if ((value & 3) == 1)
                rfu_transfer_end = 2048;
            else
                rfu_transfer_end = 256;
            uint16_t siodata_h = READ16LE(&g_ioMem[COMM_SIODATA32_H]);
            RfuTrace("ipc st=%d out=%08X cnt=%04X cmd=%02X pol=%d",
                rfu_state, READ32LE(&g_ioMem[COMM_SIODATA32_L]), value,
                rfu_cmd, rfu_polarity);
            switch (rfu_state) {
            case RFU_INIT:
                if (READ32LE(&g_ioMem[COMM_SIODATA32_L]) == 0xb0bb8001) {
                    rfu_state = RFU_COMM; // end of startup
                    rfu_initialized = true;
                    value &= 0xfffb; //0xff7b; //Bit.2 need to be 0 to indicate a finished initialization to fix MarioGolfAdv from occasionally Not Detecting wireless adapter (prevent it from sending 0x7FFE8001 comm)?
                    rfu_polarity = 0; //not needed?
                }
                rfu_buf = (READ16LE(&g_ioMem[COMM_SIODATA32_L]) << 16) | siodata_h;
                break;
            case RFU_COMM:
                CurCOM = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                if (siodata_h == 0x9966) //initialize cmd
                {
                    uint8_t tmpcmd = (uint8_t)CurCOM;
                    if (tmpcmd != 0x10 && tmpcmd != 0x11 && tmpcmd != 0x13 && tmpcmd != 0x14 && tmpcmd != 0x16 && tmpcmd != 0x17 && tmpcmd != 0x19 && tmpcmd != 0x1a && tmpcmd != 0x1b && tmpcmd != 0x1c && tmpcmd != 0x1d && tmpcmd != 0x1e && tmpcmd != 0x1f && tmpcmd != 0x20 && tmpcmd != 0x21 && tmpcmd != 0x24 && tmpcmd != 0x25 && tmpcmd != 0x26 && tmpcmd != 0x27 && tmpcmd != 0x30 && tmpcmd != 0x32 && tmpcmd != 0x33 && tmpcmd != 0x34 && tmpcmd != 0x3d && tmpcmd != 0xa8 && tmpcmd != 0xee) {
                        log("%08X : UnkCMD %08X  %04X  %08X %08X\n", GetTickCount(), CurCOM, PrevVAL, PrevCOM, PrevDAT);
                    }
                    rfu_counter = 0;
                    if ((rfu_qsend2 = rfu_qsend = g_ioMem[0x121]) != 0) { //COMM_SIODATA32_L+1, following data [to send]
                        rfu_state = RFU_SEND;
                    }
                    if (g_ioMem[COMM_SIODATA32_L] == 0xee) { //0xee cmd shouldn't override previous cmd
                        rfu_lastcmd = rfu_cmd2;
                        rfu_cmd2 = g_ioMem[COMM_SIODATA32_L];
                        //rfu_polarity = 0; //when polarity back to normal the game can initiate a new cmd even when 0xee hasn't been finalized, but it looks improper isn't?
                    } else {
                        rfu_lastcmd = rfu_cmd;
                        rfu_cmd = g_ioMem[COMM_SIODATA32_L];
                        rfu_cmd2 = 0;
                        if (rfu_cmd == 0x27 || rfu_cmd == 0x37) {
                            rfu_lastcmd2 = rfu_cmd;
                            rfu_lasttime = GetTickCount();
                        } else if (rfu_cmd == 0x24) { //non-important data shouldn't overwrite important data from 0x25
                            rfu_lastcmd2 = rfu_cmd;
                            rfu_cansend = false;
                            //previous important data need to be received successfully before sending another important data
                            rfu_lasttime = GetTickCount(); //just to mark the last time a data being sent
                            if (!speedhack) {
                                while (linkmem->numgbas >= 2 && linkmem->rfu_q[vbaid] > 1 && vbaid != gbaid && linkmem->rfu_signal[vbaid] && linkmem->rfu_signal[gbaid] && (GetTickCount() - rfu_lasttime) < (uint32_t)linktimeout) {
                                    if (!rfu_ishost)
                                        SetEvent(linksync[gbaid]);
                                    else //unlock other gba, allow other gba to move (sending their data)  //is max value of vbaid=1 ?
                                        for (int j = 0; j < linkmem->numgbas; j++)
                                            if (j != vbaid)
                                                SetEvent(linksync[j]);
                                    WaitForSingleObject(linksync[vbaid], 1); //linktimeout //wait until this gba allowed to move (to prevent both GBAs from using 0x25 at the same time)
                                    ResetEvent(linksync[vbaid]); //lock this gba, don't allow this gba to move (prevent sending another data too fast w/o giving the other side chances to read it)
                                    if (!rfu_ishost && linkmem->rfu_is_host[vbaid]) {
                                        linkmem->rfu_is_host[vbaid] = 0;
                                        break;
                                    } //workaround for a bug where rfu_request failed to reset when GBA act as client
                                }
                            }
                            //SetEvent(linksync[vbaid]); //set again to reduce the lag since it will be waited again during finalization cmd
                            else {
                                if (linkmem->numgbas >= 2 && gbaid != vbaid && linkmem->rfu_q[vbaid] > 1 && linkmem->rfu_signal[vbaid] && linkmem->rfu_signal[gbaid]) {
                                    if (!rfu_ishost)
                                        SetEvent(linksync[gbaid]);
                                    else //unlock other gba, allow other gba to move (sending their data)  //is max value of vbaid=1 ?
                                        for (int j = 0; j < linkmem->numgbas; j++)
                                            if (j != vbaid)
                                                SetEvent(linksync[j]);
                                    WaitForSingleObject(linksync[vbaid], speedhack ? 1 : linktimeout); //wait until this gba allowed to move
                                    ResetEvent(linksync[vbaid]); //lock this gba, don't allow this gba to move (prevent sending another data too fast w/o giving the other side chances to read it)
                                }
                            }
                            if (linkmem->rfu_q[vbaid] < 2) { //can overwrite now
                                rfu_cansend = true;
                                linkmem->rfu_q[vbaid] = 0; //rfu_qsend;
                                linkmem->rfu_qid[vbaid] = 0;
                            } else if (!speedhack)
                                rfu_waiting = true; //don't wait with speedhack
                        } else if (rfu_cmd == 0x25 || rfu_cmd == 0x35) {
                            rfu_lastcmd2 = rfu_cmd;
                            rfu_cansend = false;
                            //previous important data need to be received successfully before sending another important data
                            rfu_lasttime = GetTickCount();
                            if (!speedhack) {
                                //2 players connected
                                while (linkmem->numgbas >= 2 && linkmem->rfu_q[vbaid] > 1 && vbaid != gbaid && linkmem->rfu_signal[vbaid] && linkmem->rfu_signal[gbaid] && (GetTickCount() - rfu_lasttime) < (uint32_t)linktimeout) {
                                    if (!rfu_ishost)
                                        SetEvent(linksync[gbaid]); //unlock other gba, allow other gba to move (sending their data)  //is max value of vbaid=1 ?
                                    else
                                        for (int j = 0; j < linkmem->numgbas; j++)
                                            if (j != vbaid)
                                                SetEvent(linksync[j]);
                                    WaitForSingleObject(linksync[vbaid], 1); //linktimeout //wait until this gba allowed to move (to prevent both GBAs from using 0x25 at the same time)
                                    ResetEvent(linksync[vbaid]); //lock this gba, don't allow this gba to move (prevent sending another data too fast w/o giving the other side chances to read it)
                                    if (!rfu_ishost && linkmem->rfu_is_host[vbaid]) {
                                        linkmem->rfu_is_host[vbaid] = 0;
                                        break;
                                    } //workaround for a bug where rfu_request failed to reset when GBA act as client
                                }
                            }
                            //SetEvent(linksync[vbaid]); //set again to reduce the lag since it will be waited again during finalization cmd
                            else {
                                //2 players connected
                                if (linkmem->numgbas >= 2 && gbaid != vbaid && linkmem->rfu_q[vbaid] > 1 && linkmem->rfu_signal[vbaid] && linkmem->rfu_signal[gbaid]) {
                                    if (!rfu_ishost)
                                        SetEvent(linksync[gbaid]);
                                    else //unlock other gba, allow other gba to move (sending their data)  //is max value of vbaid=1 ?
                                        for (int j = 0; j < linkmem->numgbas; j++)
                                            if (j != vbaid)
                                                SetEvent(linksync[j]);
                                    WaitForSingleObject(linksync[vbaid], speedhack ? 1 : linktimeout); //wait until this gba allowed to move
                                    ResetEvent(linksync[vbaid]); //lock this gba, don't allow this gba to move (prevent sending another data too fast w/o giving the other side chances to read it)
                                }
                            }
                            if (linkmem->rfu_q[vbaid] < 2) {
                                rfu_cansend = true;
                                linkmem->rfu_q[vbaid] = 0; //rfu_qsend;
                                linkmem->rfu_qid[vbaid] = 0; //don't wait with speedhack
                            } else if (!speedhack)
                                rfu_waiting = true;
                        } else if (rfu_cmd == 0xa8 || rfu_cmd == 0xb6) {
                            //wait for [important] data when previously sent is important data, might only need to wait for the 1st 0x25 cmd
                            // bool ok = false;
                        } else if (rfu_cmd == 0x11 || rfu_cmd == 0x1a || rfu_cmd == 0x26) {
                            if (rfu_lastcmd2 == 0x24)
                                rfu_waiting = true;
                        }
                    }
                    if (rfu_waiting)
                        rfu_buf = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    else
                        rfu_buf = 0x80000000;
                } else if (siodata_h == 0x8000) //finalize cmd, the game will send this when polarity reversed (expecting something)
                {
                    rfu_qrecv_broadcast_data_len = 0;
                    if (rfu_cmd2 == 0xee) {
                        if (rfu_masterdata[0] == 2) //is this value of 2 related to polarity?
                            rfu_polarity = 0; //to normalize polarity after finalize looks more proper
                        rfu_buf = 0x99660000 | (rfu_qrecv_broadcast_data_len << 8) | (rfu_cmd2 ^ 0x80);
                    } else {
                        switch (rfu_cmd) {
                        case 0x1a: // check if someone joined
                            if (linkmem->rfu_is_host[vbaid]) {
                                gbaidx = gbaid;
                                do {
                                    gbaidx = (gbaidx + 1) % linkmem->numgbas;
                                    if (gbaidx != vbaid && linkmem->rfu_reqid[gbaidx] == (vbaid << 3) + 0x61f1)
                                        rfu_masterdata[rfu_qrecv_broadcast_data_len++] = (gbaidx << 3) + 0x61f1;
                                    log("qrecv++ %d\n", rfu_qrecv_broadcast_data_len);
                                } while (gbaidx != gbaid && linkmem->numgbas >= 2);
                                if (rfu_qrecv_broadcast_data_len > 0) {
                                    bool ok = false;
                                    for (int i = 0; i < rfu_numclients; i++)
                                        if ((rfu_clientlist[i] & 0xffff) == rfu_masterdata[0]) {
                                            ok = true;
                                            break;
                                        }
                                    if (!ok) {
                                        rfu_curclient = rfu_numclients;
                                        linkmem->rfu_clientidx[(rfu_masterdata[0] - 0x61f1) >> 3] = rfu_numclients;
                                        rfu_clientlist[rfu_numclients] = rfu_masterdata[0] | (rfu_numclients << 16);
                                        rfu_numclients++;
                                        gbaid = (rfu_masterdata[0] - 0x61f1) >> 3;
                                        linkmem->rfu_signal[gbaid] = 0xffffffff >> ((3 - (rfu_numclients - 1)) << 3);
                                    }
                                    if (gbaid == vbaid) {
                                        gbaid = (rfu_masterdata[0] - 0x61f1) >> 3;
                                    }
                                    rfu_state = RFU_RECV;
                                }
                            }
                            if (rfu_numclients > 0) {
                                for (int i = 0; i < rfu_numclients; i++)
                                    rfu_masterdata[i] = rfu_clientlist[i];
                            }
                            rfu_id = (uint16_t)((gbaid << 3) + 0x61f1);
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x1f: // join a room as client
                            // TODO: to fix infinte send&recv w/o giving much cance to update the screen when both side acting as client
                            // on MarioGolfAdv lobby(might be due to leftover data when switching from host to join mode at the same time?)
                            rfu_id = (uint16_t)rfu_masterdata[0];
                            gbaid = (rfu_id - 0x61f1) >> 3;
                            rfu_idx = rfu_id;
                            gbaidx = gbaid;
                            rfu_lastcmd2 = 0;
                            numtransfers = 0;
                            linkmem->rfu_q[vbaid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            linkmem->rfu_reqid[vbaid] = rfu_id;
                            // TODO:might failed to reset rfu_request when being accessed by otherside at the same time, sometimes both acting
                            // as client but one of them still have request[vbaid]!=0 //to prevent both GBAs from acting as Host, client can't
                            // be a host at the same time
                            linkmem->rfu_is_host[vbaid] = 0;
                            if (vbaid != gbaid) {
                                linkmem->rfu_signal[vbaid] = 0x00ff;
                                linkmem->rfu_is_host[gbaid] |= 1 << vbaid; // tells the other GBA(a host) that someone(a client) is joining
                            }
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x1e: // receive broadcast data
                            numtransfers = 0;
                            rfu_numclients = 0;
                            linkmem->rfu_is_host[vbaid] = 0; //to prevent both GBAs from acting as Host and thinking both of them have Client?
                            linkmem->rfu_q[vbaid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            [[fallthrough]];
                        case 0x1d: // no visible difference
                            linkmem->rfu_is_host[vbaid] = 0;
                            memset(rfu_masterdata, 0, sizeof(linkmem->rfu_broadcastdata[vbaid]));
                            rfu_qrecv_broadcast_data_len = 0;
                            for (int i = 0; i < linkmem->numgbas; i++) {
                                if (i != vbaid && linkmem->rfu_broadcastdata[i][0]) {
                                    memcpy(&rfu_masterdata[rfu_qrecv_broadcast_data_len], linkmem->rfu_broadcastdata[i], sizeof(linkmem->rfu_broadcastdata[i]));
                                    rfu_qrecv_broadcast_data_len += 7;
                                }
                            }
                            // is this needed? to prevent MarioGolfAdv from joining it's own room when switching
                            // from host to client mode due to left over room data in the game buffer?
                            // if(rfu_qrecv==0) rfu_qrecv = 7;
                            if (rfu_qrecv_broadcast_data_len > 0)
                                rfu_state = RFU_RECV;
                            rfu_polarity = 0;
                            rfu_counter = 0;
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x16: // send broadcast data (ie. room name)
                            //start broadcasting here may cause client to join other client in pokemon coloseum
                            //linkmem->rfu_bdata[vbaid][0] = (vbaid<<3)+0x61f1;
                            //linkmem->rfu_q[vbaid] = 0;
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x11: // get signal strength
                            //Switch remote id
                            if (linkmem->rfu_is_host[vbaid]) { //is a host
                                /*//gbaid = 1-vbaid; //linkmem->rfu_request[vbaid] & 1;
								gbaidx = gbaid;
								do {
								gbaidx = (gbaidx+1) % linkmem->numgbas;
								} while (gbaidx!=gbaid && linkmem->numgbas>=2 && (linkmem->rfu_reqid[gbaidx]!=(vbaid<<3)+0x61f1 || linkmem->rfu_q[gbaidx]<=0));
								if (gbaidx!=vbaid) {
								gbaid = gbaidx;
								rfu_id = (gbaid<<3)+0x61f1;
								}*/
                                /*if(rfu_numclients>0) {
								rfu_curclient = (rfu_curclient+1) % rfu_numclients;
								rfu_id = rfu_clientlist[rfu_curclient];
								gbaid = (rfu_id-0x61f1)>>3;
								}*/
                            }
                            //check signal
                            if (linkmem->numgbas >= 2 && (linkmem->rfu_is_host[vbaid] | linkmem->rfu_is_host[gbaid])) //signal only good when connected
                                if (rfu_ishost) { //update, just incase there are leaving clients
                                    uint8_t rfureq = linkmem->rfu_is_host[vbaid];
                                    uint8_t oldnum = rfu_numclients;
                                    rfu_numclients = 0;
                                    for (int i = 0; i < 8; i++) {
                                        if (rfureq & 1)
                                            rfu_numclients++;
                                        rfureq >>= 1;
                                    }
                                    if (rfu_numclients > oldnum)
                                        rfu_numclients = oldnum; //must not be higher than old value, which means the new client haven't been processed by 0x1a cmd yet
                                    linkmem->rfu_signal[vbaid] = 0xffffffff >> ((4 - rfu_numclients) << 3);
                                } else
                                    linkmem->rfu_signal[vbaid] = linkmem->rfu_signal[gbaid];
                            else
                                linkmem->rfu_signal[vbaid] = 0;
                            if (rfu_ishost) {
                                //linkmem->rfu_signal[vbaid] = 0x00ff; //host should have signal to prevent it from canceling the room? (may cause Digimon Racing host not knowing when a client leaving the room)
                                /*for (int i=0;i<linkmem->numgbas;i++)
								if (i!=vbaid && linkmem->rfu_reqid[i]==(vbaid<<3)+0x61f1) {
								rfu_masterdata[rfu_qrecv++] = linkmem->rfu_signal[i];
								}*/
                                //int j = 0;
                                /*int i = gbaid;
								if (linkmem->numgbas>=2)
								do {
								if (i!=vbaid && linkmem->rfu_reqid[i]==(vbaid<<3)+0x61f1) rfu_masterdata[rfu_qrecv++] = linkmem->rfu_signal[i];
								i = (i+1) % linkmem->numgbas;
								} while (i!=gbaid);*/
                                /*if(rfu_numclients>0)
								for(int i=0; i<rfu_numclients; i++) {
								uint32_t cid = (rfu_clientlist[i] & 0x0ffff);
								if(cid>=0x61f1) {
								cid = (cid-0x61f1)>>3;
								rfu_masterdata[rfu_qrecv++] = linkmem->rfu_signal[cid] = 0xffffffff>>((3-linkmem->rfu_clientidx[cid])<<3); //0x0ff << (linkmem->rfu_clientidx[cid]<<3);
								}
								}*/
                                //rfu_masterdata[0] = (uint32_t)linkmem->rfu_signal[vbaid];
                            }
                            if (rfu_qrecv_broadcast_data_len == 0) {
                                rfu_qrecv_broadcast_data_len = 1;
                                rfu_masterdata[0] = (uint32_t)linkmem->rfu_signal[vbaid];
                            }
                            if (rfu_qrecv_broadcast_data_len > 0) {
                                rfu_state = RFU_RECV;
                                int hid = vbaid;
                                if (!rfu_ishost)
                                    hid = gbaid;
                                rfu_masterdata[rfu_qrecv_broadcast_data_len - 1] = (uint32_t)linkmem->rfu_signal[hid];
                            }
                            rfu_cmd ^= 0x80;
                            //rfu_polarity = 0;
                            //rfu_transfer_end = 2048; //make it longer, giving time for data to come (since 0x26 usually used after 0x11)
                            /*//linktime = -2048; //1; //0;
							//numtransfers++; //not needed, just to keep track
							if ((numtransfers++) == 0) linktime = 1; //0; //might be needed to synchronize both performance? //numtransfers used to reset linktime to prevent it from reaching beyond max value of integer? //seems to be needed? otherwise data can't be received properly? //related to 0x24?
							linkmem->rfu_linktime[vbaid] = linktime; //save the ticks before changed to synchronize performance
							rfu_transfer_end = linkmem->rfu_linktime[gbaid] - linktime + 256; //waiting ticks = ticks difference between GBAs send/recv? //is max value of vbaid=1 ?
							if (rfu_transfer_end < 256) //lower/unlimited = faster client but slower host
							rfu_transfer_end = 256; //need to be positive for balanced performance in both GBAs?
							linktime = -rfu_transfer_end; //needed to synchronize performance on both side*/
                            break;
                        case 0x33: // rejoin status check?
                            if (linkmem->rfu_signal[vbaid] || numtransfers == 0)
                                rfu_masterdata[0] = 0;
                            else //0=success
                                rfu_masterdata[0] = (uint32_t)-1; //0xffffffff; //1=failed, 2++ = reserved/invalid, we use invalid value to let the game retries 0x33 until signal restored
                            rfu_cmd ^= 0x80;
                            rfu_state = RFU_RECV;
                            rfu_qrecv_broadcast_data_len = 1;
                            break;
                        case 0x14: // reset current client index and error check?
                            if ((linkmem->rfu_signal[vbaid] || numtransfers == 0) && gbaid != vbaid)
                                rfu_masterdata[0] = ((!rfu_ishost ? 0x100 : 0 + linkmem->rfu_clientidx[gbaid]) << 16) | ((gbaid << 3) + 0x61f1);
                            rfu_masterdata[0] = 0; //0=error, non-zero=good?
                            rfu_cmd ^= 0x80;
                            rfu_state = RFU_RECV;
                            rfu_qrecv_broadcast_data_len = 1;
                            break;
                        case 0x13: // error check?
                            if (linkmem->rfu_signal[vbaid] || numtransfers == 0 || rfu_initialized)
                                rfu_masterdata[0] = ((rfu_ishost ? 0x100 : 0 + linkmem->rfu_clientidx[vbaid]) << 16) | ((vbaid << 3) + 0x61f1);
                            else //high word should be 0x0200 ? is 0x0200 means 1st client and 0x4000 means 2nd client?
                                rfu_masterdata[0] = 0; //0=error, non-zero=good?
                            rfu_cmd ^= 0x80;
                            rfu_state = RFU_RECV;
                            rfu_qrecv_broadcast_data_len = 1;
                            break;
                        case 0x20: // client, this has something to do with 0x1f
                            rfu_masterdata[0] = (linkmem->rfu_clientidx[vbaid]) << 16; //needed for client
                            rfu_masterdata[0] |= (vbaid << 3) + 0x61f1; //0x1234; //0x641b; //max id value? Encryption key or Station Mode? (0xFBD9/0xDEAD=Access Point mode?)
                            linkmem->rfu_q[vbaid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            linkmem->rfu_is_host[vbaid] = 0; //TODO:may not works properly, sometimes both acting as client but one of them still have request[vbaid]!=0 //to prevent both GBAs from acting as Host, client can't be a host at the same time
                            if (linkmem->rfu_signal[gbaid] < linkmem->rfu_signal[vbaid])
                                linkmem->rfu_signal[gbaid] = linkmem->rfu_signal[vbaid];
                            rfu_polarity = 0;
                            rfu_state = RFU_RECV;
                            rfu_qrecv_broadcast_data_len = 1;
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x21: // client, this too
                            rfu_masterdata[0] = (linkmem->rfu_clientidx[vbaid]) << 16; //not needed?
                            rfu_masterdata[0] |= (vbaid << 3) + 0x61f1; //0x641b; //max id value? Encryption key or Station Mode? (0xFBD9/0xDEAD=Access Point mode?)
                            linkmem->rfu_q[vbaid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            linkmem->rfu_is_host[vbaid] = 0; //TODO:may not works properly, sometimes both acting as client but one of them still have request[vbaid]!=0 //to prevent both GBAs from acting as Host, client can't be a host at the same time
                            rfu_polarity = 0;
                            rfu_state = RFU_RECV; //3;
                            rfu_qrecv_broadcast_data_len = 1;
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x19: // server bind/start listening for client to join, may be used in the middle of host<->client communication w/o causing clients to dc?
                            //linkmem->rfu_request[vbaid] = 0; //to prevent both GBAs from acting as Host and thinking both of them have Client?
                            linkmem->rfu_q[vbaid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            linkmem->rfu_broadcastdata[vbaid][0] = (vbaid << 3) + 0x61f1; //start broadcasting room name
                            linkmem->rfu_clientidx[vbaid] = 0;
                            //numtransfers = 0;
                            //rfu_numclients = 0;
                            //rfu_curclient = 0;
                            //rfu_lastcmd2 = 0;
                            //rfu_polarity = 0;
                            rfu_ishost = true;
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x1c: //client, might reset some data?
                            //linkmem->rfu_request[vbaid] = 0; //to prevent both GBAs from acting as Host and thinking both of them have Client
                            //linkmem->rfu_bdata[vbaid][0] = 0; //stop broadcasting room name
                            rfu_ishost = false; //TODO: prevent both GBAs act as client but one of them have rfu_request[vbaid]!=0 on MarioGolfAdv lobby
                            //rfu_polarity = 0;
                            rfu_numclients = 0;
                            rfu_curclient = 0;
                            //c_s.Lock();
                            linkmem->rfu_listfront[vbaid] = 0;
                            linkmem->rfu_listback[vbaid] = 0;
                            linkmem->rfu_q[vbaid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            //DATALIST.clear();
                            //c_s.Unlock();
                            [[fallthrough]];
                        case 0x1b: //host, might reset some data? may be used in the middle of host<->client communication w/o causing clients to dc?
                            //linkmem->rfu_request[vbaid] = 0; //to prevent both GBAs from acting as Client and thinking one of them is a Host?
                            linkmem->rfu_broadcastdata[vbaid][0] = 0; //0 may cause player unable to join in pokemon union room?
                            //numtransfers = 0;
                            //linktime = 1;
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x30: //reset some data
                            if (vbaid != gbaid) { //(linkmem->numgbas >= 2)
                                //linkmem->rfu_signal[gbaid] = 0;
                                linkmem->rfu_is_host[gbaid] &= ~(1 << vbaid); //linkmem->rfu_request[gbaid] = 0;
                                SetEvent(linksync[gbaid]); //allow other gba to move
                            }
                            //WaitForSingleObject(linksync[vbaid], 40/*linktimeout*/);
                            while (linkmem->rfu_signal[vbaid]) {
                                WaitForSingleObject(linksync[vbaid], 1 /*linktimeout*/);
                                linkmem->rfu_signal[vbaid] = 0;
                                linkmem->rfu_is_host[vbaid] = 0; //There is a possibility where rfu_request/signal didn't get zeroed here when it's being read by the other GBA at the same time
                                //SleepEx(1,true);
                            }
                            //c_s.Lock();
                            linkmem->rfu_listfront[vbaid] = 0;
                            linkmem->rfu_listback[vbaid] = 0;
                            linkmem->rfu_q[vbaid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            //DATALIST.clear();
                            linkmem->rfu_proto[vbaid] = 0;
                            linkmem->rfu_reqid[vbaid] = 0;
                            linkmem->rfu_linktime[vbaid] = 0;
                            linkmem->rfu_gdata[vbaid] = 0;
                            linkmem->rfu_broadcastdata[vbaid][0] = 0;
                            //c_s.Unlock();
                            rfu_polarity = 0; //is this included?
                            //linkid = -1; //0;
                            numtransfers = 0;
                            rfu_numclients = 0;
                            rfu_curclient = 0;
                            linktime = 1; //0; //reset here instead of at 0x24/0xa5/0xa7
                            /*rfu_id = 0;
							rfu_idx = 0;
							gbaid = vbaid;
							gbaidx = gbaid;
							rfu_ishost = false;
							rfu_isfirst = false;*/
                            rfu_cmd ^= 0x80;
                            SetEvent(linksync[vbaid]); //may not be needed
                            break;

                        case 0x3d: // init/reset rfu data
                            rfu_initialized = false;
                            [[fallthrough]];
                        case 0x10: // init/reset rfu data
                            if (vbaid != gbaid) { //(linkmem->numgbas >= 2)
                                //linkmem->rfu_signal[gbaid] = 0;
                                linkmem->rfu_is_host[gbaid] &= ~(1 << vbaid); //linkmem->rfu_request[gbaid] = 0;
                                SetEvent(linksync[gbaid]); //allow other gba to move
                            }
                            //WaitForSingleObject(linksync[vbaid], 40/*linktimeout*/);
                            while (linkmem->rfu_signal[vbaid]) {
                                WaitForSingleObject(linksync[vbaid], 1 /*linktimeout*/);
                                linkmem->rfu_signal[vbaid] = 0;
                                linkmem->rfu_is_host[vbaid] = 0; //There is a possibility where rfu_request/signal didn't get zeroed here when it's being read by the other GBA at the same time
                                //SleepEx(1,true);
                            }
                            //c_s.Lock();
                            linkmem->rfu_listfront[vbaid] = 0;
                            linkmem->rfu_listback[vbaid] = 0;
                            linkmem->rfu_q[vbaid] = 0; //to prevent leftover data from previous session received immediately in the new session
                            //DATALIST.clear();
                            linkmem->rfu_proto[vbaid] = 0;
                            linkmem->rfu_reqid[vbaid] = 0;
                            linkmem->rfu_linktime[vbaid] = 0;
                            linkmem->rfu_gdata[vbaid] = 0;
                            linkmem->rfu_broadcastdata[vbaid][0] = 0;
                            //c_s.Unlock();
                            rfu_polarity = 0; //is this included?
                            //linkid = -1; //0;
                            numtransfers = 0;
                            rfu_numclients = 0;
                            rfu_curclient = 0;
                            linktime = 1; //0; //reset here instead of at 0x24/0xa5/0xa7
                            rfu_id = 0;
                            rfu_idx = 0;
                            gbaid = vbaid;
                            gbaidx = gbaid;
                            rfu_ishost = false;
                            rfu_qrecv_broadcast_data_len = 0;
                            SetEvent(linksync[vbaid]); //may not be needed
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x36: //does it expect data returned?
                        case 0x26:
                            //Switch remote id to available data
                            /*//if(vbaid==gbaid) {
							if(linkmem->numgbas>=2)
							if((linkmem->rfu_q[gbaid]<=0) || !(linkmem->rfu_qid[gbaid] & (1<<vbaid))) //current remote id doesn't have data
							//do
							{
							if(rfu_numclients>0) { //is a host
							uint8_t cc = rfu_curclient;
							do {
							rfu_curclient = (rfu_curclient+1) % rfu_numclients;
							rfu_idx = rfu_clientlist[rfu_curclient];
							gbaidx = (rfu_idx-0x61f1)>>3;
							} while (!AppTerminated && cc!=rfu_curclient && rfu_numclients>=1 && (!(linkmem->rfu_qid[gbaidx] & (1<<vbaid)) || linkmem->rfu_q[gbaidx]<=0));
							if (cc!=rfu_curclient) { //gbaidx!=vbaid && gbaidx!=gbaid
							gbaid = gbaidx;
							rfu_id = rfu_idx;
							//log("%d  Switch%02X:%d\n",GetTickCount(),rfu_cmd,gbaid);
							//if(linkmem->rfu_q[gbaid]>0 || rfu_lastcmd2==0)
							//break;
							}
							}
							//SleepEx(1,true);
							} //while (!AppTerminated && gbaid!=vbaid && linkmem->numgbas>=2 && linkmem->rfu_signal[gbaid] && linkmem->rfu_q[gbaid]<=0 && linkmem->rfu_q[vbaid]>0 && (GetTickCount()-rfu_lasttime)<1); //(DWORD)linktimeout
							}*/

                            //Wait for data

                            //Read data when available
                            /*if((linkmem->rfu_qid[gbaid] & (1<<vbaid))) //data is for this GBA
							if((rfu_qrecv=rfu_masterq=linkmem->rfu_q[gbaid])!=0) { //data size > 0
							memcpy(rfu_masterdata, linkmem->linkdata[gbaid], min(rfu_masterq<<2,sizeof(rfu_masterdata))); //128 //read data from other GBA
							linkmem->rfu_qid[gbaid] &= ~(1<<vbaid); //mark as received by this GBA
							if(linkmem->rfu_request[gbaid]) linkmem->rfu_qid[gbaid] &= linkmem->rfu_request[gbaid]; //remask if it's host, just incase there are client leaving multiplayer
							if(!linkmem->rfu_qid[gbaid]) linkmem->rfu_q[gbaid] = 0; //mark that it has been fully received
							if(!linkmem->rfu_q[gbaid]) SetEvent(linksync[gbaid]); // || (rfu_ishost && linkmem->rfu_qid[gbaid]!=linkmem->rfu_request[gbaid])
							//ResetEvent(linksync[vbaid]); //linksync[vbaid] //lock this gba, don't allow this gba to move (prevent both GBA using 0x25 at the same time) //slower but improve stability by preventing both GBAs from using 0x25 at the same time
							//SetEvent(linksync[1-vbaid]); //unlock other gba, allow other gba to move (sending their data) //faster but may affect stability and cause both GBAs using 0x25 at the same time, too fast communication could also cause the game from updating the screen
							}*/
                            bool ok;
                            int ctr;
                            ctr = 0;
                            //WaitForSingleObject(linksync[vbaid], linktimeout); //wait until unlocked
                            //ResetEvent(linksync[vbaid]); //lock it so noone can access it
                            if (linkmem->rfu_listfront[vbaid] != linkmem->rfu_listback[vbaid]) //data existed
                                do {
                                    uint8_t tmpq = linkmem->rfu_datalist[vbaid][linkmem->rfu_listfront[vbaid]].len; //(uint8_t)linkmem->rfu_qlist[vbaid][linkmem->rfu_listfront[vbaid]];
                                    ok = false;
                                    if (tmpq != rfu_qrecv_broadcast_data_len)
                                        ok = true;
                                    else
                                        for (int i = 0; i < tmpq; i++)
                                            if (linkmem->rfu_datalist[vbaid][linkmem->rfu_listfront[vbaid]].data[i] != rfu_masterdata[i]) {
                                                ok = true;
                                                break;
                                            }

                                    if (tmpq == 0 && ctr == 0)
                                        ok = true; //0-size data

                                    if (ok) //next data is not a duplicate of currently unprocessed data
                                        if (rfu_qrecv_broadcast_data_len < 2 || tmpq > 1) {
                                            if (rfu_qrecv_broadcast_data_len > 1) { //stop here if next data is different than currently unprocessed non-ping data
                                                linkmem->rfu_linktime[gbaid] = linkmem->rfu_datalist[vbaid][linkmem->rfu_listfront[vbaid]].time;
                                                break;
                                            }

                                            if (tmpq >= rfu_qrecv_broadcast_data_len) {
                                                rfu_masterq = rfu_qrecv_broadcast_data_len = tmpq;
                                                gbaid = linkmem->rfu_datalist[vbaid][linkmem->rfu_listfront[vbaid]].gbaid;
                                                rfu_id = (uint16_t)((gbaid << 3) + 0x61f1);
                                                if (rfu_ishost)
                                                    rfu_curclient = (uint8_t)linkmem->rfu_clientidx[gbaid];
                                                if (rfu_qrecv_broadcast_data_len != 0) { //data size > 0
                                                    memcpy(rfu_masterdata, linkmem->rfu_datalist[vbaid][linkmem->rfu_listfront[vbaid]].data, std::min(rfu_masterq << 2, (int)sizeof(rfu_masterdata)));
                                                }
                                            }
                                        } //else log("%08X  CMD26 Skip: %d %d %d\n",GetTickCount(),rfu_qrecv,linkmem->rfu_q[gbaid],tmpq);

                                    linkmem->rfu_listfront[vbaid]++;
                                    ctr++;

                                    ok = (linkmem->rfu_listfront[vbaid] != linkmem->rfu_listback[vbaid] && linkmem->rfu_datalist[vbaid][linkmem->rfu_listfront[vbaid]].gbaid == gbaid);
                                } while (ok);
                            //SetEvent(linksync[vbaid]); //unlock it so anyone can access it

                            if (rfu_qrecv_broadcast_data_len > 0) { //data was available
                                rfu_state = RFU_RECV;
                                rfu_counter = 0;
                                rfu_lastcmd2 = 0;

                                //Switch remote id to next remote id
                                /*if (linkmem->rfu_request[vbaid]) { //is a host
									if(rfu_numclients>0) {
									rfu_curclient = (rfu_curclient+1) % rfu_numclients;
									rfu_id = rfu_clientlist[rfu_curclient];
									gbaid = (rfu_id-0x61f1)>>3;
									//log("%d  SwitchNext%02X:%d\n",GetTickCount(),rfu_cmd,gbaid);
									}
									}*/
                            }
                            /*if(vbaid!=gbaid && linkmem->rfu_request[vbaid] && linkmem->rfu_request[gbaid])
								MessageBox(0,_T("Both GBAs are Host!"),_T("Warning"),0);*/
                            rfu_cmd ^= 0x80;
                            break;

                        case 0x24: // send [non-important] data (used by server often)
                            //numtransfers++; //not needed, just to keep track
                            if ((numtransfers++) == 0)
                                linktime = 1; //needed to synchronize both performance and for Digimon Racing's client to join successfully //numtransfers used to reset linktime to prevent it from reaching beyond max value of integer? //numtransfers doesn't seems to be used?
                            //linkmem->rfu_linktime[vbaid] = linktime; //save the ticks before reseted to zero

                            if (rfu_cansend) {
                                /*memcpy(linkmem->rfu_data[vbaid],rfu_masterdata,4*rfu_qsend2);
								linkmem->rfu_proto[vbaid] = 0; //UDP-like
								if(rfu_ishost)
								linkmem->rfu_qid[vbaid] = linkmem->rfu_request[vbaid]; else
								linkmem->rfu_qid[vbaid] |= 1<<gbaid;
								linkmem->rfu_q[vbaid] = rfu_qsend2;*/
                                if (rfu_ishost) {
                                    for (int j = 0; j < linkmem->numgbas; j++)
                                        if (j != vbaid) {
                                            WaitForSingleObject(linksync[j], linktimeout); //wait until unlocked
                                            ResetEvent(linksync[j]); //lock it so noone can access it
                                            memcpy(linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].data, rfu_masterdata, 4 * rfu_qsend2);
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].gbaid = (uint8_t)vbaid;
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].len = rfu_qsend2;
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].time = linktime;
                                            linkmem->rfu_listback[j]++;
                                            SetEvent(linksync[j]); //unlock it so anyone can access it
                                        }
                                } else if (vbaid != gbaid) {
                                    WaitForSingleObject(linksync[gbaid], linktimeout); //wait until unlocked
                                    ResetEvent(linksync[gbaid]); //lock it so noone can access it
                                    memcpy(linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].data, rfu_masterdata, 4 * rfu_qsend2);
                                    linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].gbaid = (uint8_t)vbaid;
                                    linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].len = rfu_qsend2;
                                    linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].time = linktime;
                                    linkmem->rfu_listback[gbaid]++;
                                    SetEvent(linksync[gbaid]); //unlock it so anyone can access it
                                }
                            } else {
                                //log("%08X : IgnoredSend[%02X] %d\n", GetTickCount(), rfu_cmd, rfu_qsend2);
                            }

                            linktime = 0; //need to zeroed when sending? //0 might cause slowdown in performance
                            rfu_cmd ^= 0x80;
                            //linkid = -1; //not needed?
                            break;

                        case 0x25: // send [important] data & wait for [important?] reply data
                        case 0x35: // send [important] data & wait for [important?] reply data
                            //numtransfers++; //not needed, just to keep track
                            if ((numtransfers++) == 0)
                                linktime = 1; //0; //might be needed to synchronize both performance? //numtransfers used to reset linktime to prevent it from reaching beyond max value of integer? //seems to be needed? otherwise data can't be received properly? //related to 0x24?
                            //linktime = 0;
                            //linkmem->rfu_linktime[vbaid] = linktime; //save the ticks before changed to synchronize performance

                            if (rfu_cansend && rfu_qsend2 > 0) {
                                /*memcpy(linkmem->rfu_data[vbaid],rfu_masterdata,4*rfu_qsend2);
								linkmem->rfu_proto[vbaid] = 1; //TCP-like
								if(rfu_ishost)
								linkmem->rfu_qid[vbaid] = linkmem->rfu_request[vbaid]; else
								linkmem->rfu_qid[vbaid] |= 1<<gbaid;
								linkmem->rfu_q[vbaid] = rfu_qsend2;*/
                                if (rfu_ishost) {
                                    for (int j = 0; j < linkmem->numgbas; j++)
                                        if (j != vbaid) {
                                            WaitForSingleObject(linksync[j], linktimeout); //wait until unlocked
                                            ResetEvent(linksync[j]); //lock it so noone can access it
                                            memcpy(linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].data, rfu_masterdata, 4 * rfu_qsend2);
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].gbaid = (uint8_t)vbaid;
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].len = rfu_qsend2;
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].time = linktime;
                                            linkmem->rfu_listback[j]++;
                                            SetEvent(linksync[j]); //unlock it so anyone can access it
                                        }
                                } else if (vbaid != gbaid) {
                                    WaitForSingleObject(linksync[gbaid], linktimeout); //wait until unlocked
                                    ResetEvent(linksync[gbaid]); //lock it so noone can access it
                                    memcpy(linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].data, rfu_masterdata, 4 * rfu_qsend2);
                                    linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].gbaid = (uint8_t)vbaid;
                                    linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].len = rfu_qsend2;
                                    linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].time = linktime;
                                    linkmem->rfu_listback[gbaid]++;
                                    SetEvent(linksync[gbaid]); //unlock it so anyone can access it
                                }
                            } else {
                                //log("%08X : IgnoredSend[%02X] %d\n", GetTickCount(), rfu_cmd, rfu_qsend2);
                            }
                            //numtransfers++; //not needed, just to keep track
                            //if((numtransfers++)==0) linktime = 1; //may not be needed here?
                            //linkmem->rfu_linktime[vbaid] = linktime; //may not be needed here? save the ticks before reseted to zero
                            //linktime = 0; //may not be needed here? //need to zeroed when sending? //0 might cause slowdown in performance
                            //TODO: there is still a chance for 0x25 to be used at the same time on both GBA (both GBAs acting as client but keep sending & receiving using 0x25 & 0x26 for infinity w/o updating the screen much)
                            //Waiting here for previous data to be received might be too late! as new data already sent before finalization cmd
                            [[fallthrough]];
                        case 0x27: // wait for data ?
                        case 0x37: // wait for data ?
                            //numtransfers++; //not needed, just to keep track
                            if ((numtransfers++) == 0)
                                linktime = 1; //0; //might be needed to synchronize both performance? //numtransfers used to reset linktime to prevent it from reaching beyond max value of integer? //seems to be needed? otherwise data can't be received properly? //related to 0x24?
                            //linktime = 0;
                            //linkmem->rfu_linktime[vbaid] = linktime; //save the ticks before changed to synchronize performance

                            if (rfu_ishost) {
                                for (int j = 0; j < linkmem->numgbas; j++)
                                    if (j != vbaid) {
                                        WaitForSingleObject(linksync[j], linktimeout); //wait until unlocked
                                        ResetEvent(linksync[j]); //lock it so noone can access it
                                        //memcpy(linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].data,rfu_masterdata,4*rfu_qsend2);
                                        linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].gbaid = (uint8_t)vbaid;
                                        linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].len = 0; //rfu_qsend2;
                                        linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].time = linktime;
                                        linkmem->rfu_listback[j]++;
                                        SetEvent(linksync[j]); //unlock it so anyone can access it
                                    }
                            } else if (vbaid != gbaid) {
                                WaitForSingleObject(linksync[gbaid], linktimeout); //wait until unlocked
                                ResetEvent(linksync[gbaid]); //lock it so noone can access it
                                //memcpy(linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].data,rfu_masterdata,4*rfu_qsend2);
                                linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].gbaid = (uint8_t)vbaid;
                                linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].len = 0; //rfu_qsend2;
                                linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].time = linktime;
                                linkmem->rfu_listback[gbaid]++;
                                SetEvent(linksync[gbaid]); //unlock it so anyone can access it
                            }
                            //}
                            rfu_cmd ^= 0x80;
                            break;

                        case 0xee: //is this need to be processed?
                            rfu_cmd ^= 0x80;
                            rfu_polarity = 1;
                            break;

                        case 0x17: // setup or something ?
                        default:
                            rfu_cmd ^= 0x80;
                            break;

                        case 0xa5: //	2nd part of send&wait function 0x25
                        case 0xa7: //	2nd part of wait function 0x27
                        case 0xb5: //	2nd part of send&wait function 0x35?
                        case 0xb7: //	2nd part of wait function 0x37?
                            if (linkmem->rfu_listfront[vbaid] != linkmem->rfu_listback[vbaid]) {
                                rfu_polarity = 1; //reverse polarity to make the game send 0x80000000 command word (to be replied with 0x99660028 later by the adapter)
                                if (rfu_cmd == 0xa5 || rfu_cmd == 0xa7)
                                    rfu_cmd = 0x28;
                                else
                                    rfu_cmd = 0x36; //there might be 0x29 also //don't return 0x28 yet until there is incoming data (or until 500ms-6sec timeout? may reset RFU after timeout)
                            } else
                                rfu_waiting = true;

                            /*//numtransfers++; //not needed, just to keep track
							if ((numtransfers++) == 0) linktime = 1; //0; //might be needed to synchronize both performance? //numtransfers used to reset linktime to prevent it from reaching beyond max value of integer? //seems to be needed? otherwise data can't be received properly? //related to 0x24?
							//linktime = 0;
							//if (rfu_cmd==0xa5)
							linkmem->rfu_linktime[vbaid] = linktime; //save the ticks before changed to synchronize performance
							*/

                            //prevent GBAs from sending data at the same time (which may cause waiting at the same time in the case of 0x25), also gives time for the other side to read the data
                            //if (linkmem->numgbas>=2 && linkmem->rfu_signal[vbaid] && linkmem->rfu_signal[gbaid]) {
                            //	SetEvent(linksync[gbaid]); //allow other gba to move (sending their data)
                            //	WaitForSingleObject(linksync[vbaid], 1); //linktimeout //wait until this gba allowed to move
                            //	//if(rfu_cmd==0xa5)
                            //	ResetEvent(linksync[vbaid]); //don't allow this gba to move (prevent sending another data too fast w/o giving the other side chances to read it)
                            //}

                            rfu_transfer_end = linkmem->rfu_linktime[gbaid] - linktime + 1; //256; //waiting ticks = ticks difference between GBAs send/recv? //is max value of vbaid=1 ?

                            if (rfu_transfer_end > 2560) //may need to cap the max ticks to prevent some games (ie. pokemon) from getting in-game timeout due to executing too many opcodes (too fast)
                                rfu_transfer_end = 2560; //10240;

                            if (rfu_transfer_end < 256) //lower/unlimited = faster client but slower host
                                rfu_transfer_end = 256; //need to be positive for balanced performance in both GBAs?

                            linktime = -rfu_transfer_end; //needed to synchronize performance on both side
                            break;
                        }
                        if (!rfu_waiting)
                            rfu_buf = 0x99660000 | (rfu_qrecv_broadcast_data_len << 8) | rfu_cmd;
                        else
                            rfu_buf = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    }
                } else { //unknown COMM word
                    // Same three-way handling as the socket path (see
                    // StartRFUSocket): a mid-session login restart (LSBs =
                    // "NI" key, e.g. 0x7FFF494E) drops to RFU_INIT with
                    // clean polarity/command state; a stray login-tail word
                    // with the 0x7FF prefix (MarioGolfAdv's 0x7FFE8001 on
                    // lobby exit) stays in RFU_COMM; anything else resyncs
                    // via a fresh login. This copy used to reset
                    // unconditionally, which broke the MarioGolfAdv case
                    // and carried stale polarity into re-logins.
                    const uint32_t com = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    log("%08X : UnkCOM %08X  %04X  %08X %08X\n", GetTickCount(), com, PrevVAL, PrevCOM, PrevDAT);
                    if ((com & 0xFFFF) == 0x494E) { // login restart ("NI")
                        rfu_state = RFU_INIT;
                        rfu_polarity = 0;
                        rfu_cmd = 0;
                        rfu_cmd2 = 0;
                        rfu_lastcmd = 0;
                        rfu_lastcmd2 = 0;
                        rfu_waiting = false;
                    } else if ((com >> 20) != 0x7ff) {
                        rfu_state = RFU_INIT;
                    }
                    rfu_buf = (READ16LE(&g_ioMem[COMM_SIODATA32_L]) << 16) | siodata_h;
                }
                break;

            case RFU_SEND: //data following after initialize cmd
                //if(rfu_qsend==0) {rfu_state = RFU_COMM; break;}
                CurDAT = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                if (--rfu_qsend == 0) {
                    rfu_state = RFU_COMM;
                }

                switch (rfu_cmd) {
                case 0x16:
                    if (rfu_counter < kRfuBroadcastPayloadWords) {
                        linkmem->rfu_broadcastdata[vbaid][1 + rfu_counter] =
                            READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    }
                    rfu_counter++;
                    break;

                case 0x17:
                    //linkid = 1;
                    rfu_masterdata[rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x1f:
                    rfu_masterdata[rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x24:
                //if(linkmem->rfu_proto[vbaid]) break; //important data from 0x25 shouldn't be overwritten by 0x24
                case 0x25:
                case 0x35:
                    //if(rfu_cansend)
                    //linkmem->rfu_data[vbaid][rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    rfu_masterdata[rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    break;

                default:
                    rfu_masterdata[rfu_counter++] = READ32LE(&g_ioMem[COMM_SIODATA32_L]);
                    break;
                }
                rfu_buf = 0x80000000;
                break;

            case RFU_RECV: //data following after finalize cmd
                //if(rfu_qrecv==0) {rfu_state = RFU_COMM; break;}
                if (--rfu_qrecv_broadcast_data_len == 0)
                    rfu_state = RFU_COMM;

                switch (rfu_cmd) {
                case 0x9d:
                case 0x9e:
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0xb6:
                case 0xa6:
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0x91: //signal strength
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0xb3: //rejoin error code?
                /*UPDATE_REG(COMM_SIODATA32_L, 2); //0 = success, 1 = failed, 0x2++ = invalid
					UPDATE_REG(COMM_SIODATA32_H, 0x0000); //high word 0 = a success indication?
					break;*/
                case 0x94: //last error code? //it seems like the game doesn't care about this value
                case 0x93: //last error code? //it seems like the game doesn't care about this value
                    /*if(linkmem->rfu_signal[vbaid] || linkmem->numgbas>=2) {
					UPDATE_REG(COMM_SIODATA32_L, 0x1234);	// put anything in here
					UPDATE_REG(COMM_SIODATA32_H, 0x0200);	// also here, but it should be 0200
					} else {
					UPDATE_REG(COMM_SIODATA32_L, 0);	// put anything in here
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					}*/
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0xa0:
                    //max id value? Encryption key or Station Mode? (0xFBD9/0xDEAD=Access Point mode?)
                    //high word 0 = a success indication?
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;
                case 0xa1:
                    //max id value? the same with 0xa0 cmd?
                    //high word 0 = a success indication?
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                case 0x9a:
                    rfu_buf = rfu_masterdata[rfu_counter++];
                    break;

                default: //unknown data (should use 0 or -1 as default), usually returning 0 might cause the game to think there is something wrong with the connection (ie. 0x11/0x13 cmd)
                    //0x0173 //not 0x0000 as default?
                    //0x0000
                    rfu_buf = 0xffffffff; //rfu_masterdata[rfu_counter++];
                    break;
                }
                break;
            }
            transfer_direction = 1;

            PrevVAL = value;
            PrevDAT = CurDAT;
            PrevCOM = CurCOM;
        }

        //Moved from the top to fix Mario Golf Adv from Occasionally Not Detecting wireless adapter
        /*if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
		value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
		else //(Bit.3, 0=Enable Transfer/Ready)
		value |= 4; //bit.2=1 (otherside is Not Ready)*/

        /*if (value & 1)
		value |= 0x02; //wireless always use 2Mhz speed right? this will fix MarioGolfAdv Not Detecting wireless*/

        if (rfu_polarity)
            value ^= 4; // sometimes it's the other way around
        /*value &= 0xfffb;
		value |= (value & 1)<<2;*/
        [[fallthrough]];
    default:
        UPDATE_REG(COMM_SIOCNT, value);
        return;
    }
}

bool LinkRFUUpdate()
{
    //if (IsLinkConnected()) {
    //}
    if (rfu_enabled) {
        if (transfer_direction && rfu_transfer_end <= 0) {
            if (rfu_waiting) {
                bool ok = false;
                // uint32_t tmout = linktimeout;
                // if ((!lanlink.active&&speedhack) || (lanlink.speed&&IsLinkConnected()))tmout = 16;
                if (rfu_state != RFU_INIT) {
                    if (rfu_cmd == 0x24 || rfu_cmd == 0x25 || rfu_cmd == 0x35) {
                        //c_s.Lock();
                        ok = linkmem->rfu_signal[vbaid] && linkmem->rfu_q[vbaid] > 1 && rfu_qsend > 1;
                        //c_s.Unlock();
                        if (ok && (GetTickCount() - rfu_lasttime) < (uint32_t)linktimeout) {
                            return false;
                        }
                        if (linkmem->rfu_q[vbaid] < 2 || rfu_qsend > 1) {
                            rfu_cansend = true;
                            //c_s.Lock();
                            linkmem->rfu_q[vbaid] = 0;
                            linkmem->rfu_qid[vbaid] = 0;
                            //c_s.Unlock();
                        }
                        rfu_buf = 0x80000000;
                    } else {

                        if (((rfu_cmd == 0x11 || rfu_cmd == 0x1a || rfu_cmd == 0x26) && (GetTickCount() - rfu_lasttime) < 16) || ((rfu_cmd == 0xa5 || rfu_cmd == 0xb5) && (GetTickCount() - rfu_lasttime) < 16) || ((rfu_cmd == 0xa7 || rfu_cmd == 0xb7) && (GetTickCount() - rfu_lasttime) < (uint32_t)linktimeout)) {
                            //c_s.Lock();
                            ok = (linkmem->rfu_listfront[vbaid] != linkmem->rfu_listback[vbaid]);
                            //c_s.Unlock();
                            if (!ok)
                                for (int i = 0; i < linkmem->numgbas; i++)
                                    if (i != vbaid)
                                        if (linkmem->rfu_q[i] && (linkmem->rfu_qid[i] & (1 << vbaid))) {
                                            ok = true;
                                            break;
                                        }
                            if (!linkmem->rfu_signal[vbaid])
                                ok = true;
                            if (!ok) {
                                return false;
                            }
                        }
                        if (rfu_cmd == 0xa5 || rfu_cmd == 0xa7 || rfu_cmd == 0xb5 || rfu_cmd == 0xb7 || rfu_cmd == 0xee)
                            rfu_polarity = 1;
                        if (rfu_cmd == 0xa5 || rfu_cmd == 0xa7)
                            rfu_cmd = 0x28;
                        else if (rfu_cmd == 0xb5 || rfu_cmd == 0xb7)
                            rfu_cmd = 0x36;

                        if (READ32LE(&g_ioMem[COMM_SIODATA32_L]) == 0x80000000)
                            rfu_buf = 0x99660000 | (rfu_qrecv_broadcast_data_len << 8) | rfu_cmd;
                        else
                            rfu_buf = 0x80000000;
                    }
                    rfu_waiting = false;
                }
            }
            UPDATE_REG(COMM_SIODATA32_L, (uint16_t)rfu_buf);
            UPDATE_REG(COMM_SIODATA32_H, rfu_buf >> 16);
        }
    }
    return true;
}

static void UpdateRFUIPC(int ticks)
{
    if (rfu_enabled) {
        rfu_transfer_end -= ticks;

        if (LinkRFUUpdate()) {
            if (transfer_direction && rfu_transfer_end <= 0) {
                transfer_direction = 0;
                uint16_t value = READ16LE(&g_ioMem[COMM_SIOCNT]);
                RfuTrace("ipc st=%d  in=%08X cnt=%04X cmd=%02X pol=%d done",
                    rfu_state, READ32LE(&g_ioMem[COMM_SIODATA32_L]), value,
                    rfu_cmd, rfu_polarity);
                if (value & 0x4000) {
                    CPURaiseSioIRQ();
                }

                //if (rfu_polarity) value ^= 4;
                value &= 0xfffb;
                value |= (value & 1) << 2; //this will automatically set the correct polarity, even w/o rfu_polarity since the game will be the one who change the polarity instead of the adapter

                //UPDATE_REG(COMM_SIOCNT, READ16LE(&g_ioMem[COMM_SIOCNT]) & 0xff7f);
                UPDATE_REG(COMM_SIOCNT, (value & 0xff7f) | 0x0008); //Start bit.7 reset, SO bit.3 set automatically upon transfer completion?
                //log("SIOn32 : %04X %04X  %08X  (VCOUNT = %d) %d %d\n", READ16LE(&g_ioMem[COMM_RCNT]), READ16LE(&g_ioMem[COMM_SIOCNT]), READ32LE(&g_ioMem[COMM_SIODATA32_L]), VCOUNT);
            }
            return;
        }
    }
}

void gbInitLinkIPC()
{
    LinkIsWaiting = false;
    LinkFirstTime = true;
    linkmem->linkcmd[linkid] = 0;
    linkmem->linkdata[linkid] = 0xff;
}

uint8_t gbStartLinkIPC(uint8_t b) //used on internal clock
{
    uint8_t dat = 0xff; //master (w/ internal clock) will gets 0xff if slave is turned off (or not ready yet also?)
    //if(linkid) return 0xff; //b; //Slave shouldn't be sending from here
    //int gbSerialOn = (gbMemory[0xff02] & 0x80); //not needed?
    gba_link_enabled = true; //(gbMemory[0xff02]!=0); //not needed?
    rfu_enabled = false;

    if (!gba_link_enabled)
        return 0xff;

    //Single Computer
    if (GetLinkMode() == LINK_GAMEBOY_IPC) {
        uint32_t tm = GetTickCount();
        do {
            WaitForSingleObject(linksync[linkid], 1);
            ResetEvent(linksync[linkid]);
        } while (linkmem->linkcmd[linkid] && (GetTickCount() - tm) < (uint32_t)linktimeout);
        linkmem->linkdata[linkid] = b;
        linkmem->linkcmd[linkid] = 1;
        SetEvent(linksync[linkid]);

        LinkIsWaiting = false;
        tm = GetTickCount();
        do {
            WaitForSingleObject(linksync[1 - linkid], 1);
            ResetEvent(linksync[1 - linkid]);
        } while (!linkmem->linkcmd[1 - linkid] && (GetTickCount() - tm) < (uint32_t)linktimeout);
        if (linkmem->linkcmd[1 - linkid]) {
            dat = (uint8_t)linkmem->linkdata[1 - linkid];
            linkmem->linkcmd[1 - linkid] = 0;
        } //else LinkIsWaiting = true;
        SetEvent(linksync[1 - linkid]);

        LinkFirstTime = true;
        if (dat != 0xff /*||b==0x00||dat==0x00*/)
            LinkFirstTime = false;

        return dat;
    }
    return dat;
}

uint16_t gbLinkUpdateIPC(uint8_t b, int gbSerialOn) //used on external clock
{
    uint8_t dat = b; //0xff; //slave (w/ external clocks) won't be getting 0xff if master turned off
    bool recvd = false;

    gba_link_enabled = true; //(gbMemory[0xff02]!=0);
    rfu_enabled = false;

    if (gbSerialOn) {
        if (gba_link_enabled) {
            //Single Computer
            if (GetLinkMode() == LINK_GAMEBOY_IPC) {
                uint32_t tm; // = GetTickCount();
                //do {
                WaitForSingleObject(linksync[1 - linkid], linktimeout);
                ResetEvent(linksync[1 - linkid]);
                //} while (!linkmem->linkcmd[1-linkid] && (GetTickCount()-tm)<(uint32_t)linktimeout);
                if (linkmem->linkcmd[1 - linkid]) {
                    dat = (uint8_t)linkmem->linkdata[1 - linkid];
                    linkmem->linkcmd[1 - linkid] = 0;
                    recvd = true;
                    LinkIsWaiting = false;
                } else
                    LinkIsWaiting = true;
                SetEvent(linksync[1 - linkid]);

                if (!LinkIsWaiting) {
                    // Wait for the master to consume our *own* previous
                    // byte (linkcmd[linkid]) before queueing the next one;
                    // waiting on the master's slot here made this loop exit
                    // immediately and the guard below silently dropped the
                    // reply byte, stalling the exchange for linktimeout.
                    tm = GetTickCount();
                    do {
                        WaitForSingleObject(linksync[linkid], 1);
                        ResetEvent(linksync[linkid]);
                    } while (linkmem->linkcmd[linkid] && (GetTickCount() - tm) < (uint32_t)linktimeout);
                    if (!linkmem->linkcmd[linkid]) {
                        linkmem->linkdata[linkid] = b;
                        linkmem->linkcmd[linkid] = 1;
                    }
                    SetEvent(linksync[linkid]);
                }
            }
        }

        if (dat == 0xff /*||dat==0x00||b==0x00*/) //dat==0xff||dat==0x00
            LinkFirstTime = true;
    }
    return ((dat << 8) | (recvd ? 1 : 0));
}

static void CloseIPC()
{
    // Nothing to do when InitIPC never succeeded -- its error paths clean
    // up after themselves, and the last-one-out logic below must not run
    // with no session attached: it would unlink a live session's objects
    // out from under its members.
    if (linkmem == NULL)
        return;

    [[maybe_unused]] int f = 0;
    {
        // Clearing our slot and recomputing the peer count is a
        // read-modify-write on the shared topology; serialize it.
        LinkMemGuard guard;
        f = linkmem->linkflags;
        f &= ~(1 << linkid);
        linkmem->linkflags = (uint8_t)f;
        // Stop driving GP pins for the peers that remain.
        linkmem->gp_rcnt[linkid] = 0;
        // numgbas is (highest still-connected slot) + 1. The old loop
        // wrote "for (i = 0; i < n; i--)", which decrements i in a "< n"
        // test -- it never scanned and left numgbas stale, corrupting the
        // topology seen by the peers that remained.
        int highest = 0;
        for (int i = 0; i < 4; i++)
            if (f & (1 << i))
                highest = i + 1;
        linkmem->numgbas = (uint8_t)highest;
    }

    // Retire our liveness token only after the flag above is cleared, so
    // no probe can see "flag set, process gone" during a clean close.
    LinkAliveRelease();

    for (int i = 0; i < 4; i++) {
        if (linksync[i] != NULL) {
#if (defined __WIN32__ || defined _WIN32)
            ReleaseSemaphore(linksync[i], 1, NULL);
            CloseHandle(linksync[i]);
#elif defined(__ANDROID__)
            // Owned by the shared mapping, which AndroidLinkShmClose()
            // unmaps below, so there is nothing to close or unlink. Don't
            // post either: the count is visible to every peer still in the
            // session, and a stray token there reads as a completed
            // transfer. Peers notice us leaving via linkflags, and every
            // wait on these is bounded by linktimeout anyway.
#else
            // Wake a peer blocked on this semaphore now rather than after
            // its full linktimeout (mirrors the Win32 branch); the master's
            // pre-start drain reclaims the stray token.
            sem_post(linksync[i]);
            sem_close(linksync[i]);
#endif
            linksync[i] = NULL;
        }
    }

    // Tear down the structural lock; name unlinking is the last-one-out
    // block's job below.
#if (defined __WIN32__ || defined _WIN32)
    if (linkmem_lock != NULL) {
        CloseHandle(linkmem_lock);
        linkmem_lock = NULL;
    }
#elif defined(__ANDROID__)
    // Part of the shared mapping; just forget it.
    linkmem_lock = SEM_FAILED;
#else
    if (linkmem_lock != SEM_FAILED) {
        sem_close(linkmem_lock);
        linkmem_lock = SEM_FAILED;
    }
#endif

#if (defined __WIN32__ || defined _WIN32)
    CloseHandle(mmf);
    UnmapViewOfFile(linkmem);
    mmf = NULL;
    linkmem = NULL;
#elif defined(__ANDROID__)
    // Unmaps the segment and, if we turn out to be the last participant,
    // removes the backing file. That check is a lock probe rather than the
    // linkflags test used above, so a session whose other members crashed
    // without clearing their flags still gets cleaned up.
    linkmem = NULL;
    AndroidLinkShmClose();
#else
    munmap(linkmem, sizeof(LINKDATA));
    linkmem = NULL;
    if (mmf >= 0)
        close(mmf);
    mmf = -1;

    // Last-one-out cleanup keys off the flock liveness probe rather than
    // linkflags: a crashed peer leaves its flag bit set (yet nobody is
    // alive, so the objects must go), while a live peer whose bit was
    // cleared by a drop must not have its session unlinked from under it.
    {
        LinkInitLock init_lock(LinkLockFilePath(".init"));
        bool last_out;
        if (link_liveness_fd >= 0) {
            // Drop our shared lock first; flock lock upgrades are not
            // atomic, so probe with a fresh descriptor instead.
            close(link_liveness_fd);
            link_liveness_fd = -1;
            last_out = false;
            int probe = open(LinkLockFilePath(".flock").c_str(), O_RDWR | O_CLOEXEC);
            if (probe >= 0) {
                last_out = (flock(probe, LOCK_EX | LOCK_NB) == 0);
                close(probe);
            }
        } else {
            last_out = !(f & 0xf); // degraded fallback (no lock file)
        }
        if (last_out) {
            shm_unlink(LinkShmName().c_str());
            for (int i = 0; i < 4; i++)
                sem_unlink(LinkSemName(i).c_str());
            sem_unlink(LinkLockSemName().c_str());
        }
    }
#endif

    // Don't leak an in-progress transfer into a later InitLink().
    transfer_direction = 0;
    numtransfers = 0;
    linktime = 0;
    GpResetState();
}
