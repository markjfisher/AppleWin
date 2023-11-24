#pragma once
#include "Response.h"

class OpenResponse : public Response
{
public:
	explicit OpenResponse(uint8_t request_sequence_number, uint8_t status);
	std::vector<uint8_t> serialize() const override;
};
