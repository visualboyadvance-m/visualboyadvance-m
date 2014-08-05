#pragma once

#include <SFML/Network.hpp>
#include "../common/Types.h"

class GBASockClient : public sf::TcpSocket
{
public:
	GBASockClient(sf::IpAddress server_addr);
	~GBASockClient();

	void Send(std::vector<char> data);
	char ReceiveCmd(char* data_in);

private:
	sf::IpAddress server_addr;
	sf::TcpSocket client;
};
