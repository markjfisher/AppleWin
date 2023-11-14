#include "StdAfx.h"

#include <atomic>
#include <iostream>

#include "Listener.h"
#include "TCPConnection.h"
#include "SLIP.h"
#include "Util.h"

#include "Log.h"

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
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port_);
	address.sin_addr.s_addr = inet_addr(ip_address_.c_str());

	if (bind(server_fd, reinterpret_cast<SOCKADDR*>(&address), sizeof(address)) == SOCKET_ERROR)
	{
		LogFileOutput("Listener::listener_function - Socket creation failed\n");
		return;
	}

	if (listen(server_fd, 3) < 0)
	{
		LogFileOutput("Listener::listener_function - listen failed\n");
		return;
	}

	while (is_listening_)
	{
		if ((new_socket = accept(server_fd, reinterpret_cast<SOCKADDR*>(&address), &address_length)) == INVALID_SOCKET)
		{
			LogFileOutput("Listener::listener_function - accept failed\n");
			is_listening_ = false;
			break;
		}

		// do it in a thread so we can respond to more requests as soon as possible
		std::thread([this, new_socket]()
		{
			create_connection(new_socket);
		}).detach();
	}

	LogFileOutput("Listener::listener_function - listener closing down\n");

	closesocket(server_fd);
	WSACleanup();
}

// Creates a Connection object, which is how a SP will register itself with our listener.
// When it connects, it tells us all the available unit IDs and names available to service requests.
// That connection will then be used to send requests to, and get responses back from.
void Listener::create_connection(int socket)
{
	std::vector<uint8_t> complete_data;
	std::vector<uint8_t> buffer(1024);

	int bytes_read;
	do
	{
		bytes_read = recv(socket, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
		if (bytes_read > 0)
		{
			complete_data.insert(complete_data.end(), buffer.begin(), buffer.begin() + bytes_read);
		}
	}
	while (bytes_read == 1024);

#ifdef DEBUG
	std::cout << "capability data from incoming connection:" << std::endl;
	Util::hex_dump(complete_data);
#endif

	// for every slip packet we received (probably only 1 as this is an initial connection) 
	// which are pairs of int, c-string (ID, Name (0 terminated))
	if (!complete_data.empty())
	{
		const std::vector<std::vector<uint8_t>> packets =
			SLIP::split_into_packets(complete_data.data(), complete_data.size());

		if (!packets.empty())
		{
			// We have at least some data that might pass as a capability, let's create the Connection object, and use it to parse the data
			const std::shared_ptr<Connection> conn = std::make_shared<TCPConnection>(socket);
			for (const auto& packet : packets)
			{
#ifdef DEBUG
				std::cout << ".. packet:" << std::endl;
				Util::hex_dump(packet);
#endif

				if (!packet.empty())
				{
					// create devices from the data
					conn->add_devices(packet);
				}
			}
			if (!conn->get_devices().empty())
			{
				// this connection is a keeper! it has some devices on it.
				conn->set_is_connected(true);
				conn->create_read_channel();
				// create a closure, so the mutex is released as it goes out of scope
				{
					std::lock_guard<std::mutex> lock(mtx_);
					connections_.push_back(conn);
				} // mtx_ unlocked here
			}
		}
	}
}

void Listener::start()
{
	is_listening_ = true;
	std::thread(&Listener::listener_function, this).detach();
}

void Listener::stop()
{
	is_listening_ = false;
}

Connection* Listener::find_connection_with_device(int device_id) const
{
	for (const auto& connection : connections_)
	{
		if (connection->find_device(device_id) != nullptr)
		{
			return connection.get();
		}
	}
	return nullptr;
}

size_t Listener::get_total_device_count() const {
	size_t total = 0;
	for (const auto& connection : connections_) {
		total += connection->get_devices().size();
	}
	return total;
}

std::string Listener::get_device_name(const int device_index) const {
	for (const auto& connection : connections_) {
		const auto& devices = connection->get_devices();
		for (const auto& device : devices) {
			if (device.device_index == device_index) {
				return device.capability.name;
			}
		}
	}
	return ""; // Return an empty string if no device with the given index is found
}

std::string Listener::to_string() const
{
	std::stringstream ss;
	ss << "Listener: ip_address = " << ip_address_ << ", port = " << port_ << ", is_listening = " << (
		is_listening_ ? "true" : "false") << ", connections = [";
	for (const auto& connection : connections_)
	{
		ss << connection->to_string() << ", ";
	}
	std::string str = ss.str();
	if (!connections_.empty())
	{
		// Remove the last comma and space
		str = str.substr(0, str.size() - 2);
	}
	str += "]";
	return str;
}
