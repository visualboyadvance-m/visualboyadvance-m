#ifndef VBAM_CORE_GBA_INTERNAL_GBASOCKCLIENT_H_
#define VBAM_CORE_GBA_INTERNAL_GBASOCKCLIENT_H_

#if defined(NO_LINK)
#error "This file should not be included with NO_LINK."
#endif  // defined(NO_LINK)

#include <cstdint>

#include <SFML/Network.hpp>

class GBASockClient {
public:
    GBASockClient(sf::IpAddress _server_addr);
    ~GBASockClient();

    bool Connect(sf::IpAddress server_addr);
    void Send(std::vector<char> data);
    char ReceiveCmd(char* data_in, bool block);
    void ReceiveClock(bool block);

    void ClockSync(uint32_t ticks);
    void Disconnect();
    bool IsDisconnected();

private:
    sf::IpAddress server_addr;
    sf::TcpSocket client;
    sf::TcpSocket clock_client;

    int32_t clock_sync;
    bool is_disconnected;
};

#endif  // VBAM_CORE_GBA_INTERNAL_GBASOCKCLIENT_H_
