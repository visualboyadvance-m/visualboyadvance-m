#pragma once

#include <SFML/Network.hpp>
#include "../common/Types.h"

class GBASockClient : public sf::SocketTCP
{
public:
	GBASockClient(sf::IPAddress server_addr);
	~GBASockClient();

	void Send(std::vector<char> data);
	char ReceiveCmd(char* data_in, bool block);
	void ReceiveClock(bool block);

	void ClockSync(u32 ticks);
	void Disconnect();
	bool IsDisconnected();

private:
	sf::IPAddress server_addr;
	sf::SocketTCP client;
	sf::SocketTCP clock_client;

	s32 clock_sync;
	bool is_disconnected;
};
