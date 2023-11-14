#include "StdAfx.h"

#include "TCPConnection.h"

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include "SLIP.h"
#include "Util.h"

void TCPConnection::send_data(const std::vector<uint8_t>& data)
{
#ifdef DEBUG
  std::cout << "TCPConnection::send_data, sending data:" << std::endl;
  Util::hex_dump(data);
#endif

	if (data.empty())
	{
		std::cerr << "TCPConnection::send_data No data was given to send" << std::endl;
		return;
	}

	auto slip_data = SLIP::encode(data);
#ifdef DEBUG
  std::cout << "TCPConnection::send_data sending:" << std::endl;
  Util::hex_dump(slip_data);
#endif

	send(socket_, reinterpret_cast<const char*>(slip_data.data()), slip_data.size(), 0);
}

void TCPConnection::create_read_channel()
{
	// Start a new thread to listen for incoming data
	std::thread reading_thread([self = shared_from_this()]()
	{
		std::vector<uint8_t> complete_data;
		std::vector<uint8_t> buffer(1024);

		// Set a timeout on the socket
		struct timeval timeout;
		timeout.tv_sec = 10; // 10 second
		timeout.tv_usec = 0;
		setsockopt(self->get_socket(), SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

		while (self->is_connected())
		{
			int valread = 0;
			do
			{
				valread = recv(self->get_socket(), reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
				int errsv = errno;
				if (valread < 0)
				{
					// timeout is fine, just reloop.
					if (errno == EAGAIN || errno == EWOULDBLOCK || errsv == 0)
					{
						continue;
					}
					// otherwise it was a genuine error.
					std::cerr << "Error in read thread for connection, errno: " << errsv << " = " << strerror(errsv) <<
						std::endl;
					self->set_is_connected(false);
				}
				if (valread == 0)
				{
					// disconnected, close connection, should remove it too: TODO
					self->set_is_connected(false);
				}
				if (valread > 0)
				{
					complete_data.insert(complete_data.end(), buffer.begin(), buffer.begin() + valread);
				}
			}
			while (valread == 1024);

			if (!complete_data.empty())
			{
				std::vector<std::vector<uint8_t>> decoded_packets = SLIP::split_into_packets(
					complete_data.data(), complete_data.size());
				if (!decoded_packets.empty())
				{
					for (const auto& packet : decoded_packets)
					{
						if (!packet.empty())
						{
#ifdef DEBUG
              std::cout << "putting packet in responses map for request id " << static_cast<unsigned int>(packet[0]) << std::endl;
#endif
							{
								std::lock_guard<std::mutex> lock(self->responses_mutex_);
								self->responses_[packet[0]] = packet;
							}
							self->response_cv_.notify_all();
						}
					}
				}
				complete_data.clear();
			}
		}
	});
	reading_thread.detach();
}

std::string TCPConnection::to_string()
{
	std::stringstream ss;
	ss << Connection::to_string(); // Include the output of the base class
	ss << ", TCPConnection specific info: {";
	ss << "socket = " << socket_;
	ss << "}";
	return ss.str();
}
