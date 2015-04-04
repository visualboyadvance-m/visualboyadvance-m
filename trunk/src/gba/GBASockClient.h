#pragma once

#include <SFML/Network.hpp>
#include "../common/Types.h"

class GBASockClient
{
public:
	GBASockClient(sf::IpAddress _server_addr);
	~GBASockClient();

	bool Connect(sf::IpAddress server_addr);
	void Send(std::vector<char> data);
	char ReceiveCmd(char* data_in, bool block);
	void ReceiveClock(bool block);

	void ClockSync(u32 ticks);
	void Disconnect();
	bool IsDisconnected();

private:
	sf::IpAddress server_addr;
	sf::TcpSocket client;
	sf::TcpSocket clock_client;

	s32 clock_sync;
	bool is_disconnected;
};
