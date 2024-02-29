#pragma once

#include <cstdint>
#include <vector>

#include "../types/Request.h"
#include "../types/Response.h"

class FormatRequest : public Request
{
public:
	FormatRequest(const uint8_t request_sequence_number, const uint8_t device_id);
	std::vector<uint8_t> serialize() const override;
	std::unique_ptr<Response> deserialize(const std::vector<uint8_t> &data) const override;
};

class FormatResponse : public Response
{
public:
	explicit FormatResponse(uint8_t request_sequence_number, uint8_t status);
	std::vector<uint8_t> serialize() const override;
};
