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
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/Http.hpp>
#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/SocketImpl.hpp>

#include <SFML/System/Err.hpp>

#include <istream>
#include <ostream>

#include <cstring>


namespace sf
{
////////////////////////////////////////////////////////////
const IpAddress IpAddress::Any(0, 0, 0, 0);
const IpAddress IpAddress::LocalHost(127, 0, 0, 1);
const IpAddress IpAddress::Broadcast(255, 255, 255, 255);

// Define inet_pton() and inet_ntop() for Windows XP
//
// This is from:
// https://stackoverflow.com/a/20816961/262458
//
#if defined(_WIN32) && _WIN32_WINNT <= _WIN32_WINNT_WINXP
#include <winsock2.h>
#include <ws2tcpip.h>


static int inet_pton(int af, const char *src, void *dst)
{
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN];

    ZeroMemory(&ss, sizeof(ss));
    /* stupid non-const API */
    strncpy (src_copy, src, INET6_ADDRSTRLEN);
    src_copy[INET6_ADDRSTRLEN] = 0;

    if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
        switch(af) {
            case AF_INET:
                *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
                return 1;
            case AF_INET6:
                *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
                return 1;
        }
    }
    return 0;
}

static const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    struct sockaddr_storage ss;
    unsigned long s = size;

    ZeroMemory(&ss, sizeof(ss));
    ss.ss_family = af;

    switch(af) {
        case AF_INET:
            ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
            break;
        case AF_INET6:
            ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
            break;
        default:
            return NULL;
    }
    /* cannot direclty use &size because of strict aliasing rules */
    return (WSAAddressToStringA((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0)?
        dst : NULL;
}
#endif // defined(_WIN32) && _WIN32_WINNT <= _WIN32_WINNT_WINXP 

////////////////////////////////////////////////////////////
nonstd::optional<IpAddress> IpAddress::resolve(std::string address)
{
    if (address.empty())
    {
        // Not generating en error message here as resolution failure is a valid outcome.
        return nonstd::nullopt;
    }

    if (address == "255.255.255.255")
    {
        // The broadcast address needs to be handled explicitly,
        // because it is also the value returned by inet_addr on error
        return Broadcast;
    }

    if (address == "0.0.0.0")
        return Any;

    // Try to convert the address as a byte representation ("xxx.xxx.xxx.xxx")
    std::uint32_t ipaddr = 0;
    inet_pton(AF_INET, address.data(), &ipaddr);
    if (ipaddr != INADDR_NONE)
        return IpAddress(ntohl(ipaddr));

    // Not a valid address, try to convert it as a host name
    addrinfo hints{}; // Zero-initialize
    hints.ai_family = AF_INET;

    addrinfo* result = nullptr;
    if (getaddrinfo(address.data(), nullptr, &hints, &result) == 0 && result != nullptr)
    {
        sockaddr_in sin{};
        std::memcpy(&sin, result->ai_addr, sizeof(*result->ai_addr));

        const std::uint32_t ip = sin.sin_addr.s_addr;
        freeaddrinfo(result);

        return IpAddress(ntohl(ip));
    }

    // Not generating en error message here as resolution failure is a valid outcome.
    return nonstd::nullopt;
}


////////////////////////////////////////////////////////////
IpAddress::IpAddress(std::uint8_t byte0, std::uint8_t byte1, std::uint8_t byte2, std::uint8_t byte3) :
m_address(static_cast<std::uint32_t>((byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3))
{
}


////////////////////////////////////////////////////////////
IpAddress::IpAddress(std::uint32_t address) : m_address(address)
{
}


////////////////////////////////////////////////////////////
std::string IpAddress::toString() const
{
    char address_str[INET_ADDRSTRLEN];
    in_addr address{};
    address.s_addr = htonl(m_address);
    inet_ntop(AF_INET, &address, address_str, INET_ADDRSTRLEN);
    return address_str;
}


////////////////////////////////////////////////////////////
std::uint32_t IpAddress::toInteger() const
{
    return m_address;
}


////////////////////////////////////////////////////////////
nonstd::optional<IpAddress> IpAddress::getLocalAddress()
{
    // The method here is to connect a UDP socket to a public ip,
    // and get the local socket address with the getsockname function.
    // UDP connection will not send anything to the network, so this function won't cause any overhead.

    // Create the socket
    const SocketHandle sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == SocketImpl::invalidSocket())
    {
        err() << "Failed to retrieve local address (invalid socket)" << std::endl;
        return nonstd::nullopt;
    }

    // Connect the socket to a public ip (here 1.1.1.1) on any
    // port. This will give the local address of the network interface
    // used for default routing which is usually what we want.
    sockaddr_in address = SocketImpl::createAddress(0x01010101, 9);
    if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
    {
        SocketImpl::close(sock);

        err() << "Failed to retrieve local address (socket connection failure)" << std::endl;
        return nonstd::nullopt;
    }

    // Get the local address of the socket connection
    SocketImpl::AddrLength size = sizeof(address);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&address), &size) == -1)
    {
        SocketImpl::close(sock);

        err() << "Failed to retrieve local address (socket local address retrieval failure)" << std::endl;
        return nonstd::nullopt;
    }

    // Close the socket
    SocketImpl::close(sock);

    // Finally build the IP address
    return IpAddress(ntohl(address.sin_addr.s_addr));
}


////////////////////////////////////////////////////////////
nonstd::optional<IpAddress> IpAddress::getPublicAddress(Time timeout)
{
    // The trick here is more complicated, because the only way
    // to get our public IP address is to get it from a distant computer.
    // Here we get the web page from http://www.sfml-dev.org/ip-provider.php
    // and parse the result to extract our IP address
    // (not very hard: the web page contains only our IP address).

    Http                 server("www.sfml-dev.org");
    const Http::Request  request("/ip-provider.php", Http::Request::Method::Get);
    const Http::Response page = server.sendRequest(request, timeout);

    const Http::Response::Status status = page.getStatus();

    if (status == Http::Response::Status::Ok)
        return IpAddress::resolve(page.getBody());

    err() << "Failed to retrieve public address from external IP resolution server (HTTP response status "
          << static_cast<int>(status) << ")" << std::endl;

    return nonstd::nullopt;
}


////////////////////////////////////////////////////////////
bool operator==(IpAddress left, IpAddress right)
{
    return !(left < right) && !(right < left);
}


////////////////////////////////////////////////////////////
bool operator!=(IpAddress left, IpAddress right)
{
    return !(left == right);
}


////////////////////////////////////////////////////////////
bool operator<(IpAddress left, IpAddress right)
{
    return left.m_address < right.m_address;
}


////////////////////////////////////////////////////////////
bool operator>(IpAddress left, IpAddress right)
{
    return right < left;
}


////////////////////////////////////////////////////////////
bool operator<=(IpAddress left, IpAddress right)
{
    return !(right < left);
}


////////////////////////////////////////////////////////////
bool operator>=(IpAddress left, IpAddress right)
{
    return !(left < right);
}


////////////////////////////////////////////////////////////
std::istream& operator>>(std::istream& stream, nonstd::optional<IpAddress>& address)
{
    std::string str;
    stream >> str;
    address = IpAddress::resolve(str);

    return stream;
}


////////////////////////////////////////////////////////////
std::ostream& operator<<(std::ostream& stream, IpAddress address)
{
    return stream << address.toString();
}

} // namespace sf
