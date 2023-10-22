#pragma once
#include "Response.h"

class ControlResponse : public Response
{
public:
	explicit ControlResponse(uint8_t request_sequence_number, uint8_t status);
	std::vector<uint8_t> serialize() const override;
};
