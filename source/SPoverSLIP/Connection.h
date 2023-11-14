#pragma once

#include <vector>
#include <map>
#include <cstdint>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <sstream>

class Connection
{
public:
	struct SmartPortCapability
	{
		uint8_t unit_id;
		std::string name;
	};

	struct Device
	{
		int device_index;
		SmartPortCapability capability;
	};

	virtual ~Connection() = default;
	virtual void send_data(const std::vector<uint8_t>& data) = 0;

	virtual void create_read_channel() = 0;

	void add_devices(const std::vector<uint8_t>& data);
	const std::vector<Device>& get_devices() const;
	const Device* find_device(int device_id) const;

	bool is_connected() { return is_connected_; }
	void set_is_connected(bool is_connected) { is_connected_ = is_connected; }

	std::vector<uint8_t> wait_for_response(int request_id, std::chrono::seconds timeout);
	std::vector<uint8_t> wait_for_request();

	uint8_t get_unit_id_by_device_index(int device_index) const;

	virtual std::string to_string();

private:
	bool is_connected_ = false;
	std::vector<Device> devices_;
	static int device_index_;

protected:
	std::map<uint8_t, std::vector<uint8_t>> responses_;

	std::mutex responses_mutex_;
	std::condition_variable response_cv_;
};
