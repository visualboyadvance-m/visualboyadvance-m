// This file was written by denopqrihg
// with major changes by tjm
#include <stdio.h>
#include <cstring>
#include <string>

// malloc.h does not seem to exist on Mac OS 10.7 and is an error on FreeBSD
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#define UPDATE_REG(address, value) WRITE16LE(((uint16_t*)&ioMem[address]), value)

static int vbaid = 0;
const char* MakeInstanceFilename(const char* Input)
{
    if (vbaid == 0)
        return Input;

    static char* result = NULL;
    if (result != NULL)
        free(result);

    result = (char*)malloc(strlen(Input) + 3);
    char* p = strrchr((char*)Input, '.');
    sprintf(result, "%.*s-%d.%s", (int)(p - Input), Input, vbaid + 1, p + 1);
    return result;
}

#ifndef NO_LINK

enum {
    SENDING = false,
    RECEIVING = true
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

// Joybus
bool gba_joybus_enabled = false;
bool gba_joybus_active = false;

// If disabled, gba core won't call any (non-joybus) link functions
bool gba_link_enabled = false;

bool speedhack = true;

#define LOCAL_LINK_NAME "VBA link memory"

#include <stdint.h>

uint32_t IP_LINK_PORT = 5738;

std::string IP_LINK_BIND_ADDRESS = "*";

#include "../common/Port.h"
#include "GBA.h"
#include "GBALink.h"
#include "GBASockClient.h"

#include <SFML/Network.hpp>

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(x) gettext(x)
#else
#define _(x) x
#endif
#define N_(x) x

#if (defined __WIN32__ || defined _WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <time.h>

#define ReleaseSemaphore(sem, nrel, orel) \
    do {                                  \
        for (int i = 0; i < nrel; i++)    \
            sem_post(sem);                \
    } while (0)
#define WAIT_TIMEOUT -1
#ifdef HAVE_SEM_TIMEDWAIT
int WaitForSingleObject(sem_t* s, int t)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += t / 1000;
    ts.tv_nsec += (t % 1000) * 1000000;
    do {
        if (!sem_timedwait(s, &ts))
            return 0;
    } while (errno == EINTR);
    return WAIT_TIMEOUT;
}

// urg.. MacOSX has no sem_timedwait (POSIX) or semtimedop (SYSV)
// so we'll have to simulate it..
// MacOSX also has no clock_gettime, and since both are "real-time", assume
// anyone who doesn't have one also doesn't have the other

// 2 ways to do this:
//   - poll & sleep loop
//   - poll & wait for timer interrupt loop

// the first consumes more CPU and requires selection of a good sleep value

// the second may interfere with other timers running on system, and
// requires that a dummy signal handler be installed for SIGALRM
#else
#include <sys/time.h>
#ifndef TIMEDWAIT_ALRM
#define TIMEDWAIT_ALRM 1
#endif
#if TIMEDWAIT_ALRM
#include <signal.h>
static void alrmhand(int sig)
{
}
#endif
int WaitForSingleObject(sem_t* s, int t)
{
#if !TIMEDWAIT_ALRM
    struct timeval ts;
    gettimeofday(&ts, NULL);
    ts.tv_sec += t / 1000;
    ts.tv_usec += (t % 1000) * 1000;
#else
    struct sigaction sa, osa;
    sigaction(SIGALRM, NULL, &osa);
    sa = osa;
    sa.sa_flags &= ~SA_RESTART;
    sa.sa_handler = alrmhand;
    sigaction(SIGALRM, &sa, NULL);
    struct itimerval tv, otv;
    tv.it_value.tv_sec = t / 1000;
    tv.it_value.tv_usec = (t % 1000) * 1000;
    // this should be 0/0, but in the wait loop, it's possible to
    // have the signal fire while not in sem_wait().  This will ensure
    // another signal within 1ms
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = 999;
    setitimer(ITIMER_REAL, &tv, &otv);
#endif
    while (1) {
#if !TIMEDWAIT_ALRM
        if (!sem_trywait(s))
            return 0;
        struct timeval ts2;
        gettimeofday(&ts2, NULL);
        if (ts2.tv_sec > ts.tv_sec || (ts2.tv_sec == ts.tv_sec && ts2.tv_usec > ts.tv_usec)) {
            return WAIT_TIMEOUT;
        }
        // is .1 ms short enough?  long enough?  who knows?
        struct timespec ts3;
        ts3.tv_sec = 0;
        ts3.tv_nsec = 100000;
        nanosleep(&ts3, NULL);
#else
        if (!sem_wait(s)) {
            setitimer(ITIMER_REAL, &otv, NULL);
            sigaction(SIGALRM, &osa, NULL);
            return 0;
        }
        getitimer(ITIMER_REAL, &tv);
        if (tv.it_value.tv_sec || tv.it_value.tv_usec > 999)
            continue;
        setitimer(ITIMER_REAL, &otv, NULL);
        sigaction(SIGALRM, &osa, NULL);
        break;
#endif
    }
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
static ConnectionState InitSocket();
static void StartCableSocket(uint16_t siocnt);
static ConnectionState ConnectUpdateSocket(char* const message, size_t size);
static void UpdateCableSocket(int ticks);
static void CloseSocket();

const uint64_t TICKS_PER_FRAME = TICKS_PER_SECOND / 60;
const uint64_t BITS_PER_SECOND = 115200;
const uint64_t BYTES_PER_SECOND = BITS_PER_SECOND / 8;

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
    uint32_t rfu_broadcastdata[5][7]; //for 0x16/0x1d/0x1e?
    uint32_t rfu_gdata[5]; //for 0x17/0x19?/0x1e?
    int32_t rfu_state[5]; //0=none, 1=waiting for ACK
    uint8_t rfu_listfront[5];
    uint8_t rfu_listback[5];
    rfu_datarec rfu_datalist[5][256];

    /*uint16_t rfu_qidlist[5][256];
	uint16_t rfu_qlist[5][256];
	uint32_t rfu_datalist[5][256][255];
	uint32_t rfu_timelist[5][256];*/
} LINKDATA;

class RFUServer {
    int numbytes;
    sf::SocketSelector fdset;
    int counter;
    int done;
    uint8_t current_host;

public:
    sf::TcpSocket tcpsocket[5];
    sf::IpAddress udpaddr[5];
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
    sf::IpAddress serveraddr;
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
static LINKDATA* linkmem = NULL;
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

#if (defined __WIN32__ || defined _WIN32)

static ConnectionState InitIPC();
static void StartCableIPC(uint16_t siocnt);
static void ReconnectCableIPC();
static void UpdateCableIPC(int ticks);
static void StartRFU(uint16_t siocnt);
static void UpdateRFUIPC(int ticks);
static void CloseIPC();

#endif

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
#if (defined __WIN32__ || defined _WIN32)
    { LINK_CABLE_IPC, InitIPC, NULL, StartCableIPC, UpdateCableIPC, CloseIPC, false },
    { LINK_RFU_IPC, InitIPC, NULL, StartRFU, UpdateRFUIPC, CloseIPC, false },
    { LINK_GAMEBOY_IPC, InitIPC, NULL, NULL, NULL, CloseIPC, false },
#endif
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
    int numslaves;
    int connectedSlaves;
    int type;
    bool server;
    bool speed; //speedhack
} LANLINKDATA;

class CableServer {
    int numbytes;
    sf::SocketSelector fdset;
    //timeval udptimeout;
    char inbuffer[256], outbuffer[256];
    int32_t* intinbuffer;
    uint16_t* uint16_tinbuffer;
    int32_t* intoutbuffer;
    uint16_t* uint16_toutbuffer;
    int counter;
    int done;

public:
    sf::TcpSocket tcpsocket[4];
    sf::IpAddress udpaddr[4];
    CableServer(void);
    void Send(void);
    void Recv(void);
    void SendGB(void);
    bool RecvGB(void);
};

class CableClient {
    sf::SocketSelector fdset;
    char inbuffer[256], outbuffer[256];
    int32_t* intinbuffer;
    uint16_t* uint16_tinbuffer;
    int32_t* intoutbuffer;
    uint16_t* uint16_toutbuffer;
    int numbytes;

public:
    sf::IpAddress serveraddr;
    unsigned short serverport;
    bool transferring;
    CableClient(void);
    void Send(void);
    void Recv(void);
    void SendGB(void);
    bool RecvGB(void);
    void CheckConn(void);
};

static int i, j;
static int linktimeout = 1;
static LANLINKDATA lanlink;
static uint16_t cable_data[4];
static uint8_t cable_gb_data[4];
static CableServer ls;
static CableClient lc;

// time to end of single GBA's transfer, in 16.78 MHz clock ticks
// first index is GBA #
static const int trtimedata[4][4] = {
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

// Hodgepodge
static uint8_t tspeed = 3;
static bool transfer_direction = false;
static int linkid = 0;
#if (defined __WIN32__ || defined _WIN32)
static HANDLE linksync[4];
#else
static sem_t* linksync[4];
#endif
static int transfer_start_time_from_master = 0;
#if (defined __WIN32__ || defined _WIN32)
static HANDLE mmf = NULL;
#else
static int mmf = -1;
#endif
static char linkevent[] =
#if !(defined __WIN32__ || defined _WIN32)
    "/"
#endif
    "VBA link event  ";

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

LinkMode GetLinkMode()
{
    if (linkDriver && gba_connection_state == LINK_OK)
        return linkDriver->mode;
    else
        return LINK_DISCONNECTED;
}

void GetLinkServerHost(char* const host, size_t size)
{
    if (host == NULL || size == 0)
        return;

    host[0] = '\0';

    if (linkDriver && linkDriver->mode == LINK_GAMECUBE_DOLPHIN)
        strncpy(host, joybusHostAddr.toString().c_str(), size);
    else if (lanlink.server) {
        if (IP_LINK_BIND_ADDRESS == "*")
            strncpy(host, sf::IpAddress::getLocalAddress().toString().c_str(), size);
        else
            strncpy(host, IP_LINK_BIND_ADDRESS.c_str(), size);
    }
    else
        strncpy(host, lc.serveraddr.toString().c_str(), size);
}

bool SetLinkServerHost(const char* host)
{
    sf::IpAddress addr = sf::IpAddress(host);

    lc.serveraddr = addr;
    joybusHostAddr = addr;

    return addr != sf::IpAddress::None;
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
    lanlink.numslaves = numSlaves;
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

    // Connect the link
    gba_connection_state = linkDriver->connect();

    if (gba_connection_state == LINK_ERROR) {
        CloseLink();
    }

    return gba_connection_state;
}

void StartLink(uint16_t siocnt)
{
    if (!linkDriver || !linkDriver->start) {
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
    UPDATE_REG(COMM_RCNT, value);

    if (!value)
        return;

    switch (GetSIOMode(READ16LE(&ioMem[COMM_SIOCNT]), value)) {
    case MULTIPLAYER:
        value &= 0xc0f0;
        value |= 3;
        if (linkid)
            value |= 4;
        UPDATE_REG(COMM_SIOCNT, ((READ16LE(&ioMem[COMM_SIOCNT]) & 0xff8b) | (linkid ? 0xc : 8) | (linkid << 4)));
        break;

    case GP:
#if (defined __WIN32__ || defined _WIN32)
        if (GetLinkMode() == LINK_RFU_IPC)
            rfu_state = RFU_INIT;
#endif
        break;
    }
}

void LinkUpdate(int ticks)
{
    if (!linkDriver || !linkDriver->update) {
        return;
    }

    // this actually gets called every single instruction, so keep default
    // path as short as possible

    linktime += ticks;

    linkDriver->update(ticks);
}

void CheckLinkConnection()
{
    if (GetLinkMode() == LINK_CABLE_SOCKET) {
        if (linkid && !lc.transferring) {
            lc.CheckConn();
        }
    }
}

void CloseLink(void)
{
    if (!linkDriver || !linkDriver->close) {
        return; // Nothing to do
    }

    linkDriver->close();
    linkDriver = NULL;

    return;
}

// Server
CableServer::CableServer(void)
{
    intinbuffer = (int32_t*)inbuffer;
    uint16_tinbuffer = (uint16_t*)inbuffer;
    intoutbuffer = (int32_t*)outbuffer;
    uint16_toutbuffer = (uint16_t*)outbuffer;
}

void CableServer::Send(void)
{
    if (lanlink.type == 0) { // TCP
        outbuffer[1] = tspeed;
        WRITE16LE(&uint16_toutbuffer[1], cable_data[0]);
        WRITE32LE(&intoutbuffer[1], transfer_start_time_from_master);

        if (lanlink.numslaves == 1) {
            if (lanlink.type == 0) {
                outbuffer[0] = 8;
                tcpsocket[1].send(outbuffer, 8);
            }
        } else if (lanlink.numslaves == 2) {
            WRITE16LE(&uint16_toutbuffer[4], cable_data[2]);
            if (lanlink.type == 0) {
                outbuffer[0] = 10;
                tcpsocket[1].send(outbuffer, 10);
                WRITE16LE(&uint16_toutbuffer[4], cable_data[1]);
                tcpsocket[2].send(outbuffer, 10);
            }
        } else {
            if (lanlink.type == 0) {
                outbuffer[0] = 12;
                WRITE16LE(&uint16_toutbuffer[4], cable_data[2]);
                WRITE16LE(&uint16_toutbuffer[5], cable_data[3]);
                tcpsocket[1].send(outbuffer, 12);
                WRITE16LE(&uint16_toutbuffer[4], cable_data[1]);
                tcpsocket[2].send(outbuffer, 12);
                WRITE16LE(&uint16_toutbuffer[5], cable_data[2]);
                tcpsocket[3].send(outbuffer, 12);
            }
        }
    }
    return;
}

// Receive data from all slaves to master
void CableServer::Recv(void)
{
    int numbytes;
    if (lanlink.type == 0) { // TCP
        fdset.clear();

        for (i = 0; i < lanlink.numslaves; i++)
            fdset.add(tcpsocket[i + 1]);

        if (fdset.wait(sf::milliseconds(50)) == 0) {
            return;
        }

        for (i = 0; i < lanlink.numslaves; i++) {
            numbytes = 0;
            inbuffer[0] = 1;
            while (numbytes < inbuffer[0]) {
                size_t nr;
                tcpsocket[i + 1].receive(inbuffer + numbytes, inbuffer[0] - numbytes, nr);
                numbytes += nr;
            }
            if (inbuffer[1] == -32) {
                char message[30];
                sprintf(message, _("Player %d disconnected."), i + 2);
                systemScreenMessage(message);
                outbuffer[0] = 4;
                outbuffer[1] = -32;
                for (i = 1; i < lanlink.numslaves; i++) {
                    tcpsocket[i].send(outbuffer, 12);
                    size_t nr;
                    tcpsocket[i].receive(inbuffer, 256, nr);
                    tcpsocket[i].disconnect();
                }
                CloseLink();
                return;
            }
            cable_data[i + 1] = READ16LE(&uint16_tinbuffer[1]);
        }
    }
    return;
}

void CableServer::SendGB(void)
{
    if (counter == 0)
        return;

    if (lanlink.type == 0) { // TCP
        if (lanlink.numslaves == 1) {
            if (lanlink.type == 0) {
                tcpsocket[1].send(&cable_gb_data[0], 1);
            }
        }
    }
    counter = 0;
}

// Receive data from all slaves to master
bool CableServer::RecvGB(void)
{
    if (counter == 1)
        return false;

    int numbytes = 0;
    if (lanlink.type == 0) { // TCP
        fdset.clear();

        for (i = 0; i < lanlink.numslaves; i++)
            fdset.add(tcpsocket[i + 1]);

        if (fdset.wait(sf::milliseconds(1)) == 0) {
            return false;
        }

        for (i = 0; i < lanlink.numslaves; i++) {
            numbytes = 0;
            uint8_t recv_byte = 0;

            size_t nr;
            tcpsocket[i + 1].receive(&recv_byte, 1, nr);
            numbytes += nr;

            if (numbytes != 0)
                counter = 1;

            if (inbuffer[1] == -32) {
                char message[30];
                sprintf(message, _("Player %d disconnected."), i + 2);
                systemScreenMessage(message);
                for (i = 1; i < lanlink.numslaves; i++) {
                    tcpsocket[i].disconnect();
                }
                CloseLink();
                return false;
            }
            if (numbytes > 0)
                cable_gb_data[i + 1] = recv_byte;
        }
    }

    return numbytes != 0;
}

// Client
CableClient::CableClient(void)
{
    intinbuffer = (int32_t*)inbuffer;
    uint16_tinbuffer = (uint16_t*)inbuffer;
    intoutbuffer = (int32_t*)outbuffer;
    uint16_toutbuffer = (uint16_t*)outbuffer;
    transferring = false;
    return;
}

void CableClient::CheckConn(void)
{
    size_t nr;
    lanlink.tcpsocket.receive(inbuffer, 1, nr);
    numbytes = nr;
    if (numbytes > 0) {
        while (numbytes < inbuffer[0]) {
            lanlink.tcpsocket.receive(inbuffer + numbytes, inbuffer[0] - numbytes, nr);
            numbytes += nr;
        }
        if (inbuffer[1] == -32) {
            outbuffer[0] = 4;
            lanlink.tcpsocket.send(outbuffer, 4);
            systemScreenMessage(_("Server disconnected."));
            CloseLink();
            return;
        }
        transferring = true;
        transfer_start_time_from_master = 0;
        cable_data[0] = READ16LE(&uint16_tinbuffer[1]);
        tspeed = inbuffer[1] & 3;
        for (i = 1, numbytes = 4; i <= lanlink.numslaves; i++)
            if (i != linkid) {
                cable_data[i] = READ16LE(&uint16_tinbuffer[numbytes]);
                numbytes++;
            }
    }
    return;
}

bool CableClient::RecvGB(void)
{
    if (!transferring)
        return false;

    fdset.clear();
    // old code used socket # instead of mask again
    fdset.add(lanlink.tcpsocket);
    // old code stripped off ms again
    if (fdset.wait(sf::milliseconds(1)) == 0) {
        return false;
    }
    numbytes = 0;
    size_t nr;
    uint8_t recv_byte = 0;

    lanlink.tcpsocket.receive(&recv_byte, 1, nr);
    numbytes += nr;

    if (numbytes != 0)
        transferring = false;

    if (inbuffer[1] == -32) {
        systemScreenMessage(_("Server disconnected."));
        CloseLink();
        return false;
    }
    if (numbytes > 0)
        cable_gb_data[0] = recv_byte;

    return numbytes != 0;
}

void CableClient::SendGB()
{
    if (transferring)
        return;

    lanlink.tcpsocket.send(&cable_gb_data[1], 1);

    transferring = true;
}

void CableClient::Recv(void)
{
    fdset.clear();
    // old code used socket # instead of mask again
    fdset.add(lanlink.tcpsocket);
    // old code stripped off ms again
    if (fdset.wait(sf::milliseconds(50)) == 0) {
        transferring = false;
        return;
    }
    numbytes = 0;
    inbuffer[0] = 1;
    size_t nr;
    while (numbytes < inbuffer[0]) {
        lanlink.tcpsocket.receive(inbuffer + numbytes, inbuffer[0] - numbytes, nr);
        numbytes += nr;
    }
    if (inbuffer[1] == -32) {
        outbuffer[0] = 4;
        lanlink.tcpsocket.send(outbuffer, 4);
        systemScreenMessage(_("Server disconnected."));
        CloseLink();
        return;
    }
    tspeed = inbuffer[1] & 3;
    cable_data[0] = READ16LE(&uint16_tinbuffer[1]);
    transfer_start_time_from_master = (int32_t)READ32LE(&intinbuffer[1]);
    for (i = 1, numbytes = 4; i < lanlink.numslaves + 1; i++) {
        if (i != linkid) {
            cable_data[i] = READ16LE(&uint16_tinbuffer[numbytes]);
            numbytes++;
        }
    }
}

void CableClient::Send()
{
    outbuffer[0] = 4;
    outbuffer[1] = linkid << 2;
    WRITE16LE(&uint16_toutbuffer[1], cable_data[linkid]);
    lanlink.tcpsocket.send(outbuffer, 4);
    return;
}

static ConnectionState InitSocket()
{
    linkid = 0;

    for (int i = 0; i < 4; i++)
        cable_data[i] = 0xffff;

    for (int i = 0; i < 4; i++)
        cable_gb_data[i] = 0xff;

    if (lanlink.server) {
        lanlink.connectedSlaves = 0;
        // should probably use GetPublicAddress()
        //sid->ShowServerIP(sf::IpAddress::getLocalAddress());

        // too bad Listen() doesn't take an address as well
        // then again, old code used INADDR_ANY anyway
        sf::IpAddress bind_ip = IP_LINK_BIND_ADDRESS == "*" ? sf::IpAddress::Any : IP_LINK_BIND_ADDRESS;

        if (lanlink.tcplistener.listen(IP_LINK_PORT, bind_ip) == sf::Socket::Error)
            // Note: old code closed socket & retried once on bind failure
            return LINK_ERROR; // FIXME: error code?
        else
            return LINK_NEEDS_UPDATE;
    } else {
        lc.serverport = IP_LINK_PORT;

        if (lc.serveraddr == sf::IpAddress::None) {
            return LINK_ERROR;
        } else {
            lanlink.tcpsocket.setBlocking(false);
            sf::Socket::Status status = lanlink.tcpsocket.connect(lc.serveraddr, lc.serverport);

            if (status == sf::Socket::Error || status == sf::Socket::Disconnected)
                return LINK_ERROR;
            else
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
            int nextSlave = lanlink.connectedSlaves + 1;

            sf::Socket::Status st = lanlink.tcplistener.accept(ls.tcpsocket[nextSlave]);

            if (st == sf::Socket::Error) {
                for (int j = 1; j < nextSlave; j++)
                    ls.tcpsocket[j].disconnect();

                snprintf(message, size, N_("Network error."));
                newState = LINK_ERROR;
            } else {
                sf::Packet packet;
                packet << static_cast<sf::Uint16>(nextSlave)
                       << static_cast<sf::Uint16>(lanlink.numslaves);

                ls.tcpsocket[nextSlave].send(packet);

                snprintf(message, size, N_("Player %d connected"), nextSlave);

                lanlink.connectedSlaves++;
            }
        }

        if (lanlink.numslaves == lanlink.connectedSlaves) {
            for (int i = 1; i <= lanlink.numslaves; i++) {
                sf::Packet packet;
                packet << true;

                ls.tcpsocket[i].send(packet);
            }

            snprintf(message, size, N_("All players connected"));
            newState = LINK_OK;
        }
    } else {

        sf::Packet packet;
        sf::Socket::Status status = lanlink.tcpsocket.receive(packet);

        if (status == sf::Socket::Error || status == sf::Socket::Disconnected) {
            snprintf(message, size, N_("Network error."));
            newState = LINK_ERROR;
        } else if (status == sf::Socket::Done) {

            if (linkid == 0) {
                sf::Uint16 receivedId, receivedSlaves;
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
            fdset.wait(sf::milliseconds(150));
        }
    }

    return newState;
}

void StartCableSocket(uint16_t value)
{
    switch (GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]))) {
    case MULTIPLAYER: {
        bool start = (value & 0x80) && !linkid && !transfer_direction;
        // clear start, seqno, si (RO on slave, start = pulse on master)
        value &= 0xff4b;
        // get current si.  This way, on slaves, it is low during xfer
        if (linkid) {
            if (!transfer_direction)
                value |= 4;
            else
                value |= READ16LE(&ioMem[COMM_SIOCNT]) & 4;
        }
        if (start) {
            cable_data[0] = READ16LE(&ioMem[COMM_SIODATA8]);
            transfer_start_time_from_master = linktime;
            tspeed = value & 3;
            ls.Send();
            transfer_direction = RECEIVING;
            linktime = 0;
            UPDATE_REG(COMM_SIOMULTI0, cable_data[0]);
            UPDATE_REG(COMM_SIOMULTI1, 0xffff);
            WRITE32LE(&ioMem[COMM_SIOMULTI2], 0xffffffff);
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

static void UpdateCableSocket(int ticks)
{
    (void)ticks; // unused param
    if (linkid && transfer_direction == SENDING && lc.transferring && linktime >= transfer_start_time_from_master) {
        cable_data[linkid] = READ16LE(&ioMem[COMM_SIODATA8]);

        lc.Send();
        UPDATE_REG(COMM_SIODATA32_L, cable_data[0]);
        UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) | 0x80);
        transfer_direction = RECEIVING;
        linktime = 0;
    }

    if (transfer_direction == RECEIVING && linktime >= trtimeend[lanlink.numslaves - 1][tspeed]) {
        if (READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000) {
            IF |= 0x80;
            UPDATE_REG(0x202, IF);
        }

        UPDATE_REG(COMM_SIOCNT, (READ16LE(&ioMem[COMM_SIOCNT]) & 0xff0f) | (linkid << 4));
        transfer_direction = SENDING;
        linktime -= trtimeend[lanlink.numslaves - 1][tspeed];

        if (linkid) {
            lc.transferring = true;
            lc.Recv();
        } else {
            ls.Recv(); // Receive data from all of the slaves
        }
        UPDATE_REG(COMM_SIOMULTI1, cable_data[1]);
        UPDATE_REG(COMM_SIOMULTI2, cable_data[2]);
        UPDATE_REG(COMM_SIOMULTI3, cable_data[3]);
    }
}

static void CloseSocket()
{
    if (linkid) {
        char outbuffer[4];
        outbuffer[0] = 4;
        outbuffer[1] = -32;
        if (lanlink.type == 0)
            lanlink.tcpsocket.send(outbuffer, 4);
    } else {
        char outbuffer[12];
        int i;
        outbuffer[0] = 12;
        outbuffer[1] = -32;
        for (i = 1; i <= lanlink.numslaves; i++) {
            if (lanlink.type == 0) {
                ls.tcpsocket[i].send(outbuffer, 12);
            }
            ls.tcpsocket[i].disconnect();
        }
    }
    lanlink.tcpsocket.disconnect();
}

// call this to clean up crashed program's shared state
// or to use TCP on same machine (for testing)
// this may be necessary under MSW as well, but I wouldn't know how
void CleanLocalLink()
{
#if !(defined __WIN32__ || defined _WIN32)
    shm_unlink("/" LOCAL_LINK_NAME);
    for (int i = 0; i < 4; i++) {
        linkevent[sizeof(linkevent) - 2] = '1' + i;
        sem_unlink(linkevent);
    }
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

    bool joybus_activated = ((READ16LE(&ioMem[COMM_RCNT])) >> 14) == 3;
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
            UPDATE_REG(COMM_JOYCNT, READ16LE(&ioMem[COMM_JOYCNT]) | JOYCNT_RESET);
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
            resp.push_back((uint8_t)(READ16LE(&ioMem[COMM_JOY_TRANS_L]) & 0xff));
            resp.push_back((uint8_t)(READ16LE(&ioMem[COMM_JOY_TRANS_L]) >> 8));
            resp.push_back((uint8_t)(READ16LE(&ioMem[COMM_JOY_TRANS_H]) & 0xff));
            resp.push_back((uint8_t)(READ16LE(&ioMem[COMM_JOY_TRANS_H]) >> 8));

            UPDATE_REG(COMM_JOYCNT, READ16LE(&ioMem[COMM_JOYCNT]) | JOYCNT_SEND_COMPLETE);
            nextjoybusupdate = TICKS_PER_SECOND / BYTES_PER_SECOND;
            booted = true;
            break;

        case JOY_CMD_WRITE:
            UPDATE_REG(COMM_JOY_RECV_L, (uint16_t)((uint16_t)data[2] << 8) | (uint8_t)data[1]);
            UPDATE_REG(COMM_JOY_RECV_H, (uint16_t)((uint16_t)data[4] << 8) | (uint8_t)data[3]);
            UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) | JOYSTAT_RECV);
            UPDATE_REG(COMM_JOYCNT, READ16LE(&ioMem[COMM_JOYCNT]) | JOYCNT_RECV_COMPLETE);
            nextjoybusupdate = TICKS_PER_SECOND / BYTES_PER_SECOND;
            booted = true;
            break;

        default:
            nextjoybusupdate = TICKS_PER_SECOND / 40000;
            lastjoybusupdate = 0;
            return; // ignore
        }

        lastjoybusupdate = 0;
        resp.push_back((uint8_t)READ16LE(&ioMem[COMM_JOYSTAT]));

        if (cmd == JOY_CMD_READ) {
            UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) & ~JOYSTAT_SEND);
        }

        dol->Send(resp);

        // Generate SIO interrupt if we can
        if (((cmd == JOY_CMD_RESET) || (cmd == JOY_CMD_READ) || (cmd == JOY_CMD_WRITE))
            && (READ16LE(&ioMem[COMM_JOYCNT]) & JOYCNT_INT_ENABLE)) {
            IF |= 0x80;
            UPDATE_REG(0x202, IF);
        }

        lastcommand = 0;
    }
}

static void JoyBusShutdown()
{
    delete dol;
    dol = NULL;
}

#define MAX_CLIENTS lanlink.numslaves + 1

// Server
RFUServer::RFUServer(void)
{
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
        current_host = slave;
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
    if (lanlink.type == 0) { // TCP
        sf::Packet packet;
        if (lanlink.numslaves == 1) {
            if (lanlink.type == 0) {
                tcpsocket[1].send(Serialize(packet, 1));
            }
        } else if (lanlink.numslaves == 2) {
            if (lanlink.type == 0) {
                tcpsocket[1].send(Serialize(packet, 1));
                tcpsocket[2].send(Serialize(packet, 2));
            }
        } else {
            if (lanlink.type == 0) {
                tcpsocket[1].send(Serialize(packet, 1));
                tcpsocket[2].send(Serialize(packet, 2));
                tcpsocket[3].send(Serialize(packet, 3));
            }
        }
    }
}

// Receive data from all slaves to master
void RFUServer::Recv(void)
{
    //int numbytes;
    if (lanlink.type == 0) { // TCP
        fdset.clear();

        for (i = 0; i < lanlink.numslaves; i++)
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

        for (i = 0; i < lanlink.numslaves; i++) {
            sf::Packet packet;
            tcpsocket[i + 1].setBlocking(false);
            sf::Socket::Status status = tcpsocket[i + 1].receive(packet);
            if (status == sf::Socket::Disconnected) {
                char message[30];
                sprintf(message, _("Player %d disconnected."), i + 1);
                systemScreenMessage(message);
                //tcpsocket[i + 1].disconnect();
                //CloseLink();
                //return;
            }
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
    lanlink.tcpsocket.send(Serialize(packet));
}

void RFUClient::Recv(void)
{
    if (rfu_data.numgbas < 2)
        return;

    fdset.clear();
    // old code used socket # instead of mask again
    lanlink.tcpsocket.setBlocking(false);
    fdset.add(lanlink.tcpsocket);
    if (fdset.wait(sf::milliseconds(166)) == 0) {
        systemScreenMessage(_("Server timed out."));
        //transferring = false;
        //return;
    }
    sf::Packet packet;
    sf::Socket::Status status = lanlink.tcpsocket.receive(packet);
    if (status == sf::Socket::Disconnected) {
        systemScreenMessage(_("Server disconnected."));
        CloseLink();
        return;
    }
    DeSerialize(packet);
}

static ConnectionState ConnectUpdateRFUSocket(char* const message, size_t size)
{
    ConnectionState newState = LINK_NEEDS_UPDATE;

    if (lanlink.server) {
        sf::SocketSelector fdset;
        fdset.add(lanlink.tcplistener);

        if (fdset.wait(sf::milliseconds(150))) {
            int nextSlave = lanlink.connectedSlaves + 1;

            sf::Socket::Status st = lanlink.tcplistener.accept(rfu_server.tcpsocket[nextSlave]);

            if (st == sf::Socket::Error) {
                for (int j = 1; j < nextSlave; j++)
                    rfu_server.tcpsocket[j].disconnect();

                snprintf(message, size, N_("Network error."));
                newState = LINK_ERROR;
            } else {
                sf::Packet packet;
                packet << static_cast<sf::Uint16>(nextSlave)
                       << static_cast<sf::Uint16>(lanlink.numslaves);

                rfu_server.tcpsocket[nextSlave].send(packet);

                snprintf(message, size, N_("Player %d connected"), nextSlave);
                lanlink.connectedSlaves++;
            }
        }

        if (lanlink.numslaves == lanlink.connectedSlaves) {
            for (int i = 1; i <= lanlink.numslaves; i++) {
                sf::Packet packet;
                packet << true;

                rfu_server.tcpsocket[i].send(packet);
                rfu_server.tcpsocket[i].setBlocking(false);
            }

            snprintf(message, size, N_("All players connected"));
            newState = LINK_OK;
        }
    } else {

        sf::Packet packet;
        lanlink.tcpsocket.setBlocking(false);
        sf::Socket::Status status = lanlink.tcpsocket.receive(packet);

        if (status == sf::Socket::Error || status == sf::Socket::Disconnected) {
            snprintf(message, size, N_("Network error."));
            newState = LINK_ERROR;
        } else if (status == sf::Socket::Done) {

            if (linkid == 0) {
                sf::Uint16 receivedId, receivedSlaves;
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
            fdset.wait(sf::milliseconds(150));
        }
    }

    rfu_data.numgbas = lanlink.numslaves + 1;
    log("num gbas: %d\n", rfu_data.numgbas);

    return newState;
}

// The GBA wireless RFU (see adapter3.txt)
static void StartRFUSocket(uint16_t value)
{
    int siomode = GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]));

    if (value)
        rfu_enabled = (siomode == NORMAL32);

    if (((READ16LE(&ioMem[COMM_SIOCNT]) & 0x5080) == SIO_TRANS_32BIT) && ((value & 0x5080) == (SIO_TRANS_32BIT | SIO_IRQ_ENABLE | SIO_TRANS_START))) { //RFU Reset, may also occur before cable link started
        rfu_data.rfu_listfront[linkid] = 0;
        rfu_data.rfu_listback[linkid] = 0;
    }

    if (!rfu_enabled) {
        if ((value & 0x5080) == (SIO_TRANS_32BIT | SIO_IRQ_ENABLE | SIO_TRANS_START)) { //0x5083 //game tried to send wireless command but w/o the adapter
            if (READ16LE(&ioMem[COMM_SIOCNT]) & SIO_IRQ_ENABLE) //IRQ Enable
            {
                IF |= 0x80; //Serial Communication
                UPDATE_REG(0x202, IF); //Interrupt Request Flags / IRQ Acknowledge
            }
            value &= ~SIO_TRANS_START; //Start bit.7 reset //may cause the game to retry sending again
            value |= SIO_TRANS_FLAG_SEND_DISABLE; //SO bit.3 set automatically upon transfer completion
            transfer_direction = SENDING;
        }
        return;
    }

    uint32_t CurCOM = 0, CurDAT = 0;

    switch (GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]))) {
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

            uint16_t siodata_h = READ16LE(&ioMem[COMM_SIODATA32_H]);
            switch (rfu_state) {
            case RFU_INIT:
                if (READ32LE(&ioMem[COMM_SIODATA32_L]) == 0xb0bb8001) {
                    rfu_state = RFU_COMM; // end of startup
                    rfu_initialized = true;
                    value &= ~SIO_TRANS_FLAG_RECV_ENABLE; //0xff7b; //Bit.2 need to be 0 to indicate a finished initialization to fix MarioGolfAdv from occasionally Not Detecting wireless adapter (prevent it from sending 0x7FFE8001 comm)?
                    rfu_polarity = 0; //not needed?
                }
                rfu_buf = (READ16LE(&ioMem[COMM_SIODATA32_L]) << 16) | siodata_h;
                break;
            case RFU_COMM:
                CurCOM = READ32LE(&ioMem[COMM_SIODATA32_L]);
                if (siodata_h == 0x9966) //initialize cmd
                {
                    uint8_t tmpcmd = CurCOM;
                    if (tmpcmd != 0x10 && tmpcmd != 0x11 && tmpcmd != 0x13 && tmpcmd != 0x14 && tmpcmd != 0x16 && tmpcmd != 0x17 && tmpcmd != 0x19 && tmpcmd != 0x1a && tmpcmd != 0x1b && tmpcmd != 0x1c && tmpcmd != 0x1d && tmpcmd != 0x1e && tmpcmd != 0x1f && tmpcmd != 0x20 && tmpcmd != 0x21 && tmpcmd != 0x24 && tmpcmd != 0x25 && tmpcmd != 0x26 && tmpcmd != 0x27 && tmpcmd != 0x30 && tmpcmd != 0x32 && tmpcmd != 0x33 && tmpcmd != 0x34 && tmpcmd != 0x3d && tmpcmd != 0xa8 && tmpcmd != 0xee) {
                    }
                    rfu_counter = 0;
                    if ((rfu_qsend2 = rfu_qsend = ioMem[0x121]) != 0) { //COMM_SIODATA32_L+1, following data [to send]
                        rfu_state = RFU_SEND;
                    }
                    if (ioMem[COMM_SIODATA32_L] == 0xee) { //0xee cmd shouldn't override previous cmd
                        rfu_lastcmd = rfu_cmd2;
                        rfu_cmd2 = ioMem[COMM_SIODATA32_L];
                    } else {
                        rfu_lastcmd = rfu_cmd;
                        rfu_cmd = ioMem[COMM_SIODATA32_L];
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
                        rfu_buf = READ32LE(&ioMem[COMM_SIODATA32_L]);
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
                            rfu_id = (gbaid << 3) + 0x61f1;
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x1f: // join a room as client
                            // TODO: to fix infinte send&recv w/o giving much cance to update the screen when both side acting as client
                            // on MarioGolfAdv lobby(might be due to leftover data when switching from host to join mode at the same time?)
                            rfu_id = rfu_masterdata[0];
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
                                            rfu_id = (gbaid << 3) + 0x61f1;
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
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].gbaid = linkid;
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].len = rfu_qsend2;
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].time = linktime;
                                            rfu_data.rfu_listback[j]++;
                                        }
                                } else if (linkid != gbaid) {
                                    memcpy(rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].data, rfu_masterdata, 4 * rfu_qsend2);
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].gbaid = linkid;
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
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].gbaid = linkid;
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].len = rfu_qsend2;
                                            rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].time = linktime;
                                            rfu_data.rfu_listback[j]++;
                                        }
                                } else if (linkid != gbaid) {
                                    memcpy(rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].data, rfu_masterdata, 4 * rfu_qsend2);
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].gbaid = linkid;
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].len = rfu_qsend2;
                                    rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].time = linktime;
                                    rfu_data.rfu_listback[gbaid]++;
                                }
                            } else {
                                log("IgnoredSend[%02X] %d\n", rfu_cmd, rfu_qsend2);
                            }
                        //TODO: there is still a chance for 0x25 to be used at the same time on both GBA (both GBAs acting as client but keep sending & receiving using 0x25 & 0x26 for infinity w/o updating the screen much)
                        //Waiting here for previous data to be received might be too late! as new data already sent before finalization cmd
                        case 0x27: // wait for data ?
                        case 0x37: // wait for data ?
                            rfu_data.rfu_linktime[linkid] = linktime; //save the ticks before changed to synchronize performance

                            if (rfu_ishost) {
                                for (int j = 0; j < rfu_data.numgbas; j++)
                                    if (j != linkid) {
                                        rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].gbaid = linkid;
                                        rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].len = 0; //rfu_qsend2;
                                        rfu_data.rfu_datalist[j][rfu_data.rfu_listback[j]].time = linktime;
                                        rfu_data.rfu_listback[j]++;
                                    }
                            } else if (linkid != gbaid) {
                                rfu_data.rfu_datalist[gbaid][rfu_data.rfu_listback[gbaid]].gbaid = linkid;
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
                            rfu_buf = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    }
                } else { //unknown COMM word //in MarioGolfAdv (when a player/client exiting lobby), There is a possibility COMM = 0x7FFE8001, PrevVAL = 0x5087, PrevCOM = 0, is this part of initialization?
                    log("%09d: UnkCOM %08X  %04X  %08X %08X\n", linktime, READ32LE(&ioMem[COMM_SIODATA32_L]), PrevVAL, PrevCOM, PrevDAT);
                    if ((READ32LE(&ioMem[COMM_SIODATA32_L]) >> 24) != 0x7ff)
                        rfu_state = RFU_INIT; //to prevent the next reinit words from getting in finalization processing (here), may cause MarioGolfAdv to show Linking error when this occurs instead of continuing with COMM cmd
                    rfu_buf = (READ16LE(&ioMem[COMM_SIODATA32_L]) << 16) | siodata_h;
                }
                break;

            case RFU_SEND: //data following after initialize cmd
                CurDAT = READ32LE(&ioMem[COMM_SIODATA32_L]);
                if (--rfu_qsend == 0) {
                    rfu_state = RFU_COMM;
                }

                switch (rfu_cmd) {
                case 0x16:
                    rfu_data.rfu_broadcastdata[linkid][1 + rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x17:
                    rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x1f:
                    rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x24:
                //if(rfu_data.rfu_proto[linkid]) break; //important data from 0x25 shouldn't be overwritten by 0x24
                case 0x25:
                case 0x35:
                    rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    break;

                default:
                    rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
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
    default:
        UPDATE_REG(COMM_SIOCNT, value);
        return;
    }
    UPDATE_REG(COMM_SIOCNT, value);
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

                        if (READ32LE(&ioMem[COMM_SIODATA32_L]) == 0x80000000)
                            rfu_buf = 0x99660000 | (rfu_qrecv_broadcast_data_len << 8) | rfu_cmd;
                        else
                            rfu_buf = 0x80000000;
                    }
                    rfu_waiting = false;
                }
            }
            UPDATE_REG(COMM_SIODATA32_L, rfu_buf);
            UPDATE_REG(COMM_SIODATA32_H, rfu_buf >> 16);
        }
    }
    return true;
}

static void UpdateRFUSocket(int ticks)
{
    rfu_last_broadcast_time -= ticks;

    if (rfu_last_broadcast_time < 0) {
        if (linkid == 0) {
            linktime = 0;
            rfu_server.Recv(); // recv broadcast data
            rfu_server.Send(); // send broadcast data
        } else {
            rfu_client.Send(); // send broadcast data
            rfu_client.Recv(); // recv broadcast data
        }
        {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (i != linkid) {
                    rfu_data.rfu_listback[i] = 0; // Flush the queue
                }
            }
        }
        rfu_transfer_end = 0;

        if (rfu_last_broadcast_time < 0)
            rfu_last_broadcast_time = 3000;
        //rfu_last_broadcast_time = 5600; // Upper physical limit of 5600? 3000 packets/sec
    }

    if (rfu_enabled) {
        if (LinkRFUUpdateSocket()) {
            if (transfer_direction == RECEIVING && rfu_transfer_end <= 0) {
                transfer_direction = SENDING;
                uint16_t value = READ16LE(&ioMem[COMM_SIOCNT]);
                if (value & SIO_IRQ_ENABLE) {
                    IF |= 0x80;
                    UPDATE_REG(0x202, IF);
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
#if (defined __WIN32__ || defined _WIN32)
        gbInitLinkIPC();
#endif
    } else {
        LinkIsWaiting = false;
        LinkFirstTime = true;
    }
}

uint8_t gbStartLink(uint8_t b) //used on internal clock
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
#if (defined __WIN32__ || defined _WIN32)
        dat = gbStartLinkIPC(b);
#endif
    } else {
        if (lanlink.numslaves == 1) {
            if (lanlink.server) {
                cable_gb_data[0] = b;
                ls.SendGB();

                if (ls.RecvGB())
                    dat = cable_gb_data[1];
            } else {
                cable_gb_data[1] = b;
                lc.SendGB();

                if (lc.RecvGB())
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
    uint8_t dat = b; //0xff; //slave (w/ external clocks) won't be getting 0xff if master turned off
    uint8_t recvd = 0;

    gba_link_enabled = true; //(gbMemory[0xff02]!=0);
    rfu_enabled = false;

    if (gbSerialOn) {
        if (gba_link_enabled) {
            //Single Computer
            if (GetLinkMode() == LINK_GAMEBOY_IPC) {
#if (defined __WIN32__ || defined _WIN32)
                return gbLinkUpdateIPC(b, gbSerialOn);
#endif
            } else {
                if (lanlink.numslaves == 1) {
                    if (lanlink.server) {
                        recvd = ls.RecvGB() ? 1 : 0;
                        if (recvd) {
                            dat = cable_gb_data[1];
                            LinkIsWaiting = false;
                        } else
                            LinkIsWaiting = true;

                        if (!LinkIsWaiting) {
                            cable_gb_data[0] = b;
                            ls.SendGB();
                        }
                    } else {
                        recvd = lc.RecvGB() ? 1 : 0;
                        if (recvd) {
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

#if (defined __WIN32__ || defined _WIN32)

static ConnectionState InitIPC()
{
    linkid = 0;

#if (defined __WIN32__ || defined _WIN32)
    if ((mmf = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LINKDATA), LOCAL_LINK_NAME)) == NULL) {
        systemMessage(0, N_("Error creating file mapping"));
        return LINK_ERROR;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        vbaid = 1;
    else
        vbaid = 0;

    if ((linkmem = (LINKDATA*)MapViewOfFile(mmf, FILE_MAP_WRITE, 0, 0, sizeof(LINKDATA))) == NULL) {
        CloseHandle(mmf);
        systemMessage(0, N_("Error mapping file"));
        return LINK_ERROR;
    }
#else
    if ((mmf = shm_open("/" LOCAL_LINK_NAME, O_RDWR | O_CREAT | O_EXCL, 0777)) < 0) {
        vbaid = 1;
        mmf = shm_open("/" LOCAL_LINK_NAME, O_RDWR, 0);
    } else
        vbaid = 0;
    if (mmf < 0 || ftruncate(mmf, sizeof(LINKDATA)) < 0 || !(linkmem = (LINKDATA*)mmap(NULL, sizeof(LINKDATA), PROT_READ | PROT_WRITE, MAP_SHARED, mmf, 0))) {
        systemMessage(0, N_("Error creating file mapping"));
        if (mmf) {
            if (!vbaid)
                shm_unlink("/" LOCAL_LINK_NAME);
            close(mmf);
        }
    }
#endif

    // get lowest-numbered available machine slot
    bool firstone = !vbaid;
    if (firstone) {
        linkmem->linkflags = 1;
        linkmem->numgbas = 1;
        linkmem->numtransfers = 0;
        for (i = 0; i < 4; i++)
            linkmem->linkdata[i] = 0xffff;
    } else {
        // FIXME: this should be done while linkmem is locked
        // (no xfer in progress, no other vba trying to connect)
        int n = linkmem->numgbas;
        int f = linkmem->linkflags;
        for (int i = 0; i <= n; i++)
            if (!(f & (1 << i))) {
                vbaid = i;
                break;
            }
        if (vbaid == 4) {
#if (defined __WIN32__ || defined _WIN32)
            UnmapViewOfFile(linkmem);
            CloseHandle(mmf);
#else
            munmap(linkmem, sizeof(LINKDATA));
            if (!vbaid)
                shm_unlink("/" LOCAL_LINK_NAME);
            close(mmf);
#endif
            systemMessage(0, N_("5 or more GBAs not supported."));
            return LINK_ERROR;
        }
        if (vbaid == n)
            linkmem->numgbas = n + 1;
        linkmem->linkflags = f | (1 << vbaid);
    }
    linkid = vbaid;

    for (i = 0; i < 4; i++) {
        linkevent[sizeof(linkevent) - 2] = (char)i + '1';
#if (defined __WIN32__ || defined _WIN32)
        linksync[i] = firstone ? CreateSemaphore(NULL, 0, 4, linkevent) : OpenSemaphore(SEMAPHORE_ALL_ACCESS, false, linkevent);
        if (linksync[i] == NULL) {
            UnmapViewOfFile(linkmem);
            CloseHandle(mmf);
            for (j = 0; j < i; j++) {
                CloseHandle(linksync[j]);
            }
            systemMessage(0, N_("Error opening event"));
            return LINK_ERROR;
        }
#else
        if ((linksync[i] = sem_open(linkevent,
                 firstone ? O_CREAT | O_EXCL : 0,
                 0777, 0))
            == SEM_FAILED) {
            if (firstone)
                shm_unlink("/" LOCAL_LINK_NAME);
            munmap(linkmem, sizeof(LINKDATA));
            close(mmf);
            for (j = 0; j < i; j++) {
                sem_close(linksync[i]);
                if (firstone) {
                    linkevent[sizeof(linkevent) - 2] = (char)i + '1';
                    sem_unlink(linkevent);
                }
            }
            systemMessage(0, N_("Error opening event"));
            return LINK_ERROR;
        }
#endif
    }

    return LINK_OK;
}

static void StartCableIPC(uint16_t value)
{
    switch (GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]))) {
    case MULTIPLAYER: {
        bool start = (value & 0x80) && !linkid && !transfer_direction;
        // clear start, seqno, si (RO on slave, start = pulse on master)
        value &= 0xff4b;
        // get current si.  This way, on slaves, it is low during xfer
        if (linkid) {
            if (!transfer_direction)
                value |= 4;
            else
                value |= READ16LE(&ioMem[COMM_SIOCNT]) & 4;
        }
        if (start) {
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
                linkmem->trgbas = n;

                // before starting xfer, make pathetic attempt
                // at clearing out any previous stuck xfer
                // this will fail if a slave was stuck for
                // too long
                for (int i = 0; i < 4; i++)
                    while (WaitForSingleObject(linksync[i], 0) != WAIT_TIMEOUT)
                        ;

                // transmit first value
                linkmem->linkcmd[0] = ('M' << 8) + (value & 3);
                linkmem->linkdata[0] = READ16LE(&ioMem[COMM_SIODATA8]);

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

                transfer_direction = 1;
                linktime = 0;
                tspeed = value & 3;
                WRITE32LE(&ioMem[COMM_SIOMULTI0], 0xffffffff);
                WRITE32LE(&ioMem[COMM_SIOMULTI2], 0xffffffff);
                value &= ~0x40;
            } else {
                value |= 0x40; // comm error
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

static void ReconnectCableIPC()
{
    int f = linkmem->linkflags;
    int n = linkmem->numgbas;
    if (f & (1 << linkid)) {
        systemMessage(0, N_("Lost link; reinitialize to reconnect"));
        return;
    }
    linkmem->linkflags |= 1 << linkid;
    if (n < linkid + 1)
        linkmem->numgbas = linkid + 1;
    numtransfers = linkmem->numtransfers;
    systemScreenMessage(_("Lost link; reconnected"));
}

static void UpdateCableIPC(int ticks)
{
    if (((READ16LE(&ioMem[COMM_RCNT])) >> 14) == 3)
        return;

    // slave startup depends on detecting change in numtransfers
    // and syncing clock with master (after first transfer)
    // this will fail if > ~2 minutes have passed since last transfer due
    // to integer overflow
    if (!transfer_direction && numtransfers && linktime < 0) {
        linktime = 0;
        // there is a very, very, small chance that this will abort
        // a transfer that was just started
        linkmem->numtransfers = numtransfers = 0;
    }
    if (linkid && !transfer_direction && linktime >= linkmem->lastlinktime && linkmem->numtransfers != numtransfers) {
        numtransfers = linkmem->numtransfers;
        if (!numtransfers)
            return;

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

// there's really no point to this switch; 'M' is the only
// possible command.
#if 0
		switch ((linkmem->linkcmd) >> 8)
		{
		case 'M':
#endif
        tspeed = linkmem->linkcmd[0] & 3;
        transfer_direction = 1;
        WRITE32LE(&ioMem[COMM_SIOMULTI0], 0xffffffff);
        WRITE32LE(&ioMem[COMM_SIOMULTI2], 0xffffffff);
        UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & ~0x40 | 0x80);
#if 0
			break;
		}
#endif
    }

    if (!transfer_direction)
        return;

    if (transfer_direction <= linkmem->trgbas && linktime >= trtimedata[transfer_direction - 1][tspeed]) {
        // transfer #n -> wait for value n - 1
        if (transfer_direction > 1 && linkid != transfer_direction - 1) {
            if (WaitForSingleObject(linksync[transfer_direction - 1], linktimeout) == WAIT_TIMEOUT) {
                // assume slave has dropped off if timed out
                if (!linkid) {
                    linkmem->trgbas = transfer_direction - 1;
                    int f = linkmem->linkflags;
                    f &= ~(1 << (transfer_direction - 1));
                    linkmem->linkflags = f;
                    if (f < (1 << transfer_direction) - 1)
                        linkmem->numgbas = transfer_direction - 1;
                    char message[30];
                    sprintf(message, _("Player %d disconnected."), transfer_direction - 1);
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
            UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & ~4);
            UPDATE_REG(COMM_RCNT, 10);
            linkmem->linkdata[linkid] = READ16LE(&ioMem[COMM_SIODATA8]);
            ReleaseSemaphore(linksync[linkid], linkmem->numgbas - 1, NULL);
        }
        if (linkid == transfer_direction - 1) {
            // SO becomes low to begin next trasnfer
            // may need to set DDR as well
            UPDATE_REG(COMM_RCNT, 0x22);
        }

        // next cycle
        transfer_direction = !transfer_direction;
    }

    if (transfer_direction > linkmem->trgbas && linktime >= trtimeend[transfer_direction - 3][tspeed]) {
        // wait for slaves to finish
        // this keeps unfinished slaves from screwing up last xfer
        // not strictly necessary; may just slow things down
        if (!linkid) {
            for (int i = 2; i < transfer_direction; i++)
                if (WaitForSingleObject(linksync[0], linktimeout) == WAIT_TIMEOUT) {
                    // impossible to determine which slave died
                    // so leave them alone for now
                    systemScreenMessage(_("Unknown slave timed out; resetting comm"));
                    linkmem->numtransfers = numtransfers = 0;
                    break;
                }
        } else if (linkmem->trgbas > linkid)
            // signal master that this slave is finished
            ReleaseSemaphore(linksync[0], 1, NULL);
        linktime -= trtimeend[transfer_direction - 3][tspeed];
        transfer_direction = 0;
        uint16_t value = READ16LE(&ioMem[COMM_SIOCNT]);
        if (!linkid)
            value |= 4; // SI becomes high on slaves after xfer
        UPDATE_REG(COMM_SIOCNT, (value & 0xff0f) | (linkid << 4));
        // SC/SI high after transfer
        UPDATE_REG(COMM_RCNT, linkid ? 15 : 11);
        if (value & 0x4000) {
            IF |= 0x80;
            UPDATE_REG(0x202, IF);
        }
    }
}

// The GBA wireless RFU (see adapter3.txt)
static void StartRFU(uint16_t value)
{
    int siomode = GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]));

    if (value)
        rfu_enabled = (siomode == NORMAL32);

    if (((READ16LE(&ioMem[COMM_SIOCNT]) & 0x5080) == 0x1000) && ((value & 0x5080) == 0x5080)) { //RFU Reset, may also occur before cable link started
        log("RFU Reset2 : %04X  %04X  %d\n", READ16LE(&ioMem[COMM_RCNT]), READ16LE(&ioMem[COMM_SIOCNT]), GetTickCount());
        linkmem->rfu_listfront[vbaid] = 0;
        linkmem->rfu_listback[vbaid] = 0;
    }

    if (!rfu_enabled) {
        if ((value & 0x5080) == 0x5080) { //0x5083 //game tried to send wireless command but w/o the adapter
            /*if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
			value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
			else //(Bit.3, 0=Enable Transfer/Ready)
			value |= 4; //bit.2=1 (otherside is Not Ready)*/
            if (READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000) //IRQ Enable
            {
                IF |= 0x80; //Serial Communication
                UPDATE_REG(0x202, IF); //Interrupt Request Flags / IRQ Acknowledge
            }
            value &= 0xff7f; //Start bit.7 reset //may cause the game to retry sending again
            //value |= 0x0008; //SO bit.3 set automatically upon transfer completion
            transfer_direction = 0;
        }
        return;
    }

    linktimeout = 1;

    uint32_t CurCOM = 0, CurDAT = 0;
    bool rfulogd = (READ16LE(&ioMem[COMM_SIOCNT]) != value);

    switch (GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]))) {
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
            uint16_t siodata_h = READ16LE(&ioMem[COMM_SIODATA32_H]);
            switch (rfu_state) {
            case RFU_INIT:
                if (READ32LE(&ioMem[COMM_SIODATA32_L]) == 0xb0bb8001) {
                    rfu_state = RFU_COMM; // end of startup
                    rfu_initialized = true;
                    value &= 0xfffb; //0xff7b; //Bit.2 need to be 0 to indicate a finished initialization to fix MarioGolfAdv from occasionally Not Detecting wireless adapter (prevent it from sending 0x7FFE8001 comm)?
                    rfu_polarity = 0; //not needed?
                }
                rfu_buf = (READ16LE(&ioMem[COMM_SIODATA32_L]) << 16) | siodata_h;
                break;
            case RFU_COMM:
                CurCOM = READ32LE(&ioMem[COMM_SIODATA32_L]);
                if (siodata_h == 0x9966) //initialize cmd
                {
                    uint8_t tmpcmd = CurCOM;
                    if (tmpcmd != 0x10 && tmpcmd != 0x11 && tmpcmd != 0x13 && tmpcmd != 0x14 && tmpcmd != 0x16 && tmpcmd != 0x17 && tmpcmd != 0x19 && tmpcmd != 0x1a && tmpcmd != 0x1b && tmpcmd != 0x1c && tmpcmd != 0x1d && tmpcmd != 0x1e && tmpcmd != 0x1f && tmpcmd != 0x20 && tmpcmd != 0x21 && tmpcmd != 0x24 && tmpcmd != 0x25 && tmpcmd != 0x26 && tmpcmd != 0x27 && tmpcmd != 0x30 && tmpcmd != 0x32 && tmpcmd != 0x33 && tmpcmd != 0x34 && tmpcmd != 0x3d && tmpcmd != 0xa8 && tmpcmd != 0xee) {
                        log("%08X : UnkCMD %08X  %04X  %08X %08X\n", GetTickCount(), CurCOM, PrevVAL, PrevCOM, PrevDAT);
                    }
                    rfu_counter = 0;
                    if ((rfu_qsend2 = rfu_qsend = ioMem[0x121]) != 0) { //COMM_SIODATA32_L+1, following data [to send]
                        rfu_state = RFU_SEND;
                    }
                    if (ioMem[COMM_SIODATA32_L] == 0xee) { //0xee cmd shouldn't override previous cmd
                        rfu_lastcmd = rfu_cmd2;
                        rfu_cmd2 = ioMem[COMM_SIODATA32_L];
                        //rfu_polarity = 0; //when polarity back to normal the game can initiate a new cmd even when 0xee hasn't been finalized, but it looks improper isn't?
                    } else {
                        rfu_lastcmd = rfu_cmd;
                        rfu_cmd = ioMem[COMM_SIODATA32_L];
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
                                while (linkmem->numgbas >= 2 && linkmem->rfu_q[vbaid] > 1 && vbaid != gbaid && linkmem->rfu_signal[vbaid] && linkmem->rfu_signal[gbaid] && (GetTickCount() - rfu_lasttime) < (DWORD)linktimeout) {
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
                                while (linkmem->numgbas >= 2 && linkmem->rfu_q[vbaid] > 1 && vbaid != gbaid && linkmem->rfu_signal[vbaid] && linkmem->rfu_signal[gbaid] && (GetTickCount() - rfu_lasttime) < (DWORD)linktimeout) {
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
                            bool ok = false;
                        } else if (rfu_cmd == 0x11 || rfu_cmd == 0x1a || rfu_cmd == 0x26) {
                            if (rfu_lastcmd2 == 0x24)
                                rfu_waiting = true;
                        }
                    }
                    if (rfu_waiting)
                        rfu_buf = READ32LE(&ioMem[COMM_SIODATA32_L]);
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
                                        rfu_clientlist[rfu_numclients] = rfu_masterdata[0] | (rfu_numclients++ << 16);
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
                            rfu_id = (gbaid << 3) + 0x61f1;
                            rfu_cmd ^= 0x80;
                            break;
                        case 0x1f: // join a room as client
                            // TODO: to fix infinte send&recv w/o giving much cance to update the screen when both side acting as client
                            // on MarioGolfAdv lobby(might be due to leftover data when switching from host to join mode at the same time?)
                            rfu_id = rfu_masterdata[0];
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
                                                rfu_id = (gbaid << 3) + 0x61f1;
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

                            if (rfu_cansend && rfu_qsend2 >= 0) {
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
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].gbaid = vbaid;
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].len = rfu_qsend2;
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].time = linktime;
                                            linkmem->rfu_listback[j]++;
                                            SetEvent(linksync[j]); //unlock it so anyone can access it
                                        }
                                } else if (vbaid != gbaid) {
                                    WaitForSingleObject(linksync[gbaid], linktimeout); //wait until unlocked
                                    ResetEvent(linksync[gbaid]); //lock it so noone can access it
                                    memcpy(linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].data, rfu_masterdata, 4 * rfu_qsend2);
                                    linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].gbaid = vbaid;
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
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].gbaid = vbaid;
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].len = rfu_qsend2;
                                            linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].time = linktime;
                                            linkmem->rfu_listback[j]++;
                                            SetEvent(linksync[j]); //unlock it so anyone can access it
                                        }
                                } else if (vbaid != gbaid) {
                                    WaitForSingleObject(linksync[gbaid], linktimeout); //wait until unlocked
                                    ResetEvent(linksync[gbaid]); //lock it so noone can access it
                                    memcpy(linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].data, rfu_masterdata, 4 * rfu_qsend2);
                                    linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].gbaid = vbaid;
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
                                        linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].gbaid = vbaid;
                                        linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].len = 0; //rfu_qsend2;
                                        linkmem->rfu_datalist[j][linkmem->rfu_listback[j]].time = linktime;
                                        linkmem->rfu_listback[j]++;
                                        SetEvent(linksync[j]); //unlock it so anyone can access it
                                    }
                            } else if (vbaid != gbaid) {
                                WaitForSingleObject(linksync[gbaid], linktimeout); //wait until unlocked
                                ResetEvent(linksync[gbaid]); //lock it so noone can access it
                                //memcpy(linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].data,rfu_masterdata,4*rfu_qsend2);
                                linkmem->rfu_datalist[gbaid][linkmem->rfu_listback[gbaid]].gbaid = vbaid;
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
                            rfu_buf = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    }
                } else { //unknown COMM word //in MarioGolfAdv (when a player/client exiting lobby), There is a possibility COMM = 0x7FFE8001, PrevVAL = 0x5087, PrevCOM = 0, is this part of initialization?
                    log("%08X : UnkCOM %08X  %04X  %08X %08X\n", GetTickCount(), READ32LE(&ioMem[COMM_SIODATA32_L]), PrevVAL, PrevCOM, PrevDAT);
                    /*rfu_cmd ^= 0x80;
				 UPDATE_REG(COMM_SIODATA32_L, 0);
				 UPDATE_REG(COMM_SIODATA32_H, 0x8000);*/
                    rfu_state = RFU_INIT; //to prevent the next reinit words from getting in finalization processing (here), may cause MarioGolfAdv to show Linking error when this occurs instead of continuing with COMM cmd
                    //UPDATE_REG(COMM_SIODATA32_H, READ16LE(&ioMem[COMM_SIODATA32_L])); //replying with reversed words may cause MarioGolfAdv to reinit RFU when COMM = 0x7FFE8001
                    //UPDATE_REG(COMM_SIODATA32_L, a);
                    rfu_buf = (READ16LE(&ioMem[COMM_SIODATA32_L]) << 16) | siodata_h;
                }
                break;

            case RFU_SEND: //data following after initialize cmd
                //if(rfu_qsend==0) {rfu_state = RFU_COMM; break;}
                CurDAT = READ32LE(&ioMem[COMM_SIODATA32_L]);
                if (--rfu_qsend == 0) {
                    rfu_state = RFU_COMM;
                }

                switch (rfu_cmd) {
                case 0x16:
                    linkmem->rfu_broadcastdata[vbaid][1 + rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x17:
                    //linkid = 1;
                    rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x1f:
                    rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    break;

                case 0x24:
                //if(linkmem->rfu_proto[vbaid]) break; //important data from 0x25 shouldn't be overwritten by 0x24
                case 0x25:
                case 0x35:
                    //if(rfu_cansend)
                    //linkmem->rfu_data[vbaid][rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
                    break;

                default:
                    rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
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
    default:
        UPDATE_REG(COMM_SIOCNT, value);
        return;
    }
    UPDATE_REG(COMM_SIOCNT, value);
}

bool LinkRFUUpdate()
{
    //if (IsLinkConnected()) {
    //}
    if (rfu_enabled) {
        if (transfer_direction && rfu_transfer_end <= 0) {
            if (rfu_waiting) {
                bool ok = false;
                uint8_t oldcmd = rfu_cmd;
                uint8_t oldq = linkmem->rfu_q[vbaid];
                uint32_t tmout = linktimeout;
                //if ((!lanlink.active&&speedhack) || (lanlink.speed&&IsLinkConnected()))tmout = 16;
                if (rfu_state != RFU_INIT) {
                    if (rfu_cmd == 0x24 || rfu_cmd == 0x25 || rfu_cmd == 0x35) {
                        //c_s.Lock();
                        ok = linkmem->rfu_signal[vbaid] && linkmem->rfu_q[vbaid] > 1 && rfu_qsend > 1;
                        //c_s.Unlock();
                        if (ok && (GetTickCount() - rfu_lasttime) < (DWORD)linktimeout) {
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

                        if (((rfu_cmd == 0x11 || rfu_cmd == 0x1a || rfu_cmd == 0x26) && (GetTickCount() - rfu_lasttime) < 16) || ((rfu_cmd == 0xa5 || rfu_cmd == 0xb5) && (GetTickCount() - rfu_lasttime) < 16) || ((rfu_cmd == 0xa7 || rfu_cmd == 0xb7) && (GetTickCount() - rfu_lasttime) < (DWORD)linktimeout)) {
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

                        if (READ32LE(&ioMem[COMM_SIODATA32_L]) == 0x80000000)
                            rfu_buf = 0x99660000 | (rfu_qrecv_broadcast_data_len << 8) | rfu_cmd;
                        else
                            rfu_buf = 0x80000000;
                    }
                    rfu_waiting = false;
                }
            }
            UPDATE_REG(COMM_SIODATA32_L, rfu_buf);
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
                uint16_t value = READ16LE(&ioMem[COMM_SIOCNT]);
                if (value & 0x4000) {
                    IF |= 0x80;
                    UPDATE_REG(0x202, IF);
                }

                //if (rfu_polarity) value ^= 4;
                value &= 0xfffb;
                value |= (value & 1) << 2; //this will automatically set the correct polarity, even w/o rfu_polarity since the game will be the one who change the polarity instead of the adapter

                //UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & 0xff7f);
                UPDATE_REG(COMM_SIOCNT, (value & 0xff7f) | 0x0008); //Start bit.7 reset, SO bit.3 set automatically upon transfer completion?
                //log("SIOn32 : %04X %04X  %08X  (VCOUNT = %d) %d %d\n", READ16LE(&ioMem[COMM_RCNT]), READ16LE(&ioMem[COMM_SIOCNT]), READ32LE(&ioMem[COMM_SIODATA32_L]), VCOUNT);
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
    BOOL sent = false;
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
    BOOL recvd = false;
    int idx = 0;

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
                    tm = GetTickCount();
                    do {
                        WaitForSingleObject(linksync[linkid], 1);
                        ResetEvent(linksync[linkid]);
                    } while (linkmem->linkcmd[1 - linkid] && (GetTickCount() - tm) < (uint32_t)linktimeout);
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
    return ((dat << 8) | (recvd & (uint8_t)0xff));
}

static void CloseIPC()
{
    int f = linkmem->linkflags;
    f &= ~(1 << linkid);
    if (f & 0xf) {
        linkmem->linkflags = f;
        int n = linkmem->numgbas;
        for (int i = 0; i < n; i--)
            if (f <= (1 << (i + 1)) - 1) {
                linkmem->numgbas = i + 1;
                break;
            }
    }

    for (i = 0; i < 4; i++) {
        if (linksync[i] != NULL) {
#if (defined __WIN32__ || defined _WIN32)
            ReleaseSemaphore(linksync[i], 1, NULL);
            CloseHandle(linksync[i]);
#else
            sem_close(linksync[i]);
            if (!(f & 0xf)) {
                linkevent[sizeof(linkevent) - 2] = (char)i + '1';
                sem_unlink(linkevent);
            }
#endif
        }
    }
#if (defined __WIN32__ || defined _WIN32)
    CloseHandle(mmf);
    UnmapViewOfFile(linkmem);

// FIXME: move to caller
// (but there are no callers, so why bother?)
//regSetDwordValue("LAN", lanlink.active);
#else
    if (!(f & 0xf))
        shm_unlink("/" LOCAL_LINK_NAME);
    munmap(linkmem, sizeof(LINKDATA));
    close(mmf);
#endif
}

#endif

#else
bool gba_joybus_active = false;
#endif
