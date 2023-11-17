#include "StdAfx.h"

#include "TCPConnection.h"

#include <iostream>
#include <cstring>
#include <thread>

#include "Log.h"
#include "SLIP.h"

void TCPConnection::send_data(const std::vector<uint8_t>& data)
{
	if (data.empty())
	{
		std::cerr << "TCPConnection::send_data No data was given to send" << std::endl;
		return;
	}

	const auto slip_data = SLIP::encode(data);
	send(socket_, reinterpret_cast<const char*>(slip_data.data()), slip_data.size(), 0);
}

void TCPConnection::create_read_channel()
{
	// Start a new thread to listen for incoming data
	reading_thread_ = std::thread([self = shared_from_this()]()
	{
		std::vector<uint8_t> complete_data;
		std::vector<uint8_t> buffer(1024);
		bool is_initialising = true;

		// Set a timeout on the socket
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		setsockopt(self->get_socket(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));

		while (self->is_connected() || is_initialising)
		{
			int valread = 0;
			do
			{
				if (is_initialising)
				{
					is_initialising = false;
					self->set_is_connected(true);
				}

				valread = recv(self->get_socket(), reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
				const int errsv = errno;
				if (valread < 0)
				{
					// timeout is fine, just reloop.
					if (errno == EAGAIN || errno == EWOULDBLOCK || errsv == 0)
					{
						continue;
					}
					// otherwise it was a genuine error.
					std::cerr << "Error in read thread for connection, errno: " << errsv << " = " << strerror(errsv) << std::endl;
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
				std::vector<std::vector<uint8_t>> decoded_packets = SLIP::split_into_packets(complete_data.data(), complete_data.size());
				if (!decoded_packets.empty())
				{
					for (const auto& packet : decoded_packets)
					{
						if (!packet.empty())
						{
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
		LogFileOutput("FujiNet TCPConnection::create_read_channel - thread is EXITING\n");
	});
}
