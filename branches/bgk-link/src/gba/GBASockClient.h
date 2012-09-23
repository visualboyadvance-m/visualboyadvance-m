#pragma once

#include <SFML/Network.hpp>
#include "../common/Types.h"

class GBASockClient
{
public:
	GBASockClient();
	~GBASockClient();

	bool Connect(sf::IPAddress server_addr);
	void Send(std::vector<char> data);
	char ReceiveCmd(char* data_in);

private:
	sf::IPAddress server_addr;
	sf::SocketTCP client;
};
