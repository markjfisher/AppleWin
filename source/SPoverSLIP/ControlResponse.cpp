#include "stdafx.h"
#include "ControlResponse.h"

std::vector<uint8_t> ControlResponse::serialize() const
{
	std::vector<uint8_t> data;
	data.push_back(this->get_request_sequence_number());
	data.push_back(this->get_status());
	return data;
}