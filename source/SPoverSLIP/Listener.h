#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "Connection.h"

class Listener
{
public:
	Listener(std::string ip_address, const int port);

	~Listener();

	void start();
	void stop();

	std::thread create_listener_thread();
	bool get_is_listening() const;

	std::pair<uint8_t, std::shared_ptr<Connection>> find_connection_with_device(const uint8_t device_id) const;
	std::vector<std::pair<uint8_t, Connection*>> get_all_connections() const;

	void insert_connection(uint8_t start_id, uint8_t end_id, const std::shared_ptr<Connection>& conn);

	static uint8_t get_total_device_count();

private:
	std::string ip_address_;
	int port_;
	std::mutex mtx_;
	std::thread listening_thread_;
	static uint8_t next_device_id_;
	std::map<std::pair<uint8_t, uint8_t>, std::shared_ptr<Connection>> connection_map_;

	bool is_listening_;
	void create_connection(int socket);
	void listener_function();
};
