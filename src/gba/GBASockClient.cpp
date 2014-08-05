#ifndef NO_LINK

#include "GBASockClient.h"

// Currently only for Joybus communications

GBASockClient::GBASockClient(sf::IpAddress _server_addr)
{
	if (_server_addr == sf::IpAddress::None)
		server_addr = sf::IpAddress::LocalHost;
	else
		server_addr = _server_addr;

	client.connect(server_addr, 0xd6ba);
	//client.SetBlocking(false);
}

GBASockClient::~GBASockClient()
{
	client.disconnect();
}

void GBASockClient::Send(std::vector<char> data)
{
	char* plain_data = new char[data.size()];
	std::copy(data.begin(), data.end(), plain_data);

	client.send(plain_data, data.size());

	delete[] plain_data;
}

// Returns cmd for convenience
char GBASockClient::ReceiveCmd(char* data_in)
{
	std::size_t num_received;
	client.receive(data_in, 5, num_received);

	return data_in[0];
}

#endif // NO_LINK
