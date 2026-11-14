////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2025 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// BSD sockets on MacTCP (classic Mac OS, Retro68). See
// include/mactcp-sockets.h for the API and its limits.
//
// The model, per descriptor:
//  - TCP: one MacTCP stream (TCPCreate) per connection. connect() is an
//    asynchronous TCPActiveOpen; send() queues asynchronous TCPSends from
//    a few socket-owned buffers (MacTCP reads the data until the command
//    completes, so it cannot point into the caller's); recv() asks
//    TCPStatus how much is waiting and takes it with a synchronous TCPRcv,
//    which then never blocks.
//  - A listener holds up to SOMAXCONN streams, each in an asynchronous
//    TCPPassiveOpen on the port (MacTCP lets several streams listen on one
//    port, which is how its servers take more than one connection). A
//    passive open completes when a peer has connected; accept() hands that
//    stream to a new descriptor and listens again on a fresh one.
//  - UDP: one MacTCP UDP stream, with at most one asynchronous UDPRead
//    outstanding; the datagram it returns is copied out by recvfrom() and
//    its buffer handed back with UDPBfrReturn.
// Blocking calls poll their parameter blocks and nap in between.
////////////////////////////////////////////////////////////

#include "mactcp-sockets.h"
#include "Resolver.hpp"

#include "SFML/System/MacOS/Microseconds.hpp"

#include <Multiverse.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include <unistd.h>

namespace
{
////////////////////////////////////////////////////////////
// UDP parameter blocks. Multiversal Interfaces carry only the TCP half of
// MacTCP.h; these follow Universal Interfaces 3.4.2 (68k alignment).
////////////////////////////////////////////////////////////
#pragma pack(push, 2)
struct UdpCreatePB
{
    Ptr       rcvBuff;
    ULONGINT  rcvBuffLen;
    ProcPtr   notifyProc;
    uint16_t  localPort;
    Ptr       userDataPtr;
    uint16_t  endingPort;
};

struct UdpSendPB
{
    uint16_t reserved;
    ip_addr  remoteHost;
    uint16_t remotePort;
    Ptr      wdsPtr;
    Boolean  checkSum;
    int8_t   filler;
    uint16_t sendLength;
    Ptr      userDataPtr;
    uint16_t localPort;
};

struct UdpReceivePB
{
    uint16_t timeOut;
    ip_addr  remoteHost;
    uint16_t remotePort;
    Ptr      rcvBuff;
    uint16_t rcvBuffLen;
    uint16_t secondTimeStamp;
    Ptr      userDataPtr;
    ip_addr  destHost;
    uint16_t destPort;
};

struct UdpIopb
{
    int8_t    fill12[12];
    ProcPtr   ioCompletion;
    OSErr     ioResult;
    StringPtr ioNamePtr;
    INTEGER   ioVRefNum;
    INTEGER   ioCRefNum;
    INTEGER   csCode;
    StreamPtr udpStream;
    union
    {
        UdpCreatePB  create;
        UdpSendPB    send;
        UdpReceivePB receive;
    } csParam;
};
#pragma pack(pop)

static_assert(sizeof(TCPiopb) == 102, "TCPiopb must have MacTCP's layout");
static_assert(sizeof(GetAddrParamBlock) == 36, "GetAddrParamBlock must have MacTCP's layout");

////////////////////////////////////////////////////////////
// Tunables
////////////////////////////////////////////////////////////
constexpr int           kMaxSockets    = 16;
constexpr unsigned long kTcpBufferSize = 16384; // MacTCP wants 4 KB at the least
constexpr unsigned long kUdpBufferSize = 8192;  // 2 KB at the least
constexpr int           kSendSlots     = 4;
constexpr uint16_t      kSendChunk     = 8192;
constexpr int           kNapMicros     = 1000;

static_assert(MACTCP_FIRST_FD + kMaxSockets <= FD_SETSIZE, "socket descriptors must fit an fd_set");

////////////////////////////////////////////////////////////
// Driver state
////////////////////////////////////////////////////////////
short    g_ipp       = 0;     // .IPP driver reference number, 0 while closed
uint32_t g_localAddr = 0;     // our IPv4 address
bool     g_atexit    = false; // mactcp_shutdown registered

////////////////////////////////////////////////////////////
struct Stream
{
    StreamPtr ptr{};
    Ptr       buffer{};
};

struct SendSlot
{
    TCPiopb  pb{};
    wdsEntry wds[2]{};
    Ptr      data{};
    bool     busy{};
};

struct ListenSlot
{
    TCPiopb pb{};
    Stream  stream;
    bool    armed{}; // a passive open was queued (it may have completed)
};

enum class Kind : uint8_t
{
    Free,
    Tcp,
    Udp
};

enum class TcpState : uint8_t
{
    Idle,       // created, not connected
    Listening,  // listen() succeeded
    Connecting, // TCPActiveOpen queued
    Connected,  // established (or closing; recv() reports the end)
    Failed      // the connection attempt or the connection failed
};

struct Sock
{
    Kind     kind{Kind::Free};
    bool     nonBlocking{};
    int      pendingError{}; // SO_ERROR
    uint32_t localAddr{};
    uint16_t localPort{};
    uint32_t remoteAddr{};
    uint16_t remotePort{};

    // TCP
    TcpState    state{TcpState::Idle};
    Stream      stream;
    TCPiopb     openPb{};
    TCPiopb     closePb{};
    bool        closeQueued{};
    int         sendError{};
    SendSlot    sends[kSendSlots];
    ListenSlot* listenSlots{};
    int         listenCount{};

    // UDP
    Stream   udpStream;
    UdpIopb  readPb{};
    bool     readQueued{};
    bool     udpConnected{};
};

Sock g_socks[kMaxSockets];

////////////////////////////////////////////////////////////
inline OSErr pbResult(const OSErr& ioResult)
{
    // Written by the driver at interrupt time.
    return *static_cast<const volatile OSErr*>(&ioResult);
}

inline bool pbBusy(const OSErr& ioResult)
{
    return pbResult(ioResult) > 0;
}

void nap()
{
    usleep(kNapMicros);
}

std::uint64_t nowMicros()
{
    return sf::priv::macMicroseconds();
}

////////////////////////////////////////////////////////////
int mapError(OSErr err, bool connecting = false)
{
    switch (err)
    {
        case noErr:
            return 0;
        case openFailed:
            return ECONNREFUSED;
        case connectionTerminated:
            return connecting ? ECONNREFUSED : ECONNRESET;
        case commandTimeout:
            return ETIMEDOUT;
        case connectionExists:
        case duplicateSocket:
            return EADDRINUSE;
        case connectionClosing:
            return EPIPE;
        case connectionDoesntExist:
            return ENOTCONN;
        case insufficientResources:
        case memFullErr:
            return ENOBUFS;
        case ipBadAddr:
            return EADDRNOTAVAIL;
        case ipBadLapErr:
        case ipBadCnfgErr:
        case ipNoCnfgErr:
        case ipLoadErr:
            return ENETDOWN;
        case invalidLength:
            return EMSGSIZE;
        case invalidStreamPtr:
            return EBADF;
        default:
            return EIO;
    }
}

int fail(int error)
{
    errno = error;
    return -1;
}

////////////////////////////////////////////////////////////
// Opens .IPP once, on first use.
int openDriver()
{
    if (g_ipp != 0)
        return 0;

    static const unsigned char name[] = {4, '.', 'I', 'P', 'P'};
    short                      refNum = 0;
    if (OpenDriver(name, &refNum) != noErr)
        return ENETDOWN;

    g_ipp = refNum;

    GetAddrParamBlock pb{};
    pb.ioCRefNum = g_ipp;
    pb.csCode    = ipctlGetAddr;
    if (PBControlSync(reinterpret_cast<ParmBlkPtr>(&pb)) == noErr)
        g_localAddr = pb.ourAddress;

    if (!g_atexit)
    {
        g_atexit = true;
        std::atexit(mactcp_shutdown);
    }
    return 0;
}

////////////////////////////////////////////////////////////
OSErr tcpCall(TCPiopb& pb, INTEGER csCode, StreamPtr stream, bool async)
{
    pb.ioCompletion = nullptr;
    pb.ioCRefNum    = g_ipp;
    pb.csCode       = csCode;
    pb.tcpStream    = stream;
    return async ? PBControlAsync(reinterpret_cast<ParmBlkPtr>(&pb))
                 : PBControlSync(reinterpret_cast<ParmBlkPtr>(&pb));
}

OSErr udpCall(UdpIopb& pb, INTEGER csCode, StreamPtr stream, bool async)
{
    pb.ioCompletion = nullptr;
    pb.ioCRefNum    = g_ipp;
    pb.csCode       = csCode;
    pb.udpStream    = stream;
    return async ? PBControlAsync(reinterpret_cast<ParmBlkPtr>(&pb))
                 : PBControlSync(reinterpret_cast<ParmBlkPtr>(&pb));
}

////////////////////////////////////////////////////////////
OSErr createTcpStream(Stream& stream)
{
    stream.buffer = NewPtr(kTcpBufferSize);
    if (!stream.buffer)
        return memFullErr;

    TCPiopb pb{};
    pb.csParam.create.rcvBuff     = stream.buffer;
    pb.csParam.create.rcvBuffLen  = kTcpBufferSize;
    pb.csParam.create.notifyProc  = nullptr;
    pb.csParam.create.userDataPtr = nullptr;
    const OSErr err               = tcpCall(pb, TCPCreate, nullptr, false);
    if (err != noErr)
    {
        DisposePtr(stream.buffer);
        stream.buffer = nullptr;
        return err;
    }
    stream.ptr = pb.tcpStream;
    return noErr;
}

// Waits (boundedly) for an asynchronous command to come back.
void settle(const OSErr& ioResult, std::uint64_t micros)
{
    const std::uint64_t start = nowMicros();
    while (pbBusy(ioResult) && nowMicros() - start < micros)
        nap();
}

// Aborts the connection and frees the stream. Every command still queued
// on it completes (with an error) by the time TCPRelease returns.
void releaseTcpStream(Stream& stream)
{
    if (stream.ptr)
    {
        TCPiopb pb{};
        tcpCall(pb, TCPAbort, stream.ptr, false);
        pb = TCPiopb{};
        tcpCall(pb, TCPRelease, stream.ptr, false);
        stream.ptr = nullptr;
    }
    if (stream.buffer)
    {
        DisposePtr(stream.buffer);
        stream.buffer = nullptr;
    }
}

////////////////////////////////////////////////////////////
OSErr createUdpStream(Sock& s)
{
    s.udpStream.buffer = NewPtr(kUdpBufferSize);
    if (!s.udpStream.buffer)
        return memFullErr;

    UdpIopb pb{};
    pb.csParam.create.rcvBuff     = s.udpStream.buffer;
    pb.csParam.create.rcvBuffLen  = kUdpBufferSize;
    pb.csParam.create.notifyProc  = nullptr;
    pb.csParam.create.localPort   = s.localPort;
    pb.csParam.create.userDataPtr = nullptr;
    const OSErr err               = udpCall(pb, udpCreate, nullptr, false);
    if (err != noErr)
    {
        DisposePtr(s.udpStream.buffer);
        s.udpStream.buffer = nullptr;
        return err;
    }
    s.udpStream.ptr = pb.udpStream;
    s.localPort     = pb.csParam.create.localPort;
    return noErr;
}

void releaseUdpStream(Sock& s)
{
    if (s.udpStream.ptr)
    {
        UdpIopb pb{};
        udpCall(pb, udpRelease, s.udpStream.ptr, false);
        s.udpStream.ptr = nullptr;
        settle(s.readPb.ioResult, 1'000'000);
    }
    s.readQueued = false;
    if (s.udpStream.buffer)
    {
        DisposePtr(s.udpStream.buffer);
        s.udpStream.buffer = nullptr;
    }
}

////////////////////////////////////////////////////////////
Sock* lookup(int fd)
{
    const int index = fd - MACTCP_FIRST_FD;
    if (index < 0 || index >= kMaxSockets || g_socks[index].kind == Kind::Free)
        return nullptr;
    return &g_socks[index];
}

int descriptorOf(const Sock& s)
{
    return static_cast<int>(&s - g_socks) + MACTCP_FIRST_FD;
}

Sock* allocate(Kind kind)
{
    for (Sock& s : g_socks)
    {
        if (s.kind == Kind::Free)
        {
            s      = Sock{};
            s.kind = kind;
            return &s;
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////
// Folds a finished TCPActiveOpen into the socket state.
void settleConnect(Sock& s)
{
    if (s.state != TcpState::Connecting || pbBusy(s.openPb.ioResult))
        return;

    const OSErr err = pbResult(s.openPb.ioResult);
    if (err == noErr)
    {
        s.state     = TcpState::Connected;
        s.localPort = s.openPb.csParam.open.localPort;
        if (s.openPb.csParam.open.localHost)
            s.localAddr = s.openPb.csParam.open.localHost;
    }
    else
    {
        s.state        = TcpState::Failed;
        s.pendingError = mapError(err, true);
    }
}

// Collects finished sends; the first failure sticks.
void reapSends(Sock& s)
{
    for (SendSlot& slot : s.sends)
    {
        if (slot.busy && !pbBusy(slot.pb.ioResult))
        {
            slot.busy       = false;
            const OSErr err = pbResult(slot.pb.ioResult);
            if (err != noErr && s.sendError == 0)
                s.sendError = mapError(err);
        }
    }
}

SendSlot* freeSendSlot(Sock& s)
{
    reapSends(s);
    for (SendSlot& slot : s.sends)
        if (!slot.busy)
            return &slot;
    return nullptr;
}

bool sendsPending(Sock& s)
{
    reapSends(s);
    for (const SendSlot& slot : s.sends)
        if (slot.busy)
            return true;
    return false;
}

////////////////////////////////////////////////////////////
// Queues a passive open on a listener slot; false if MacTCP refused it.
bool armListenSlot(Sock& s, ListenSlot& slot)
{
    if (!slot.stream.ptr && createTcpStream(slot.stream) != noErr)
    {
        slot.armed = false;
        return false;
    }

    slot.pb                                  = TCPiopb{};
    slot.pb.csParam.open.validityFlags       = 0;
    slot.pb.csParam.open.commandTimeoutValue = 0; // wait for a peer forever
    slot.pb.csParam.open.remoteHost          = 0;
    slot.pb.csParam.open.remotePort          = 0;
    slot.pb.csParam.open.localHost           = 0;
    slot.pb.csParam.open.localPort           = s.localPort;
    const OSErr err                          = tcpCall(slot.pb, TCPPassiveOpen, slot.stream.ptr, true);
    slot.armed                               = (err == noErr || pbBusy(slot.pb.ioResult));
    if (!slot.armed)
    {
        s.pendingError = mapError(err);
        releaseTcpStream(slot.stream);
    }
    return slot.armed;
}

// Re-arms listener slots whose passive open failed (e.g. a peer that reset
// mid-handshake); returns the slot holding a connection, if any.
ListenSlot* serviceListener(Sock& s)
{
    ListenSlot* ready = nullptr;
    for (int i = 0; i < s.listenCount; ++i)
    {
        ListenSlot& slot = s.listenSlots[i];
        if (!slot.armed || pbBusy(slot.pb.ioResult))
            continue;
        if (pbResult(slot.pb.ioResult) == noErr)
        {
            if (!ready)
                ready = &slot;
            continue;
        }
        // Failed: start over on a fresh stream.
        releaseTcpStream(slot.stream);
        armListenSlot(s, slot);
    }
    return ready;
}

bool listenerAlive(const Sock& s)
{
    for (int i = 0; i < s.listenCount; ++i)
        if (s.listenSlots[i].armed)
            return true;
    return false;
}

////////////////////////////////////////////////////////////
// Starts a UDPRead if none is outstanding; true if a datagram is waiting.
bool udpReadable(Sock& s)
{
    if (!s.udpStream.ptr)
        return false;

    if (s.readQueued)
    {
        if (pbBusy(s.readPb.ioResult))
            return false;
        if (pbResult(s.readPb.ioResult) == noErr)
            return true;
        s.readQueued = false; // timed out or failed: ask again
    }

    s.readPb                          = UdpIopb{};
    s.readPb.csParam.receive.timeOut  = 0; // no timeout
    s.readPb.csParam.receive.rcvBuff  = nullptr;
    s.readPb.csParam.receive.rcvBuffLen = 0;
    const OSErr err                   = udpCall(s.readPb, udpRead, s.udpStream.ptr, true);
    s.readQueued                      = (err == noErr || pbBusy(s.readPb.ioResult));
    return s.readQueued && !pbBusy(s.readPb.ioResult) && pbResult(s.readPb.ioResult) == noErr;
}

int ensureUdpStream(Sock& s)
{
    if (s.udpStream.ptr)
        return 0;
    const OSErr err = createUdpStream(s);
    return err == noErr ? 0 : mapError(err);
}

////////////////////////////////////////////////////////////
bool readable(Sock& s)
{
    if (s.kind == Kind::Udp)
        return udpReadable(s);

    switch (s.state)
    {
        case TcpState::Listening:
            return serviceListener(s) != nullptr || !listenerAlive(s);
        case TcpState::Connecting:
            settleConnect(s);
            return s.state == TcpState::Failed;
        case TcpState::Failed:
            return true;
        case TcpState::Idle:
            return false;
        case TcpState::Connected:
            break;
    }

    TCPiopb pb{};
    if (tcpCall(pb, TCPStatus, s.stream.ptr, false) != noErr)
        return true; // gone: recv() reports it
    const TCPStatusPB& st = pb.csParam.status;
    if (st.amtUnreadData > 0)
        return true;
    switch (st.connectionState)
    {
        case TCPSEstablished:
        case TCPSFinWait1:
        case TCPSFinWait2:
        case TCPSSynSent:
        case TCPSSynReceived:
            return false;
        default:
            return true; // peer closed: recv() returns 0
    }
}

bool writable(Sock& s)
{
    if (s.kind == Kind::Udp)
        return true;

    switch (s.state)
    {
        case TcpState::Connecting:
            settleConnect(s);
            return s.state != TcpState::Connecting;
        case TcpState::Connected:
            return s.sendError != 0 || freeSendSlot(s) != nullptr;
        case TcpState::Failed:
            return true;
        default:
            return false;
    }
}

////////////////////////////////////////////////////////////
void fillAddress(struct sockaddr* addr, socklen_t* len, uint32_t host, uint16_t port)
{
    if (!addr || !len)
        return;
    sockaddr_in in{};
    in.sin_len         = sizeof(in);
    in.sin_family      = AF_INET;
    in.sin_port        = htons(port);
    in.sin_addr.s_addr = htonl(host);
    const socklen_t n  = *len < sizeof(in) ? *len : static_cast<socklen_t>(sizeof(in));
    std::memcpy(addr, &in, n);
    *len = sizeof(in);
}

bool readAddress(const struct sockaddr* addr, socklen_t len, uint32_t& host, uint16_t& port)
{
    if (!addr || len < sizeof(sockaddr_in))
        return false;
    sockaddr_in in{};
    std::memcpy(&in, addr, sizeof(in));
    if (in.sin_family != AF_INET)
        return false;
    host = ntohl(in.sin_addr.s_addr);
    port = ntohs(in.sin_port);
    return true;
}

uint32_t ourAddress(const Sock& s)
{
    return s.localAddr ? s.localAddr : g_localAddr;
}

////////////////////////////////////////////////////////////
void closeSocket(Sock& s, bool graceful)
{
    if (s.kind == Kind::Tcp)
    {
        if (s.state == TcpState::Listening)
        {
            for (int i = 0; i < s.listenCount; ++i)
            {
                releaseTcpStream(s.listenSlots[i].stream);
                settle(s.listenSlots[i].pb.ioResult, 1'000'000);
            }
            delete[] s.listenSlots;
            s.listenSlots = nullptr;
            s.listenCount = 0;
        }

        settleConnect(s);
        if (graceful && s.state == TcpState::Connected && s.stream.ptr)
        {
            // Let queued data go out, then send our FIN.
            const std::uint64_t start = nowMicros();
            while (sendsPending(s) && s.sendError == 0 && nowMicros() - start < 2'000'000)
                nap();
            if (!s.closeQueued && s.sendError == 0)
            {
                s.closePb                              = TCPiopb{};
                s.closePb.csParam.close.validityFlags = 0;
                tcpCall(s.closePb, TCPClose, s.stream.ptr, true);
                s.closeQueued = true;
            }
            settle(s.closePb.ioResult, 500'000);
        }

        releaseTcpStream(s.stream);
        settle(s.openPb.ioResult, 1'000'000);
        settle(s.closePb.ioResult, 1'000'000);
        for (SendSlot& slot : s.sends)
        {
            settle(slot.pb.ioResult, 1'000'000);
            if (slot.data)
                DisposePtr(slot.data);
            slot = SendSlot{};
        }
    }
    else if (s.kind == Kind::Udp)
    {
        releaseUdpStream(s);
    }

    s.kind = Kind::Free;
}

} // namespace


extern "C" {

////////////////////////////////////////////////////////////
int mactcp_available(void)
{
    const int err = openDriver();
    return err ? -err : 0;
}

uint32_t mactcp_local_address(void)
{
    openDriver();
    return htonl(g_localAddr);
}

int mactcp_is_socket(int fd)
{
    return lookup(fd) != nullptr;
}

void mactcp_shutdown(void)
{
    for (Sock& s : g_socks)
        if (s.kind != Kind::Free)
            closeSocket(s, false);
    sf::priv::closeResolver();
}

////////////////////////////////////////////////////////////
int socket(int domain, int type, int protocol)
{
    if (domain != AF_INET)
        return fail(EAFNOSUPPORT);
    if (type != SOCK_STREAM && type != SOCK_DGRAM)
        return fail(EPROTONOSUPPORT);
    if (protocol != 0 && protocol != (type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP))
        return fail(EPROTONOSUPPORT);
    if (const int err = openDriver())
        return fail(err);

    Sock* s = allocate(type == SOCK_STREAM ? Kind::Tcp : Kind::Udp);
    if (!s)
        return fail(EMFILE);
    return descriptorOf(*s);
}

int mactcp_close(int fd)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    closeSocket(*s, true);
    return 0;
}

int mactcp_set_nonblocking(int fd, int nonblocking)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    s->nonBlocking = nonblocking != 0;
    return 0;
}

////////////////////////////////////////////////////////////
int bind(int fd, const struct sockaddr* addr, socklen_t len)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);

    uint32_t host = 0;
    uint16_t port = 0;
    if (!readAddress(addr, len, host, port))
        return fail(EINVAL);
    if (host != 0 && host != g_localAddr)
        return fail(EADDRNOTAVAIL); // MacTCP has the one interface

    if (s->kind == Kind::Udp)
    {
        if (s->udpStream.ptr)
            return fail(EINVAL);
        s->localPort = port;
        if (const int err = ensureUdpStream(*s))
            return fail(err);
        return 0;
    }

    if (s->state != TcpState::Idle)
        return fail(EINVAL);
    s->localAddr = host;
    s->localPort = port;
    return 0;
}

int listen(int fd, int backlog)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    if (s->kind != Kind::Tcp)
        return fail(EOPNOTSUPP);
    if (s->state == TcpState::Listening)
        return 0;
    if (s->state != TcpState::Idle)
        return fail(EINVAL);

    int count = backlog < 1 ? 1 : backlog;
    if (count > SOMAXCONN)
        count = SOMAXCONN;

    s->listenSlots = new (std::nothrow) ListenSlot[static_cast<std::size_t>(count)];
    if (!s->listenSlots)
        return fail(ENOBUFS);
    s->listenCount = count;
    s->state       = TcpState::Listening;

    // The first slot decides: a port in use fails it immediately.
    s->pendingError = 0;
    if (!armListenSlot(*s, s->listenSlots[0]))
    {
        const int err = s->pendingError ? s->pendingError : EADDRINUSE;
        delete[] s->listenSlots;
        s->listenSlots  = nullptr;
        s->listenCount  = 0;
        s->state        = TcpState::Idle;
        s->pendingError = 0;
        return fail(err);
    }
    for (int i = 1; i < count; ++i)
        armListenSlot(*s, s->listenSlots[i]);
    s->pendingError = 0;
    return 0;
}

int accept(int fd, struct sockaddr* addr, socklen_t* len)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    if (s->kind != Kind::Tcp || s->state != TcpState::Listening)
        return fail(EINVAL);

    for (;;)
    {
        if (ListenSlot* slot = serviceListener(*s))
        {
            Sock* conn = allocate(Kind::Tcp);
            if (!conn)
                return fail(EMFILE); // the connection stays queued

            conn->stream     = slot->stream;
            conn->state      = TcpState::Connected;
            conn->remoteAddr = slot->pb.csParam.open.remoteHost;
            conn->remotePort = slot->pb.csParam.open.remotePort;
            conn->localPort  = slot->pb.csParam.open.localPort;
            conn->localAddr  = slot->pb.csParam.open.localHost;

            slot->stream = Stream{};
            armListenSlot(*s, *slot);

            fillAddress(addr, len, conn->remoteAddr, conn->remotePort);
            return descriptorOf(*conn);
        }

        if (!listenerAlive(*s))
            return fail(s->pendingError ? s->pendingError : ECONNABORTED);
        if (s->nonBlocking)
            return fail(EWOULDBLOCK);
        nap();
    }
}

int connect(int fd, const struct sockaddr* addr, socklen_t len)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);

    uint32_t host = 0;
    uint16_t port = 0;

    if (s->kind == Kind::Udp)
    {
        if (addr && len >= sizeof(sockaddr) && addr->sa_family == AF_UNSPEC)
        {
            s->udpConnected = false;
            return 0;
        }
        if (!readAddress(addr, len, host, port))
            return fail(EINVAL);
        s->remoteAddr   = host;
        s->remotePort   = port;
        s->udpConnected = true;
        return 0;
    }

    switch (s->state)
    {
        case TcpState::Listening:
            return fail(EOPNOTSUPP);
        case TcpState::Connected:
            return fail(EISCONN);
        case TcpState::Connecting:
            settleConnect(*s);
            if (s->state == TcpState::Connecting)
                return fail(EALREADY);
            if (s->state == TcpState::Connected)
                return fail(EISCONN);
            [[fallthrough]];
        case TcpState::Failed:
        {
            const int err   = s->pendingError ? s->pendingError : ECONNREFUSED;
            s->pendingError = 0;
            s->state        = TcpState::Idle;
            releaseTcpStream(s->stream);
            return fail(err);
        }
        case TcpState::Idle:
            break;
    }

    if (!readAddress(addr, len, host, port))
        return fail(EINVAL);
    if (host == 0 || port == 0)
        return fail(EADDRNOTAVAIL);

    if (!s->stream.ptr)
    {
        const OSErr err = createTcpStream(s->stream);
        if (err != noErr)
            return fail(mapError(err));
    }

    s->remoteAddr                            = host;
    s->remotePort                            = port;
    s->openPb                                = TCPiopb{};
    s->openPb.csParam.open.validityFlags     = 0; // MacTCP's own timeouts
    s->openPb.csParam.open.remoteHost        = host;
    s->openPb.csParam.open.remotePort        = port;
    s->openPb.csParam.open.localHost         = 0;
    s->openPb.csParam.open.localPort         = s->localPort;
    const OSErr err                          = tcpCall(s->openPb, TCPActiveOpen, s->stream.ptr, true);
    if (err != noErr && !pbBusy(s->openPb.ioResult))
        return fail(mapError(err, true));
    s->state = TcpState::Connecting;

    if (s->nonBlocking)
        return fail(EINPROGRESS);

    while (pbBusy(s->openPb.ioResult))
        nap();
    settleConnect(*s);
    if (s->state == TcpState::Connected)
        return 0;

    const int failure = s->pendingError ? s->pendingError : ECONNREFUSED;
    s->pendingError   = 0;
    s->state          = TcpState::Idle;
    releaseTcpStream(s->stream);
    return fail(failure);
}

////////////////////////////////////////////////////////////
ssize_t send(int fd, const void* buf, size_t len, int flags)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);

    if (s->kind == Kind::Udp)
    {
        if (!s->udpConnected)
            return fail(EDESTADDRREQ);
        sockaddr_in to{};
        to.sin_len         = sizeof(to);
        to.sin_family      = AF_INET;
        to.sin_port        = htons(s->remotePort);
        to.sin_addr.s_addr = htonl(s->remoteAddr);
        return sendto(fd, buf, len, flags, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    }

    const bool wait = !s->nonBlocking && !(flags & MSG_DONTWAIT);

    settleConnect(*s);
    while (s->state == TcpState::Connecting && wait)
    {
        nap();
        settleConnect(*s);
    }
    if (s->state == TcpState::Connecting)
        return fail(EWOULDBLOCK);
    if (s->state != TcpState::Connected)
        return fail(s->pendingError ? s->pendingError : ENOTCONN);
    if (s->closeQueued)
        return fail(EPIPE);
    if (len == 0)
        return 0;

    SendSlot* slot = freeSendSlot(*s);
    while (!slot && s->sendError == 0 && wait)
    {
        nap();
        slot = freeSendSlot(*s);
    }
    if (s->sendError)
        return fail(s->sendError);
    if (!slot)
        return fail(EWOULDBLOCK);

    if (!slot->data)
    {
        slot->data = NewPtr(kSendChunk);
        if (!slot->data)
            return fail(ENOBUFS);
    }

    const uint16_t n = len > kSendChunk ? kSendChunk : static_cast<uint16_t>(len);
    std::memcpy(slot->data, buf, n);
    slot->wds[0].length                   = n;
    slot->wds[0].ptr                      = slot->data;
    slot->wds[1].length                   = 0;
    slot->wds[1].ptr                      = nullptr;
    slot->pb                              = TCPiopb{};
    slot->pb.csParam.send.validityFlags   = 0;
    slot->pb.csParam.send.pushFlag        = 1; // no Nagle-style holding back
    slot->pb.csParam.send.urgentFlag      = 0;
    slot->pb.csParam.send.wdsPtr          = reinterpret_cast<Ptr>(slot->wds);
    const OSErr err                       = tcpCall(slot->pb, TCPSend, s->stream.ptr, true);
    if (err != noErr && !pbBusy(slot->pb.ioResult))
        return fail(mapError(err));
    slot->busy = true;
    return n;
}

ssize_t recv(int fd, void* buf, size_t len, int flags)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);

    if (s->kind == Kind::Udp)
        return recvfrom(fd, buf, len, flags, nullptr, nullptr);

    if (flags & (MSG_PEEK | MSG_OOB))
        return fail(EOPNOTSUPP);

    const bool wait = !s->nonBlocking && !(flags & MSG_DONTWAIT);

    settleConnect(*s);
    while (s->state == TcpState::Connecting && wait)
    {
        nap();
        settleConnect(*s);
    }
    if (s->state == TcpState::Connecting)
        return fail(EWOULDBLOCK);
    if (s->state != TcpState::Connected)
        return fail(s->pendingError ? s->pendingError : ENOTCONN);
    if (len == 0)
        return 0;

    for (;;)
    {
        TCPiopb pb{};
        if (tcpCall(pb, TCPStatus, s->stream.ptr, false) != noErr)
            return 0; // the connection is gone

        if (pb.csParam.status.amtUnreadData > 0)
        {
            TCPiopb rcv{};
            rcv.csParam.receive.commandTimeoutValue = 1; // data is waiting: never hit
            rcv.csParam.receive.rcvBuff             = static_cast<Ptr>(buf);
            rcv.csParam.receive.rcvBuffLen          = len > 0xffff ? 0xffff : static_cast<uint16_t>(len);
            const OSErr err                         = tcpCall(rcv, TCPRcv, s->stream.ptr, false);
            if (err == noErr)
                return rcv.csParam.receive.rcvBuffLen;
            if (err == connectionClosing || err == connectionDoesntExist || err == connectionTerminated)
                return 0;
            if (err != commandTimeout)
                return fail(mapError(err));
        }
        else
        {
            switch (pb.csParam.status.connectionState)
            {
                case TCPSEstablished:
                case TCPSFinWait1:
                case TCPSFinWait2:
                    break;
                default:
                    return 0; // the peer has closed its side
            }
        }

        if (!wait)
            return fail(EWOULDBLOCK);
        nap();
    }
}

////////////////////////////////////////////////////////////
ssize_t sendto(int fd, const void* buf, size_t len, int flags, const struct sockaddr* to, socklen_t tolen)
{
    (void)flags;

    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    if (s->kind == Kind::Tcp)
        return to ? fail(EISCONN) : send(fd, buf, len, flags);

    uint32_t host = s->remoteAddr;
    uint16_t port = s->remotePort;
    if (to)
    {
        if (!readAddress(to, tolen, host, port))
            return fail(EINVAL);
    }
    else if (!s->udpConnected)
    {
        return fail(EDESTADDRREQ);
    }
    if (len > 0xffff)
        return fail(EMSGSIZE);
    if (const int err = ensureUdpStream(*s))
        return fail(err);

    wdsEntry wds[2]{};
    wds[0].length = static_cast<uint16_t>(len);
    wds[0].ptr    = static_cast<Ptr>(const_cast<void*>(buf));

    UdpIopb pb{};
    pb.csParam.send.remoteHost = host;
    pb.csParam.send.remotePort = port;
    pb.csParam.send.wdsPtr     = reinterpret_cast<Ptr>(wds);
    pb.csParam.send.checkSum   = true;
    const OSErr err            = udpCall(pb, udpWrite, s->udpStream.ptr, false);
    if (err != noErr)
        return fail(mapError(err));
    return static_cast<ssize_t>(len);
}

ssize_t recvfrom(int fd, void* buf, size_t len, int flags, struct sockaddr* from, socklen_t* fromlen)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    if (s->kind == Kind::Tcp)
    {
        const ssize_t n = recv(fd, buf, len, flags);
        if (n >= 0 && from)
            getpeername(fd, from, fromlen);
        return n;
    }
    if (const int err = ensureUdpStream(*s))
        return fail(err);

    const bool wait = !s->nonBlocking && !(flags & MSG_DONTWAIT);
    while (!udpReadable(*s))
    {
        if (!wait)
            return fail(EWOULDBLOCK);
        nap();
    }

    UdpReceivePB& r   = s->readPb.csParam.receive;
    const size_t  got = r.rcvBuffLen;
    const size_t  n   = got < len ? got : len; // the rest of the datagram is dropped, as in BSD
    if (n)
        std::memcpy(buf, r.rcvBuff, n);
    fillAddress(from, fromlen, r.remoteHost, r.remotePort);

    // Hand the driver its buffer back.
    UdpIopb ret{};
    ret.csParam.receive.rcvBuff = r.rcvBuff;
    udpCall(ret, udpBfrReturn, s->udpStream.ptr, false);
    s->readQueued = false;

    return static_cast<ssize_t>(n);
}

////////////////////////////////////////////////////////////
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout)
{
    const bool          forever  = timeout == nullptr;
    const std::uint64_t limit    = forever ? 0
                                           : static_cast<std::uint64_t>(timeout->tv_sec) * 1'000'000 +
                                              static_cast<std::uint64_t>(timeout->tv_usec);
    const std::uint64_t start    = nowMicros();
    const int           last     = nfds < MACTCP_FIRST_FD + kMaxSockets ? nfds : MACTCP_FIRST_FD + kMaxSockets;

    fd_set wantRead;
    fd_set wantWrite;
    FD_ZERO(&wantRead);
    FD_ZERO(&wantWrite);
    for (int fd = 0; fd < nfds && fd < FD_SETSIZE; ++fd)
    {
        const bool r = readfds && FD_ISSET(fd, readfds);
        const bool w = writefds && FD_ISSET(fd, writefds);
        if ((r || w) && !lookup(fd))
            return fail(EBADF);
        if (r)
            FD_SET(fd, &wantRead);
        if (w)
            FD_SET(fd, &wantWrite);
    }
    if (exceptfds)
        FD_ZERO(exceptfds); // MacTCP has no out-of-band data to report

    for (;;)
    {
        int    count = 0;
        fd_set gotRead;
        fd_set gotWrite;
        FD_ZERO(&gotRead);
        FD_ZERO(&gotWrite);
        for (int fd = MACTCP_FIRST_FD; fd < last; ++fd)
        {
            Sock* s = lookup(fd);
            if (!s)
                continue;
            if (FD_ISSET(fd, &wantRead) && readable(*s))
            {
                FD_SET(fd, &gotRead);
                ++count;
            }
            if (FD_ISSET(fd, &wantWrite) && writable(*s))
            {
                FD_SET(fd, &gotWrite);
                ++count;
            }
        }

        if (count > 0 || (!forever && nowMicros() - start >= limit))
        {
            if (readfds)
                *readfds = gotRead;
            if (writefds)
                *writefds = gotWrite;
            return count;
        }
        nap();
    }
}

////////////////////////////////////////////////////////////
int getsockname(int fd, struct sockaddr* addr, socklen_t* len)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    if (!addr || !len)
        return fail(EFAULT);

    uint16_t port = s->localPort;
    if (s->kind == Kind::Tcp)
    {
        settleConnect(*s);
        if (s->state == TcpState::Connected && s->stream.ptr)
        {
            TCPiopb pb{};
            if (tcpCall(pb, TCPStatus, s->stream.ptr, false) == noErr)
                port = pb.csParam.status.localPort;
        }
    }
    fillAddress(addr, len, ourAddress(*s), port);
    return 0;
}

int getpeername(int fd, struct sockaddr* addr, socklen_t* len)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    if (!addr || !len)
        return fail(EFAULT);

    if (s->kind == Kind::Udp)
    {
        if (!s->udpConnected)
            return fail(ENOTCONN);
    }
    else
    {
        settleConnect(*s);
        if (s->state != TcpState::Connected)
            return fail(s->state == TcpState::Failed && s->pendingError ? s->pendingError : ENOTCONN);
    }
    fillAddress(addr, len, s->remoteAddr, s->remotePort);
    return 0;
}

////////////////////////////////////////////////////////////
int setsockopt(int fd, int level, int name, const void* value, socklen_t len)
{
    (void)level;
    (void)name;
    (void)value;
    (void)len;

    // TCP_NODELAY: every TCPSend is pushed already. SO_REUSEADDR,
    // SO_BROADCAST and the rest: MacTCP has no such switches, and does
    // what callers ask for by them anyway.
    if (!lookup(fd))
        return fail(EBADF);
    return 0;
}

int getsockopt(int fd, int level, int name, void* value, socklen_t* len)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    if (!value || !len || *len < sizeof(int))
        return fail(EINVAL);

    int result = 0;
    if (level == SOL_SOCKET && name == SO_ERROR)
    {
        if (s->kind == Kind::Tcp)
            settleConnect(*s);
        result          = s->pendingError;
        s->pendingError = 0;
    }
    else if (level == SOL_SOCKET && name == SO_TYPE)
    {
        result = s->kind == Kind::Tcp ? SOCK_STREAM : SOCK_DGRAM;
    }
    else if (level == SOL_SOCKET && (name == SO_RCVBUF || name == SO_SNDBUF))
    {
        result = static_cast<int>(s->kind == Kind::Tcp ? kTcpBufferSize : kUdpBufferSize);
    }
    std::memcpy(value, &result, sizeof(result));
    *len = sizeof(result);
    return 0;
}

int shutdown(int fd, int how)
{
    Sock* s = lookup(fd);
    if (!s)
        return fail(EBADF);
    if (s->kind != Kind::Tcp)
        return fail(EOPNOTSUPP);
    settleConnect(*s);
    if (s->state != TcpState::Connected)
        return fail(ENOTCONN);
    if ((how == SHUT_WR || how == SHUT_RDWR) && !s->closeQueued)
    {
        // TCPClose sends our FIN after the data already queued.
        s->closePb                              = TCPiopb{};
        s->closePb.csParam.close.validityFlags = 0;
        tcpCall(s->closePb, TCPClose, s->stream.ptr, true);
        s->closeQueued = true;
    }
    return 0;
}

////////////////////////////////////////////////////////////
int gethostname(char* name, size_t len)
{
    openDriver();
    char text[INET_ADDRSTRLEN];
    in_addr addr{htonl(g_localAddr)};
    inet_ntop(AF_INET, &addr, text, sizeof(text));
    if (std::strlen(text) + 1 > len)
        return fail(ENAMETOOLONG);
    std::strcpy(name, text);
    return 0;
}

} // extern "C"
