#include "stdafx.h"
#include "ControlRequest.h"

#include "ControlResponse.h"
#include "SmartPortCodes.h"


ControlRequest::ControlRequest(const uint8_t request_sequence_number, const uint8_t sp_unit, const uint8_t control_code, std::vector<uint8_t>& data)
	: control_code_(control_code), data_(std::move(data))
{
	set_request_sequence_number(request_sequence_number);
	set_command_number(SP_CONTROL);
	set_sp_unit(sp_unit);
}

std::vector<uint8_t> ControlRequest::serialize() const
{
	std::vector<uint8_t> request_data;
	request_data.push_back(this->get_request_sequence_number());
	request_data.push_back(SP_STATUS);
	request_data.push_back(this->get_sp_unit());
	request_data.push_back(this->get_control_code());
	request_data.insert(request_data.end(), get_data().begin(), get_data().end());
	return request_data;
}

std::unique_ptr<Response> ControlRequest::deserialize(const std::vector<uint8_t>& data) const
{
	if (data.size() < 2)
	{
		throw std::runtime_error("Not enough data to deserialize ControlResponse");
	}

	// There is only a status returned
	auto response = std::make_unique<ControlResponse>();
	response->set_request_sequence_number(data[0]);
	response->set_status(data[1]);

	return response;
}
