#pragma once

#include "CommandPacket.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

// Forward reference
class Response;

class Request : public CommandPacket
{
public:
	Request(uint8_t request_sequence_number, uint8_t command_number, uint8_t device_id);
	std::vector<uint8_t> serialize() const override = 0;
	virtual std::unique_ptr<Response> deserialize(const std::vector<uint8_t> &data) const = 0;

	uint8_t get_command_number() const;
	uint8_t get_device_id() const;

private:
	uint8_t command_number_ = 0;
	uint8_t device_id_ = 0;
};
