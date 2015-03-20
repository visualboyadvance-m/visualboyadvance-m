#ifndef NO_LINK

#include "GBASockClient.h"

// Currently only for Joybus communications

GBASockClient::GBASockClient(sf::IPAddress _server_addr)
{
	if (!_server_addr.IsValid())
		server_addr = sf::IPAddress::LocalHost;
	else
		server_addr = _server_addr;

	client.Connect(0xd6ba, server_addr);
	client.SetBlocking(false);

	clock_client.Connect(0xc10c, server_addr);
	clock_client.SetBlocking(false);

	clock_sync = 0;
	is_disconnected = false;
}

GBASockClient::~GBASockClient()
{
	client.Close();
	clock_client.Close();
}

u32 clock_sync_ticks = 0;

void GBASockClient::Send(std::vector<char> data)
{
	char* plain_data = new char[data.size()];
	std::copy(data.begin(), data.end(), plain_data);

	client.Send(plain_data, data.size());

	delete[] plain_data;
}

// Returns cmd for convenience
char GBASockClient::ReceiveCmd(char* data_in, bool block)
{
	if (IsDisconnected())
		return data_in[0];

	std::size_t num_received = 0;
	if (block || clock_sync == 0)
	{
		sf::SelectorTCP Selector;
		Selector.Add(client);
		Selector.Wait(6);
	}
	if (client.Receive(data_in, 5, num_received) == sf::Socket::Disconnected)
		Disconnect();

	return data_in[0];
}

void GBASockClient::ReceiveClock(bool block)
{
	if (IsDisconnected())
		return;

	char sync_ticks[4] = { 0, 0, 0, 0 };
	std::size_t num_received = 0;
	if (clock_client.Receive(sync_ticks, 4, num_received) == sf::Socket::Disconnected)
		Disconnect();

	if (num_received == 4)
	{
		clock_sync_ticks = 0;
		for (int i = 0; i < 4; i++)
			clock_sync_ticks |= (u8)(sync_ticks[i]) << ((3 - i) * 8);
		clock_sync += clock_sync_ticks;
	}
}

void GBASockClient::ClockSync(u32 ticks)
{
	if (clock_sync > (s32)ticks)
		clock_sync -= (s32)ticks;
	else
		clock_sync = 0;
}

void GBASockClient::Disconnect()
{
	is_disconnected = true;
	client.Close();
	clock_client.Close();
}

bool GBASockClient::IsDisconnected()
{
	return !client.IsValid() || !clock_client.IsValid() || is_disconnected;
}
#endif // NO_LINK
