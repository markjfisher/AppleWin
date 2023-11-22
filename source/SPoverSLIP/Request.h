#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include "Packet.h"

// Forward reference to break circular dependency
class Response;

class Request : public Packet
{
public:
	Request(uint8_t request_sequence_number, uint8_t command_number, uint8_t sp_unit);
	std::vector<uint8_t> serialize() const override = 0;
	virtual std::unique_ptr<Response> deserialize(const std::vector<uint8_t>& data) const = 0;

	uint8_t get_command_number() const;
	uint8_t get_sp_unit() const;

private:
	uint8_t command_number_ = 0;
	uint8_t sp_unit_ = 0;
};
