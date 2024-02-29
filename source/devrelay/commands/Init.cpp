#include "Init.h"

InitRequest::InitRequest(const uint8_t request_sequence_number, const uint8_t device_id) : Request(request_sequence_number, CMD_INIT, device_id) {}

std::vector<uint8_t> InitRequest::serialize() const
{
	std::vector<uint8_t> request_data;
	request_data.push_back(this->get_request_sequence_number());
	request_data.push_back(this->get_command_number());
	request_data.push_back(this->get_device_id());
	return request_data;
}

std::unique_ptr<Response> InitRequest::deserialize(const std::vector<uint8_t> &data) const
{
	if (data.size() < 2)
	{
		throw std::runtime_error("Not enough data to deserialize InitResponse");
	}

	auto response = std::make_unique<InitResponse>(data[0], data[1]);
	return response;
}

InitResponse::InitResponse(const uint8_t request_sequence_number, const uint8_t status) : Response(request_sequence_number, status) {}

std::vector<uint8_t> InitResponse::serialize() const
{
	std::vector<uint8_t> data;
	data.push_back(this->get_request_sequence_number());
	data.push_back(this->get_status());
	return data;
}