#include "StdAfx.h"

#include "StatusResponse.h"
#include "StatusRequest.h"

std::vector<uint8_t> StatusResponse::serialize() const
{
	std::vector<uint8_t> data;
	data.push_back(this->get_request_sequence_number());
	data.push_back(this->get_status());

	for (uint8_t b : get_data())
	{
		data.push_back(b);
	}
	return data;
}
