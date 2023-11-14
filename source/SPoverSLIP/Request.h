#pragma once

#include <vector>
#include <memory>
#include <sstream>
#include <stdint.h>
#include "Packet.h"
#include "SPoSLIP.h"

// Forward reference to break circular dependency
class Response;

class Request : public Packet
{
public:
	virtual std::vector<uint8_t> serialize() const override = 0;
	virtual std::unique_ptr<Response> deserialize(const std::vector<uint8_t>& data) const = 0;

	uint8_t get_command_number() const { return command_number_; }
	void set_command_number(uint8_t command_number) { command_number_ = command_number; }

	uint8_t get_sp_unit() const { return sp_unit_; }
	void set_sp_unit(uint8_t sp_unit) { sp_unit_ = sp_unit; }

	virtual std::string to_string() const
	{
		std::stringstream ss;
		ss << "Request: {";
		ss << "seq_number = " << static_cast<unsigned int>(get_request_sequence_number());
		ss << ", command_number = " << static_cast<unsigned int>(command_number_);
		ss << ", sp_unit = " << static_cast<unsigned int>(sp_unit_);
		ss << "}";
		return ss.str();
	}

	friend std::ostream& operator<<(std::ostream& os, const Request& request)
	{
		os << request.to_string();
		return os;
	}

private:
	uint8_t command_number_ = 0;
	uint8_t sp_unit_ = 0;
};
