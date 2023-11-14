#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Connection.h"

class Listener
{
public:
	Listener(const std::string& ip_address, int port)
		: ip_address_(ip_address), port_(port), is_listening_(false)
	{
	}

	~Listener();

	void start();
	void stop();

	std::thread create_listener_thread();
	bool get_is_listening() const { return is_listening_; }

	Connection* find_connection_with_device(int device_id) const;
	size_t get_total_device_count() const;
	std::string get_device_name(int device_index) const;

	std::string to_string() const;

private:
	std::string ip_address_;
	int port_;
	std::mutex mtx_;
	std::vector<std::shared_ptr<Connection>> connections_;

	bool is_listening_;
	void create_connection(int socket);
	void listener_function();
};
