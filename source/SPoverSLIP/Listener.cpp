#include "StdAfx.h"

#include <atomic>
#include <iostream>

#include "Listener.h"
#include "TCPConnection.h"
#include "SLIP.h"

#include "Log.h"
#include "Requestor.h"
#include "StatusRequest.h"
#include "StatusResponse.h"

uint8_t Listener::next_device_id_ = 1;

Listener::~Listener()
{
	stop();
}

std::thread Listener::create_listener_thread()
{
	return std::thread(&Listener::listener_function, this);
}

void Listener::listener_function()
{
	LogFileOutput("Listener::listener_function - RUNNING\n");
	int server_fd, new_socket;
	struct sockaddr_in address;
	int address_length = sizeof(address);
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		LogFileOutput("Listener::listener_function - Socket creation failed\n");
		return;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(port_);
	address.sin_addr.s_addr = inet_addr(ip_address_.c_str());

	if (bind(server_fd, reinterpret_cast<SOCKADDR*>(&address), sizeof(address)) == SOCKET_ERROR)
	{
		LogFileOutput("Listener::listener_function - bind failed\n");
		return;
	}

	if (listen(server_fd, 3) < 0)
	{
		LogFileOutput("Listener::listener_function - listen failed\n");
		return;
	}

	while (is_listening_)
	{
		fd_set sockSet;
		FD_ZERO(&sockSet);
		FD_SET(server_fd, &sockSet);

		timeval timeout;
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;

		const int activity = select(0, &sockSet, nullptr, nullptr, &timeout);

		if (activity == SOCKET_ERROR) {
			LogFileOutput("Listener::listener_function - select failed\n");
			is_listening_ = false;
			break;
		}

		if (activity == 0) {
			// timeout occurred, no client connection. Still need to check is_listening_
			continue;
		}

		if ((new_socket = accept(server_fd, reinterpret_cast<SOCKADDR*>(&address), &address_length)) == INVALID_SOCKET)
		{
			LogFileOutput("Listener::listener_function - accept failed\n");
			is_listening_ = false;
			break;
		}

		create_connection(new_socket);
	}

	LogFileOutput("Listener::listener_function - listener closing down\n");

	closesocket(server_fd);
	WSACleanup();
}

// Creates a Connection object, which is how a SP will register itself with our listener.
void Listener::create_connection(int socket)
{
	// New implementation:
	//  - Send Status Request for Device Count
	//  - Add device_id range to the connection_map_ for this connection.
	// I did consider doing DIB here, but A2 OS can do that against each device ID as it needs it.

	const std::shared_ptr<Connection> conn = std::make_shared<TCPConnection>(socket);
	conn->create_read_channel();
	while (!conn->is_connected()) {}

	StatusRequest request(Requestor::next_request_number(), 0, 0);	// Device Count Request
	const auto response = Requestor::send_request(request, conn.get());
	const auto status_response = dynamic_cast<StatusResponse*>(response.get());
	if (status_response != nullptr)
	{
		if (status_response->get_status() == 0 && status_response->get_data()[0] > 0)
		{
			// Read the device count from the first byte of data, and save the connection so it persists
			const auto device_count = status_response->get_data()[0];
			const auto start_id = next_device_id_;
			const auto end_id = static_cast<uint8_t>(start_id + device_count - 1);
			next_device_id_ = end_id + 1;
			connection_map_[{start_id, end_id}] = conn;
		}
	}
}

void Listener::start()
{
	is_listening_ = true;
	listening_thread_ = std::thread(&Listener::listener_function, this);
}

void Listener::stop()
{
	LogFileOutput("Listener::stop()\n");
	if (is_listening_)
	{
		// Give our own thread time to stop while we stop the connections.
		is_listening_ = false;

		for (auto& pair : connection_map_)
		{
			const auto& connection = pair.second;
			connection->set_is_connected(false);
			connection->join();
		}
		LogFileOutput("Listener::stop() ... joining listener until it stops\n");
		listening_thread_.join();
	}
	LogFileOutput("Listener::stop() ... finished\n");
}

// Returns the lower bound of the device id, and connection.
std::pair<uint8_t, std::shared_ptr<Connection>> Listener::find_connection_with_device(const uint8_t device_id) const
{
	std::pair<uint8_t, std::shared_ptr<Connection>> result;
	for (const auto& kv : connection_map_) {
		if (device_id >= kv.first.first && device_id <= kv.first.second) {
			result = std::make_pair(kv.first.first, kv.second);
			break;
		}
	}
	return result;
}
