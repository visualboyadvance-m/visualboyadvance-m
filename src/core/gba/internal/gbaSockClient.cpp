// winsock2.h must come before anything that may pull in windows.h, which the
// SFML network headers do (via ghc filesystem.hpp), or the mingw headers warn.
#if defined(_WIN32)
#include <winsock2.h>
#endif

#include "core/gba/internal/gbaSockClient.h"

#if defined(NO_LINK)
#error "This file should not be compiled with NO_LINK."
#endif  // defined(NO_LINK)

#if !defined(_WIN32)
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

// Currently only for Joybus communications

// The JoyBus protocol is a per-command request/response of a few bytes;
// Nagle + delayed ACK would serialize it at ACK latency (worst on Windows,
// ~200 ms), so disable it like the LAN link sockets do (see gbaLink.cpp).
static void SockSetNoDelay(sf::TcpSocket& sock)
{
    const int one = 1;
    setsockopt(sock.getNativeHandle(), IPPROTO_TCP, TCP_NODELAY,
        (const char*)&one, sizeof(one));
}

GBASockClient::GBASockClient(sf::IpAddress _server_addr)
{
    server_addr = _server_addr;

    (void)client.connect(server_addr, 0xd6ba);
    client.setBlocking(false);
    SockSetNoDelay(client);

    (void)clock_client.connect(server_addr, 0xc10c);
    clock_client.setBlocking(false);
    SockSetNoDelay(clock_client);

    clock_sync = 0;
    is_disconnected = false;
}

GBASockClient::~GBASockClient()
{
    client.disconnect();
    clock_client.disconnect();
}

uint32_t clock_sync_ticks = 0;

void GBASockClient::Send(std::vector<char> data)
{
    char* plain_data = new char[data.size()];
    std::copy(data.begin(), data.end(), plain_data);

    (void)client.send(plain_data, data.size());

    delete[] plain_data;
}

// Returns cmd for convenience
char GBASockClient::ReceiveCmd(char* data_in, bool block)
{
    if (IsDisconnected()) {
        return data_in[0];
    }

    std::size_t num_received = 0;
    if (block || clock_sync == 0) {
        sf::SocketSelector Selector;
        Selector.add(client);
        (void)Selector.wait(sf::seconds(6));
    }
    if (client.receive(data_in, 5, num_received) == sf::Socket::Status::Disconnected) {
        Disconnect();
    }

    return data_in[0];
}

void GBASockClient::ReceiveClock(bool block)
{
    (void)block; // unused param
    if (IsDisconnected())
        return;

    char sync_ticks[4] = { 0, 0, 0, 0 };
    std::size_t num_received = 0;
    if (clock_client.receive(sync_ticks, 4, num_received) == sf::Socket::Status::Disconnected) {
        Disconnect();
    }

    if (num_received == 4) {
        clock_sync_ticks = 0;
        for (int i = 0; i < 4; i++) {
            clock_sync_ticks |= (uint8_t)(sync_ticks[i]) << ((3 - i) * 8);
        }
        clock_sync += clock_sync_ticks;
    }
}

void GBASockClient::ClockSync(uint32_t ticks)
{
    if (clock_sync > (int32_t)ticks)
        clock_sync -= (int32_t)ticks;
    else
        clock_sync = 0;
}

void GBASockClient::Disconnect()
{
    is_disconnected = true;
    client.disconnect();
    clock_client.disconnect();
}

bool GBASockClient::IsDisconnected()
{
    return is_disconnected;
}
