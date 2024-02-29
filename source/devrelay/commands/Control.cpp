#include "Control.h"

ControlRequest::ControlRequest(const uint8_t request_sequence_number, const uint8_t device_id, const uint8_t control_code, std::vector<uint8_t> &data)
	: Request(request_sequence_number, CMD_CONTROL, device_id), control_code_(control_code), data_(std::move(data))
{
}

std::vector<uint8_t> ControlRequest::serialize() const
{
	std::vector<uint8_t> request_data;
	request_data.push_back(this->get_request_sequence_number());
	request_data.push_back(this->get_command_number());
	request_data.push_back(this->get_device_id());
	request_data.push_back(this->get_control_code());
	request_data.insert(request_data.end(), get_data().begin(), get_data().end());
	return request_data;
}

std::unique_ptr<Response> ControlRequest::deserialize(const std::vector<uint8_t> &data) const
{
	if (data.size() < 2)
	{
		throw std::runtime_error("Not enough data to deserialize ControlResponse");
	}

	auto response = std::make_unique<ControlResponse>(data[0], data[1]);
	return response;
}

ControlResponse::ControlResponse(const uint8_t request_sequence_number, const uint8_t status) : Response(request_sequence_number, status) {}

std::vector<uint8_t> ControlResponse::serialize() const
{
	std::vector<uint8_t> data;
	data.push_back(this->get_request_sequence_number());
	data.push_back(this->get_status());
	return data;
}