#include "StdAfx.h"

#include <vector>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include "Connection.h"

int Connection::device_index_ = 0;

const std::vector<Connection::Device>& Connection::get_devices() const
{
	return devices_;
}

const Connection::Device* Connection::find_device(int device_id) const
{
	for (const auto& device : devices_)
	{
		if (device.device_index == device_id)
		{
			return &device;
		}
	}
	return nullptr;
}

// Convert
void Connection::add_devices(const std::vector<uint8_t>& data)
{
	size_t i = 0;
	while (i < data.size())
	{
		// There should be at least one more byte for the unit number
		if (i >= data.size())
		{
			// Handle the error...
			break;
		}

		// Take one byte as the unit number
		uint8_t unit_id = data[i++];

		// Take the remaining bytes as the unit name until \0 is found
		std::string unit_name;
		while (i < data.size() && data[i] != 0)
		{
			unit_name.push_back(static_cast<char>(data[i++]));
		}

		// Skip the null character
		if (i < data.size())
		{
			++i;
		}

		// Add the Device to the devices vector
		devices_.push_back({++device_index_, {unit_id, unit_name}});
	}
}

std::vector<uint8_t> Connection::wait_for_response(int request_id, std::chrono::seconds timeout)
{
	std::unique_lock<std::mutex> lock(responses_mutex_);
	// mutex is unlocked as it goes into a wait, so then the inserting thread can add to map, and this can then pick it up when notified, or timeout.
	if (!response_cv_.wait_for(lock, timeout, [this, request_id]() { return responses_.count(request_id) > 0; }))
	{
		throw std::runtime_error("Timeout waiting for response");
	}
	std::vector<uint8_t> response_data = responses_[request_id];
	responses_.erase(request_id);
	return response_data;
}

std::vector<uint8_t> Connection::wait_for_request()
{
	std::unique_lock<std::mutex> lock(responses_mutex_);
	response_cv_.wait(lock, [this]() { return responses_.size() > 0; });
	auto it = responses_.begin();
	std::vector<uint8_t> request_data = it->second;
	responses_.erase(it);

#ifdef DEBUG
  std::cout << "Connection::wait_for_request, request data:" << std::endl;
  Util::hex_dump(request_data);
#endif

	return request_data;
}

std::string Connection::to_string()
{
	std::stringstream ss;
	ss << "Connection: is_connected = " << (is_connected_ ? "true" : "false") << ", devices = [";
	for (auto it = devices_.begin(); it != devices_.end(); ++it)
	{
		ss << "{device_index = " << it->device_index
			<< ", capability = {unit_id = " << static_cast<unsigned int>(it->capability.unit_id)
			<< ", name = " << it->capability.name << "}}";
		if (std::next(it) != devices_.end())
		{
			ss << ", ";
		}
	}
	ss << "]";
	return ss.str();
}

uint8_t Connection::get_unit_id_by_device_index(int device_index) const
{
	for (const auto& device : devices_)
	{
		if (device.device_index == device_index)
		{
			return device.capability.unit_id;
		}
	}
	throw std::runtime_error("Device not found");
}
