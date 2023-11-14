#include "StdAfx.h"

#include <iostream>
#include <stdexcept>

#include "StatusRequest.h"
#include "StatusResponse.h"
#include "SmartPortHandler.h"
#include "Util.h"

StatusRequest::StatusRequest(uint8_t request_sequence_number, uint8_t sp_unit, uint8_t status_code)
	: status_code_(status_code)
{
	set_request_sequence_number(request_sequence_number);
	set_command_number(SP_STATUS);
	set_sp_unit(sp_unit);
}

std::vector<uint8_t> StatusRequest::serialize() const
{
	std::vector<uint8_t> data;
	data.push_back(this->get_request_sequence_number());
	data.push_back(SP_STATUS);
	data.push_back(this->get_sp_unit());
	data.push_back(this->get_status_code());

#ifdef DEBUG
  std::cout << "Serialised version of StatusRequest:" << std::endl;
  Util::hex_dump(data);
#endif

	return data;
}

// This is for the requestor to easily create the correct response type from the Responder's return value.
std::unique_ptr<Response> StatusRequest::deserialize(const std::vector<uint8_t>& data) const
{
	if (data.size() < 2)
	{
		throw std::runtime_error("Not enough data to deserialize StatusRequest");
	}

	std::unique_ptr<StatusResponse> response = std::make_unique<StatusResponse>();
	response->set_request_sequence_number(data[0]);
	response->set_status(data[1]);

	if (response->get_status() == 0 && data.size() > 2)
	{
		for (size_t i = 2; i < data.size(); ++i)
		{
			response->add_status_value(data[i]);
		}
	}

	return response;
}
