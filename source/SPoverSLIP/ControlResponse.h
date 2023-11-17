#pragma once
#include "Response.h"

class ControlResponse : public Response
{
public:
	std::vector<uint8_t> serialize() const override;
};
