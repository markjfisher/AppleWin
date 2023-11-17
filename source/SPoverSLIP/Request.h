#pragma once

#include <vector>
#include <memory>
#include <stdint.h>
#include "Packet.h"

// Forward reference to break circular dependency
class Response;

class Request : public Packet
{
public:
	std::vector<uint8_t> serialize() const override = 0;
	virtual std::unique_ptr<Response> deserialize(const std::vector<uint8_t>& data) const = 0;

	uint8_t get_command_number() const { return command_number_; }
	void set_command_number(uint8_t command_number) { command_number_ = command_number; }

	uint8_t get_sp_unit() const { return sp_unit_; }
	void set_sp_unit(uint8_t sp_unit) { sp_unit_ = sp_unit; }

private:
	uint8_t command_number_ = 0;
	uint8_t sp_unit_ = 0;
};
